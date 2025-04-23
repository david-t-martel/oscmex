#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mmsystem.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32
typedef SSIZE_T ssize_t;
#endif
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <process.h>
#include "oscmix.h"
#include "arg.h"
#include "socket.h"
#include "util.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")

extern int dflag;
static int lflag;
static SOCKET rfd, wfd;
static HMIDIIN hMidiIn;
static HMIDIOUT hMidiOut;

static void usage(void)
{
    fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr] [-R port] [-S port]\n");
    exit(1);
}

// MIDI callback function for Windows
static void CALLBACK midiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
                                DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    static unsigned char sysexBuffer[8192];
    static size_t sysexPos = 0;
    uint_least32_t payload[sizeof(sysexBuffer) / 4];

    if (wMsg == MIM_DATA)
    {
        // Handle short MIDI messages
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

static unsigned __stdcall midiread(void *arg)
{
    unsigned char data[8192], *datapos, *dataend, *nextpos;
    uint_least32_t payload[sizeof data / 4];
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

static unsigned __stdcall oscread(void *arg)
{
    SOCKET fd = *(SOCKET *)arg;
    ssize_t ret;
    unsigned char buf[8192];

    for (;;)
    {
        ret = recv(fd, buf, sizeof buf, 0);
        if (ret < 0)
        {
            perror("recv");
            break;
        }
        handleosc(buf, ret);
    }
    return 0;
}

void writemidi(const void *buf, size_t len)
{
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
        memcpy(&message, pos, min(len, 4)); // Copy up to 4 bytes

        mmResult = midiOutShortMsg(hMidiOut, message);
        if (mmResult != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiOutShortMsg failed: %d\n", mmResult);
        }
    }
}

void writeosc(const void *buf, size_t len)
{
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
}

// Open a MIDI device by name
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

int main(int argc, char *argv[])
{
    static char defrecvaddr[] = "127.0.0.1";
    static char defsendaddr[] = "127.0.0.1";
    static char mcastaddr[] = "224.0.0.1";
    static const unsigned char refreshosc[] = "/refresh\0\0\0\0,\0\0\0";
    MMRESULT mmResult;
    UINT midiDeviceId;
    char *recvaddr, *sendaddr;
    char recvport[6] = "7222";
    char sendport[6] = "8222";
    HANDLE midireader, oscreader;
    const char *port;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fatal("WSAStartup failed");
    }

    recvaddr = defrecvaddr;
    sendaddr = defsendaddr;
    port = NULL;

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
    case 'R':
        strncpy(recvport, EARGF(usage()), sizeof(recvport) - 1);
        recvport[sizeof(recvport) - 1] = '\0';
        break;
    case 'S':
        strncpy(sendport, EARGF(usage()), sizeof(sendport) - 1);
        sendport[sizeof(sendport) - 1] = '\0';
        break;
    case 'p':
        port = EARGF(usage());
        break;
    default:
        usage();
        break;
    }
    ARGEND

    char recvaddr_with_port[256];
    snprintf(recvaddr_with_port, sizeof(recvaddr_with_port), "udp!%s!%s", recvaddr, recvport);

    char sendaddr_with_port[256];
    snprintf(sendaddr_with_port, sizeof(sendaddr_with_port), "udp!%s!%s", sendaddr, sendport);

    rfd = sockopen(recvaddr_with_port, 1);
    wfd = sockopen(sendaddr_with_port, 0);

    if (!port)
    {
        port = getenv("MIDIPORT");
        if (!port)
            fatal("device is not specified; pass -p or set MIDIPORT");
    }

    // Open MIDI devices
    if (openMidiDevice(port, &midiDeviceId) != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "Could not find MIDI device: %s\n", port);
        return 1;
    }

    mmResult = midiInOpen(&hMidiIn, midiDeviceId, (DWORD_PTR)midiInProc, 0, CALLBACK_FUNCTION);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed: %d\n", mmResult);
        return 1;
    }

    mmResult = midiOutOpen(&hMidiOut, midiDeviceId, 0, 0, 0);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutOpen failed: %d\n", mmResult);
        midiInClose(hMidiIn);
        return 1;
    }

    // Start MIDI input
    mmResult = midiInStart(hMidiIn);
    if (mmResult != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInStart failed: %d\n", mmResult);
        midiOutClose(hMidiOut);
        midiInClose(hMidiIn);
        return 1;
    }

    if (init(port) != 0)
    {
        midiInStop(hMidiIn);
        midiOutClose(hMidiOut);
        midiInClose(hMidiIn);
        return 1;
    }

    midireader = (HANDLE)_beginthreadex(NULL, 0, midiread, &wfd, 0, NULL);
    if (midireader == NULL)
        fatal("CreateThread failed");

    oscreader = (HANDLE)_beginthreadex(NULL, 0, oscread, &rfd, 0, NULL);
    if (oscreader == NULL)
        fatal("CreateThread failed");

    handleosc(refreshosc, sizeof refreshosc - 1);
    for (;;)
    {
        Sleep(100);
        handletimer(lflag == 0);
    }

    // Cleanup (never reached in this implementation)
    midiInStop(hMidiIn);
    midiOutClose(hMidiOut);
    midiInClose(hMidiIn);
    WSACleanup();
    return 0;
}
