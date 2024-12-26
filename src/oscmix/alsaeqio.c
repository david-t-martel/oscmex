#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef SSIZE_T ssize_t;
#include <process.h>
#include "arg.h"

#define LEN(a) (sizeof(a) / sizeof *(a))

static HMIDIIN hMidiIn;
static HMIDIOUT hMidiOut;
static HANDLE hThread;
static HANDLE hEvent;

static void usage(void)
{
    fprintf(stderr, "usage: alsaseq client:port cmd [arg...]\n");
    exit(1);
}

static void CALLBACK midiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    if (wMsg == MIM_DATA)
    {
        SetEvent(hEvent);
    }
}

static unsigned __stdcall midiread(void *arg)
{
    int fd = *(int *)arg;
    DWORD bytesRead;
    unsigned char buf[1024];

    for (;;)
    {
        WaitForSingleObject(hEvent, INFINITE);
        bytesRead = midiInGetMessage(hMidiIn, buf, sizeof(buf));
        if (bytesRead == 0)
        {
            continue;
        }
        if (write(fd, buf, bytesRead) < 0)
        {
            perror("write");
            exit(1);
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int err;
    ssize_t ret;
    size_t len;
    int pid;
    char *end;
    int rfd[2], wfd[2];
    unsigned char *pos, buf[1024];

    ARGBEGIN
    {
    default:
        usage();
    }
    ARGEND

    if (argc < 2)
        usage();

    if (pipe(wfd) != 0)
    {
        perror("pipe");
        return 1;
    }
    if (pipe(rfd) != 0)
    {
        perror("pipe");
        return 1;
    }
    pid = fork();
    switch (pid)
    {
    case -1:
        perror("fork");
        return 1;
    case 0:
        close(rfd[1]);
        close(wfd[0]);
        break;
    default:
        close(wfd[1]);
        close(rfd[0]);
        if (dup2(wfd[0], 6) < 0 || dup2(rfd[1], 7) < 0)
        {
            perror("dup2");
            return 1;
        }
        execvp(argv[1], argv + 1);
        fprintf(stderr, "execvp %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hEvent == NULL)
    {
        fprintf(stderr, "CreateEvent failed: %d\n", GetLastError());
        return 1;
    }

    err = midiInOpen(&hMidiIn, 0, (DWORD_PTR)midiInProc, 0, CALLBACK_FUNCTION);
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed: %d\n", err);
        return 1;
    }

    err = midiOutOpen(&hMidiOut, 0, 0, 0, CALLBACK_NULL);
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutOpen failed: %d\n", err);
        return 1;
    }

    hThread = (HANDLE)_beginthreadex(NULL, 0, midiread, &wfd[1], 0, NULL);
    if (hThread == NULL)
    {
        fprintf(stderr, "CreateThread failed: %d\n", GetLastError());
        return 1;
    }

    midiInStart(hMidiIn);

    for (;;)
    {
        ret = read(rfd[0], buf, sizeof(buf));
        if (ret < 0)
        {
            perror("read");
            exit(1);
        }
        if (ret == 0)
            break;
        pos = buf;
        len = ret;
        while (len > 0)
        {
            ret = midiOutShortMsg(hMidiOut, *(DWORD *)pos);
            if (ret != MMSYSERR_NOERROR)
            {
                fprintf(stderr, "midiOutShortMsg failed: %d\n", ret);
                return 1;
            }
            pos += sizeof(DWORD);
            len -= sizeof(DWORD);
        }
    }

    midiInStop(hMidiIn);
    midiInClose(hMidiIn);
    midiOutClose(hMidiOut);
    CloseHandle(hEvent);
    CloseHandle(hThread);

    return 0;
}