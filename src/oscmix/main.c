#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
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

extern int dflag;
static int lflag;
static SOCKET rfd, wfd;
static HANDLE hMidiIn, hMidiOut;

static void usage(void)
{
    fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr] [-R port] [-S port]\n");
    exit(1);
}

static unsigned __stdcall midiread(void *arg)
{
    unsigned char data[8192], *datapos, *dataend, *nextpos;
    uint_least32_t payload[sizeof data / 4];
    DWORD bytesRead;

    dataend = data;
    for (;;)
    {
        bytesRead = midiInGetMessage(hMidiIn, dataend, sizeof(data) - (dataend - data));
        if (bytesRead == 0)
        {
            fprintf(stderr, "midiInGetMessage failed\n");
            exit(1);
        }
        dataend += bytesRead;
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
    DWORD bytesWritten;

    while (len > 0)
    {
        bytesWritten = midiOutShortMsg(hMidiOut, *(DWORD *)pos);
        if (bytesWritten != MMSYSERR_NOERROR)
        {
            fprintf(stderr, "midiOutShortMsg failed\n");
            exit(1);
        }
        pos += sizeof(DWORD);
        len -= sizeof(DWORD);
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

int main(int argc, char *argv[])
{
    static char defrecvaddr[] = "udp!127.0.0.1!7222";
    static char defsendaddr[] = "udp!127.0.0.1!8222";
    static char mcastaddr[] = "udp!224.0.0.1!8222";
    static const unsigned char refreshosc[] = "/refresh\0\0\0\0,\0\0\0";
    int err, sig;
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
    if (init(port) != 0)
        return 1;

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

    WSACleanup();
    return 0;
}