/**
 * @file main.c
 * @brief Cross-platform main entry point for the OSCMix application
 *
 * This file implements the unified main application logic and platform-specific code
 * for running OSCMix across Windows, macOS, and Linux systems. It provides:
 * - Command line argument parsing
 * - Socket initialization for OSC communication
 * - Platform-specific MIDI device connection
 * - Threading for MIDI and OSC message handling
 * - Event and signal handling
 * - Graceful cleanup on exit
 *
 * The code uses conditional compilation to support multiple platforms while
 * maintaining a consistent interface to the core OSCMix logic.
 */

/* Platform-specific includes */
#include "platform.h"

#ifdef PLATFORM_WINDOWS
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#endif

// Rename these for backward compatibility with existing code
typedef platform_thread_t thread_t;
typedef platform_socket_t socket_t;
#define thread_create platform_thread_create
#define thread_join platform_thread_join
#define sleep_ms platform_sleep_ms

/* Common includes */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Project includes */
#include "socket.h"
#include "oscmix.h"
#include "arg.h"
#include "util.h"

/* Global variables */
extern int dflag;    /**< Debug flag: enables verbose logging when set */
static int lflag;    /**< Level meters flag: disables level meters when set */
static socket_t rfd; /**< Socket file descriptor for receiving OSC messages */
static socket_t wfd; /**< Socket file descriptor for sending OSC messages */

#ifdef _WIN32
static HMIDIIN hMidiIn;          /**< Windows MIDI input device handle */
static HMIDIOUT hMidiOut;        /**< Windows MIDI output device handle */
static HANDLE hEvent;            /**< Windows event handle for MIDI notification */
static volatile int running = 1; /**< Flag to control thread execution */
#endif

/* Default addresses for OSC communication */
static char defrecvaddr[] = "udp!127.0.0.1!7222";                    /**< Default address for receiving OSC messages */
static char defsendaddr[] = "udp!127.0.0.1!8222";                    /**< Default address for sending OSC messages */
static char mcastaddr[] = "udp!224.0.0.1!8222";                      /**< Multicast address for sending OSC messages */
static const unsigned char refreshosc[] = "/refresh\0\0\0\0,\0\0\0"; /**< OSC message for triggering a refresh */

/**
 * @brief Prints usage information and exits
 */
static void
usage(void)
{
#ifdef _WIN32
    fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr] [-R port] [-S port] [-p midiport]\n");
#else
    fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr] [-p midiport]\n");
#endif
    exit(1);
}

#ifdef _WIN32
/**
 * @brief MIDI callback function for Windows
 *
 * This function is called by the Windows Multimedia API when MIDI data is received.
 * It processes both short MIDI messages and SysEx messages, and dispatches them
 * to the appropriate handlers.
 *
 * @param hMidiIn MIDI input device handle
 * @param wMsg Type of MIDI message (MIM_DATA or MIM_LONGDATA)
 * @param dwInstance Instance data passed to midiInOpen
 * @param dwParam1 First parameter (depends on wMsg)
 * @param dwParam2 Second parameter (depends on wMsg)
 */
static void CALLBACK midiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    static unsigned char sysexBuffer[8192];
    static size_t sysexPos = 0;
    uint_least32_t payload[sizeof(sysexBuffer) / 4];

    if (wMsg == MIM_DATA)
    {
        // Handle short MIDI messages if needed
    }
    else if (wMsg == MIM_LONGDATA)
    {
        /* Handle SysEx messages */
        MIDIHDR *pMidiHdr = (MIDIHDR *)dwParam1;

        if (pMidiHdr->dwBytesRecorded > 0)
        {
            /* Process SysEx data */
            handlesysex(pMidiHdr->lpData, pMidiHdr->dwBytesRecorded, payload);

            /* Prepare the buffer for reuse */
            pMidiHdr->dwBytesRecorded = 0;
            midiInPrepareHeader(hMidiIn, pMidiHdr, sizeof(MIDIHDR));
            midiInAddBuffer(hMidiIn, pMidiHdr, sizeof(MIDIHDR));
        }
    }
}

/**
 * @brief Thread function for reading MIDI messages from the RME device (Windows)
 *
 * Sets up MIDI input buffers and continuously processes MIDI data using
 * the Windows Multimedia API. The actual processing is done in the midiInProc callback.
 *
 * @param arg Thread argument (unused)
 * @return Thread return value (unused)
 */
static unsigned __stdcall midiread(void *arg)
{
    unsigned char data[8192];
    MIDIHDR midiHdr;
    MMRESULT mmResult;

    /* Set up the MIDI input header */
    ZeroMemory(&midiHdr, sizeof(MIDIHDR));
    midiHdr.lpData = data;
    midiHdr.dwBufferLength = sizeof(data);
    midiHdr.dwFlags = 0;

    mmResult = midiInPrepareHeader(hMidiIn, &midiHdr, sizeof(MIDIHDR));
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInPrepareHeader failed: %d\n", mmResult);
        return 1;
    }

    mmResult = midiInAddBuffer(hMidiIn, &midiHdr, sizeof(MIDIHDR));
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInAddBuffer failed: %d\n", mmResult);
        return 1;
    }

    /* Main loop to wait for messages */
    while (running)
    {
        Sleep(10); /* Short sleep to reduce CPU usage */
    }

    return 0;
}
#else
/**
 * @brief Thread function for reading MIDI messages from the RME device (POSIX)
 *
 * Continuously reads MIDI SysEx messages from the device via file descriptor 6,
 * processes them using handlesysex(), and updates application state.
 *
 * @param arg Thread argument (unused)
 * @return Thread return value (unused)
 */
static void *midiread(void *arg)
{
    unsigned char data[8192], *datapos, *dataend, *nextpos;
    uint_least32_t payload[sizeof data / 4];
    ssize_t ret;

    dataend = data;
    for (;;)
    {
        ret = read(6, dataend, (data + sizeof data) - dataend);
        if (ret < 0)
            fatal("read 6:");
        dataend += ret;
        datapos = data;
        for (;;)
        {
            assert(datapos <= dataend);
            datapos = memchr(datapos, 0xf0, dataend - datapos);
            if (!datapos)
            {
                dataend = data;
                break;
            }
            nextpos = memchr(datapos + 1, 0xf7, dataend - datapos - 1);
            if (!nextpos)
            {
                if (dataend == data + sizeof data)
                {
                    fprintf(stderr, "sysex packet too large; dropping\n");
                    dataend = data;
                }
                else
                {
                    memmove(data, datapos, dataend - datapos);
                    dataend -= datapos - data;
                }
                break;
            }
            ++nextpos;
            handlesysex(datapos, nextpos - datapos, payload);
            datapos = nextpos;
        }
    }
    return NULL;
}
#endif

#ifdef _WIN32
/**
 * @brief Thread function for reading OSC messages from the network (Windows)
 *
 * @param arg Pointer to the socket file descriptor
 * @return Thread return value (unused)
 */
static unsigned __stdcall oscread(void *arg)
{
    socket_t fd = *(socket_t *)arg;
    ssize_t ret;
    unsigned char buf[8192];

    while (running)
    {
        ret = recv(fd, buf, sizeof buf, 0);
        if (ret < 0)
        {
            if (WSAGetLastError() != WSAECONNRESET)
                perror("recv");
            continue;
        }
        handleosc(buf, ret);
    }
    return 0;
}
#else
/**
 * @brief Thread function for reading OSC messages from the network (POSIX)
 *
 * Continuously reads OSC messages from the network socket,
 * and dispatches them to handleosc() for processing.
 *
 * @param arg Pointer to the socket file descriptor
 * @return Thread return value (unused)
 */
static void *oscread(void *arg)
{
    socket_t fd = *(int *)arg;
    ssize_t ret;
    unsigned char buf[8192];

    for (;;)
    {
        ret = read(fd, buf, sizeof buf);
        if (ret < 0)
        {
            perror("recv");
            continue;
        }
        handleosc(buf, ret);
    }
    return NULL;
}
#endif

/**
 * @brief Sends MIDI data to the RME device
 *
 * Platform-specific implementation to send MIDI data to the connected device.
 * On Windows, uses the Multimedia API. On POSIX systems, writes to file descriptor 7.
 *
 * @param buf Pointer to the MIDI data to send
 * @param len Length of the MIDI data in bytes
 */
void writemidi(const void *buf, size_t len)
{
#ifdef _WIN32
    const unsigned char *pos = buf;
    MMRESULT mmResult;

    if (buf == NULL || len == 0)
    {
        return;
    }

    /* If it's a SysEx message (starts with 0xF0) */
    if (pos[0] == 0xF0)
    {
        MIDIHDR midiHdr;

        /* Set up the MIDI output header */
        ZeroMemory(&midiHdr, sizeof(MIDIHDR));
        midiHdr.lpData = (LPSTR)buf;
        midiHdr.dwBufferLength = len;
        midiHdr.dwFlags = 0;

        mmResult = midiOutPrepareHeader(hMidiOut, &midiHdr, sizeof(MIDIHDR));
        if (mmResult != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiOutPrepareHeader failed: %d\n", mmResult);
            return;
        }

        mmResult = midiOutLongMsg(hMidiOut, &midiHdr, sizeof(MIDIHDR));
        if (mmResult != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiOutLongMsg failed: %d\n", mmResult);
        }

        /* Wait for the message to be sent */
        while (!(midiHdr.dwFlags & MHDR_DONE))
        {
            Sleep(1);
        }

        mmResult = midiOutUnprepareHeader(hMidiOut, &midiHdr, sizeof(MIDIHDR));
        if (mmResult != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiOutUnprepareHeader failed: %d\n", mmResult);
        }
    }
    else
    {
        /* Short MIDI message */
        DWORD message = 0;
        memcpy(&message, pos, min(len, 4)); /* Copy up to 4 bytes */

        mmResult = midiOutShortMsg(hMidiOut, message);
        if (mmResult != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiOutShortMsg failed: %d\n", mmResult);
        }
    }
#else
    const unsigned char *pos;
    ssize_t ret;

    pos = buf;
    while (len > 0)
    {
        ret = write(7, pos, len);
        if (ret < 0)
            fatal("write 7:");
        pos += ret;
        len -= ret;
    }
#endif
}

/**
 * @brief Sends OSC data to the network
 *
 * Writes OSC data to the network socket. This function is called by the oscmix
 * core logic to send OSC responses and notifications to clients.
 *
 * @param buf Pointer to the OSC data to send
 * @param len Length of the OSC data in bytes
 */
void writeosc(const void *buf, size_t len)
{
#ifdef _WIN32
    ssize_t ret;
    ret = send(wfd, buf, len, 0);
    if (ret < 0)
    {
        if (WSAGetLastError() != WSAECONNREFUSED)
            perror("send");
    }
    else if (ret != len)
    {
        fprintf(stderr, "send: %zd != %zu", ret, len);
    }
#else
    ssize_t ret;

    ret = write(wfd, buf, len);
    if (ret < 0)
    {
        if (errno != ECONNREFUSED)
            perror("write");
    }
    else if (ret != len)
    {
        fprintf(stderr, "write: %zd != %zu", ret, len);
    }
#endif
}

#ifdef _WIN32
/**
 * @brief Find and open a MIDI device by name
 *
 * Searches for a MIDI device with a name containing the specified string,
 * or tries to interpret the string as a device ID number.
 *
 * @param name Name or ID of the MIDI device to open
 * @param deviceId Pointer to store the found device ID
 * @return MMSYSERR_NOERROR on success, MMSYSERR_ERROR on failure
 */
static MMRESULT openMidiDevice(const char *name, UINT *deviceId)
{
    UINT numDevs = midiInGetNumDevs();
    MIDIINCAPS caps;
    *deviceId = MIDI_MAPPER; /* Default */

    /* First try to find device by name substring */
    for (UINT i = 0; i < numDevs; i++)
    {
        if (midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS)) == MMSYSERR_NOERROR)
        {
            if (strstr(caps.szPname, name) != NULL)
            {
                *deviceId = i;
                return MMSYSERR_NOERROR;
            }
        }
    }

    /* Not found by name, try as a device ID number */
    char *endptr;
    long num = strtol(name, &endptr, 10);
    if (*name != '\0' && *endptr == '\0' && num >= 0 && num < numDevs)
    {
        *deviceId = (UINT)num;
        return MMSYSERR_NOERROR;
    }

    return MMSYSERR_ERROR;
}

/**
 * @brief Windows control handler for clean shutdown
 *
 * Handles Ctrl+C and Windows service stop events.
 *
 * @param ctrlType The type of control signal received
 * @return TRUE if the event was handled, FALSE otherwise
 */
static BOOL WINAPI controlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        /* Set running flag to false to stop threads */
        running = 0;

        /* Clean up MIDI resources */
        if (hMidiIn != NULL)
        {
            midiInStop(hMidiIn);
            midiInClose(hMidiIn);
        }

        if (hMidiOut != NULL)
        {
            midiOutClose(hMidiOut);
        }

        /* Clean up network resources */
        if (rfd != INVALID_SOCKET)
        {
            closesocket(rfd);
        }

        if (wfd != INVALID_SOCKET)
        {
            closesocket(wfd);
        }

        WSACleanup();
        exit(0);
        return TRUE;
    }

    return FALSE;
}
#else
/**
 * @brief Signal handler for clean shutdown (POSIX)
 *
 * Handles SIGINT, SIGTERM, etc. for clean application shutdown
 *
 * @param sig The signal number that was received
 */
static void signalHandler(int sig)
{
    /* Clean up resources here */
    close(rfd);
    close(wfd);

    fprintf(stderr, "\nReceived signal %d, exiting...\n", sig);
    exit(0);
}
#endif

/**
 * @brief Main entry point for the OSCMix application
 *
 * Parses command line arguments, initializes networking and MIDI connections,
 * creates threads for message handling, and enters the main event loop.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit code (0 on success, non-zero on failure)
 */
int main(int argc, char *argv[])
{
    char *recvaddr, *sendaddr;
    platform_thread_t midireader, oscreader;
    const char *port;

    if (platform_socket_init() != 0)
    {
        fatal("Socket initialization failed");
    }

#ifdef _WIN32
    MMRESULT mmResult;
    UINT midiDeviceId;
    char recvport[6] = "7222"; /**< Default port for receiving OSC messages */
    char sendport[6] = "8222"; /**< Default port for sending OSC messages */

    /* Set up control handler for clean shutdown */
    SetConsoleCtrlHandler(controlHandler, TRUE);
#else
    /* POSIX-specific initialization */
    int err, sig;
    struct itimerval it;
    sigset_t set;

    /* Validate file descriptors for MIDI I/O */
    if (fcntl(6, F_GETFD) < 0)
        fatal("fcntl 6:");
    if (fcntl(7, F_GETFD) < 0)
        fatal("fcntl 7:");

    /* Set up signal handlers */
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#endif

    /* Initialize network and port variables */
    recvaddr = defrecvaddr;
    sendaddr = defsendaddr;
    port = NULL;

    /* Parse command line arguments */
    ARGBEGIN
    {
    case 'd':
        dflag = 1;
        break;
    case 'l':
        lflag = 1;
        break;
    case 'r':
        recvaddr = EARGF(usage());
        break;
    case 's':
        sendaddr = EARGF(usage());
        break;
    case 'm':
        sendaddr = mcastaddr;
        break;
#ifdef _WIN32
    case 'R':
        strncpy(recvport, EARGF(usage()), sizeof(recvport) - 1);
        recvport[sizeof(recvport) - 1] = '\0';
        break;
    case 'S':
        strncpy(sendport, EARGF(usage()), sizeof(sendport) - 1);
        sendport[sizeof(sendport) - 1] = '\0';
        break;
#endif
    case 'p':
        port = EARGF(usage());
        break;
    default:
        usage();
        break;
    }
    ARGEND

#ifdef _WIN32
    /* Format UDP socket addresses with IP and port for Windows */
    char recvaddr_with_port[256];
    snprintf(recvaddr_with_port, sizeof(recvaddr_with_port), "udp!%s!%s",
             strchr(recvaddr, '!') ? recvaddr + 4 : recvaddr,
             recvport);

    char sendaddr_with_port[256];
    snprintf(sendaddr_with_port, sizeof(sendaddr_with_port), "udp!%s!%s",
             strchr(sendaddr, '!') ? sendaddr + 4 : sendaddr,
             sendport);

    /* Open sockets for OSC communication */
    rfd = sockopen(recvaddr_with_port, 1);
    wfd = sockopen(sendaddr_with_port, 0);
#else
    /* Open sockets for OSC communication */
    rfd = sockopen(recvaddr, 1);
    wfd = sockopen(sendaddr, 0);
#endif

    /* Get MIDI device port from argument or environment */
    if (!port)
    {
        port = getenv("MIDIPORT");
        if (!port)
            fatal("device is not specified; pass -p or set MIDIPORT");
    }

#ifdef _WIN32
    /* Open MIDI devices on Windows */
    if (openMidiDevice(port, &midiDeviceId) != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Could not find MIDI device: %s\n", port);
        return 1;
    }

    /* Open MIDI input device with callback function */
    mmResult = midiInOpen(&hMidiIn, midiDeviceId, (DWORD_PTR)midiInProc, 0, CALLBACK_FUNCTION);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed: %d\n", mmResult);
        return 1;
    }

    /* Open MIDI output device */
    mmResult = midiOutOpen(&hMidiOut, midiDeviceId, 0, 0, 0);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutOpen failed: %d\n", mmResult);
        midiInClose(hMidiIn);
        return 1;
    }

    /* Start MIDI input */
    mmResult = midiInStart(hMidiIn);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInStart failed: %d\n", mmResult);
        midiOutClose(hMidiOut);
        midiInClose(hMidiIn);
        return 1;
    }
#endif

    /* Initialize OSCMix core with the specified device */
    if (init(port) != 0)
    {
#ifdef _WIN32
        midiInStop(hMidiIn);
        midiOutClose(hMidiOut);
        midiInClose(hMidiIn);
#endif
        return 1;
    }

#ifdef _WIN32
    /* Create thread for reading MIDI messages (Windows) */
    platform_thread_create(&midireader, midiread, NULL);

    /* Create thread for reading OSC messages (Windows) */
    platform_thread_create(&oscreader, oscread, &rfd);

    /* Send initial refresh command and enter main loop (Windows) */
    handleosc(refreshosc, sizeof refreshosc - 1);
    while (running)
    {
        Sleep(100);
        handletimer(lflag == 0);
    }
#else
    /* Block all signals in main thread */
    sigfillset(&set);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    /* Create thread for reading MIDI messages (POSIX) */
    platform_thread_create(&midireader, midiread, NULL);

    /* Create thread for reading OSC messages (POSIX) */
    platform_thread_create(&oscreader, oscread, &rfd);

    /* Set up real-time timer for periodic level updates */
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 100000; /* 100ms interval */
    it.it_value = it.it_interval;
    if (setitimer(ITIMER_REAL, &it, NULL) != 0)
        fatal("setitimer:");

    /* Send initial refresh command and enter main loop (POSIX) */
    handleosc(refreshosc, sizeof refreshosc - 1);
    for (;;)
    {
        sigwait(&set, &sig);
        handletimer(lflag == 0);
    }
#endif

    /* Cleanup (never reached in normal operation) */
#ifdef _WIN32
    midiInStop(hMidiIn);
    midiOutClose(hMidiOut);
    midiInClose(hMidiIn);
    WSACleanup();
#endif
    return 0;
}
