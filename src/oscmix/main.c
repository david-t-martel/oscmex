/**
 * OSCMix - Cross-platform implementation
 * Unified main file for Windows, macOS and Linux
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mmsystem.h>
#include <process.h>
typedef HANDLE thread_t;
#define thread_create(t, f, a) ((*t = (HANDLE)_beginthreadex(NULL, 0, f, a, 0, NULL)) == 0)
#define thread_join(t) WaitForSingleObject(t, INFINITE)
#define sleep_ms(ms) Sleep(ms)
typedef SOCKET socket_t;
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
static HMIDIIN hMidiIn;
static HMIDIOUT hMidiOut;
#else
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
typedef pthread_t thread_t;
#define thread_create(t, f, a) pthread_create(t, NULL, f, a)
#define thread_join(t) pthread_join(t, NULL)
#define sleep_ms(ms) usleep((ms) * 1000)
typedef int socket_t;
#endif

#include "socket.h"
#include "oscmix.h"
#include "arg.h"
#include "util.h"

extern int dflag;
static int lflag;
static socket_t rfd, wfd;

// Logging macros
#define LOG_ERROR(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
    if (dflag)             \
    fprintf(stderr, "INFO: " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    if (dflag > 1)          \
    fprintf(stderr, "DEBUG: " fmt "\n", ##__VA_ARGS__)

static void usage(void)
{
    fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr] [-R port] [-S port] [-p device]\n");
    exit(1);
}

// Initialize networking based on platform
int init_networking(void)
{
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        LOG_ERROR("WSAStartup failed");
        return -1;
    }
#endif
    return 0;
}

void cleanup_networking(void)
{
#ifdef _WIN32
    WSACleanup();
#endif
}

// Handle signals for clean shutdown
void handle_signal(int sig)
{
    LOG_INFO("Received signal %d, cleaning up...", sig);

#ifdef _WIN32
    if (hMidiIn)
    {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
    }
    if (hMidiOut)
    {
        midiOutClose(hMidiOut);
    }
#endif

    cleanup_networking();
    exit(0);
}

#ifdef _WIN32
// MIDI callback function for Windows
static void CALLBACK midiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    static unsigned char sysexBuffer[8192];
    static size_t sysexPos = 0;
    uint_least32_t payload[sizeof(sysexBuffer) / 4];

    if (wMsg == MIM_DATA)
    {
        // Handle short MIDI messages (if needed)
        DWORD dwMidiMessage = (DWORD)dwParam1;
        // Process if needed
    }
    else if (wMsg == MIM_LONGDATA)
    {
        // Handle SysEx messages
        MIDIHDR *pMidiHdr = (MIDIHDR *)dwParam1;

        if (pMidiHdr->dwBytesRecorded > 0)
        {
            // Process SysEx data
            handlesysex(pMidiHdr->lpData, pMidiHdr->dwBytesRecorded, payload);

            // Prepare the buffer for reuse
            pMidiHdr->dwBytesRecorded = 0;
            midiInPrepareHeader(hMidiIn, pMidiHdr, sizeof(MIDIHDR));
            midiInAddBuffer(hMidiIn, pMidiHdr, sizeof(MIDIHDR));
        }
    }
}

// Open a MIDI device by name on Windows
static MMRESULT openMidiDevice(const char *name, UINT *deviceId)
{
    UINT numDevs = midiInGetNumDevs();
    MIDIINCAPS caps;
    *deviceId = MIDI_MAPPER; // Default

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

    // Not found, try as a device ID number
    char *endptr;
    long num = strtol(name, &endptr, 10);
    if (*name != '\0' && *endptr == '\0' && num >= 0 && num < numDevs)
    {
        *deviceId = (UINT)num;
        return MMSYSERR_NOERROR;
    }

    return MMSYSERR_ERROR;
}

static unsigned __stdcall midiread(void *arg)
{
    unsigned char data[8192];
    MIDIHDR midiHdr;
    MMRESULT mmResult;

    // Set up the MIDI input header
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

    // Main loop to wait for messages
    for (;;)
    {
        Sleep(10); // Short sleep to reduce CPU usage
    }

    return 0;
}
#else
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
    return 0;
}
#endif

#ifdef _WIN32
static unsigned __stdcall oscread(void *arg)
#else
static void *oscread(void *arg)
#endif
{
    socket_t fd = *(socket_t *)arg;
    ssize_t ret;
    unsigned char buf[8192];

    for (;;)
    {
#ifdef _WIN32
        ret = recv(fd, buf, sizeof buf, 0);
#else
        ret = read(fd, buf, sizeof buf);
#endif
        if (ret < 0)
        {
#ifdef _WIN32
            if (WSAGetLastError() != WSAECONNREFUSED)
#else
            if (errno != ECONNREFUSED)
#endif
                perror("recv");
            continue;
        }
        handleosc(buf, ret);
    }
    return 0;
}

void writemidi(const void *buf, size_t len)
{
#ifdef _WIN32
    const unsigned char *pos = buf;
    MMRESULT mmResult;

    if (buf == NULL || len == 0)
    {
        return;
    }

    // If it's a SysEx message (starts with 0xF0)
    if (pos[0] == 0xF0)
    {
        MIDIHDR midiHdr;

        // Set up the MIDI output header
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

        // Wait for the message to be sent
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
        // Short MIDI message
        DWORD message = 0;
        memcpy(&message, pos, len < 4 ? len : 4); // Copy up to 4 bytes

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

void writeosc(const void *buf, size_t len)
{
    ssize_t ret;

#ifdef _WIN32
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

int init_midi(const char *device_name)
{
#ifdef _WIN32
    UINT midiDeviceId;
    MMRESULT mmResult;

    // Open MIDI devices
    if (openMidiDevice(device_name, &midiDeviceId) != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Could not find MIDI device: %s\n", device_name);
        return -1;
    }

    // Open MIDI input device
    mmResult = midiInOpen(&hMidiIn, midiDeviceId, (DWORD_PTR)midiInProc, 0, CALLBACK_FUNCTION);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed: %d\n", mmResult);
        return -1;
    }

    // Open MIDI output device
    mmResult = midiOutOpen(&hMidiOut, midiDeviceId, 0, 0, 0);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutOpen failed: %d\n", mmResult);
        midiInClose(hMidiIn);
        return -1;
    }

    // Start MIDI input
    mmResult = midiInStart(hMidiIn);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInStart failed: %d\n", mmResult);
        midiOutClose(hMidiOut);
        midiInClose(hMidiIn);
        return -1;
    }
#else
    // For POSIX systems, MIDI devices are typically handled through file descriptors
    // which are assumed to be already set up (fd 6 and 7)
    if (fcntl(6, F_GETFD) < 0)
        fatal("fcntl 6:");
    if (fcntl(7, F_GETFD) < 0)
        fatal("fcntl 7:");
#endif

    return 0;
}

void cleanup_midi(void)
{
#ifdef _WIN32
    if (hMidiIn)
    {
        midiInStop(hMidiIn);
        midiInClose(hMidiIn);
    }
    if (hMidiOut)
    {
        midiOutClose(hMidiOut);
    }
#endif
    // Nothing special to do for POSIX
}

int main(int argc, char *argv[])
{
    static char defrecvaddr[] = "udp!127.0.0.1!7222";
    static char defsendaddr[] = "udp!127.0.0.1!8222";
    static char mcastaddr[] = "udp!224.0.0.1!8222";
    static const unsigned char refreshosc[] = "/refresh\0\0\0\0,\0\0\0";
    char *recvaddr, *sendaddr;
    thread_t midireader, oscreader;
    const char *port = NULL;

    // Initialize networking
    if (init_networking() != 0)
    {
        return 1;
    }

    // Set up signal handlers for clean shutdown
#ifdef _WIN32
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)handle_signal, TRUE);
#else
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif

    recvaddr = defrecvaddr;
    sendaddr = defsendaddr;

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
    case 'p':
        port = EARGF(usage());
        break;
    default:
        usage();
        break;
    }
    ARGEND

    // Open sockets
    rfd = sockopen(recvaddr, 1);
    wfd = sockopen(sendaddr, 0);

    // Check for MIDI device name
    if (!port)
    {
        port = getenv("MIDIPORT");
        if (!port)
            fatal("device is not specified; pass -p or set MIDIPORT");
    }

    // Initialize MIDI
    if (init_midi(port) != 0)
    {
        cleanup_networking();
        return 1;
    }

    // Initialize the application
    if (init(port) != 0)
    {
        cleanup_midi();
        cleanup_networking();
        return 1;
    }

    // Create threads
    thread_create(&midireader, midiread, &wfd);
    thread_create(&oscreader, oscread, &rfd);

    // Send initial refresh command
    handleosc(refreshosc, sizeof refreshosc - 1);

    // Main loop
#ifdef _WIN32
    for (;;)
    {
        sleep_ms(100);
        handletimer(lflag == 0);
    }
#else
    sigset_t set;
    int sig;
    struct itimerval it;

    // Set up timer for periodic updates
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_SETMASK, &set, NULL);
    it.it_interval.tv_sec = 0;
    it.it_interval.tv_usec = 100000;
    it.it_value = it.it_interval;
    if (setitimer(ITIMER_REAL, &it, NULL) != 0)
        fatal("setitimer:");

    for (;;)
    {
        sigwait(&set, &sig);
        handletimer(lflag == 0);
    }
#endif

    // This point is never reached, but good practice
    cleanup_midi();
    cleanup_networking();
    return 0;
}
