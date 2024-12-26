#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <process.h>
#include "arg.h"

static void usage(void)
{
    fprintf(stderr, "usage: midirawio cmd [arg...]\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    HMIDIIN hMidiIn;
    HMIDIOUT hMidiOut;
    int err;
    UINT numDevs;
    char *arg, *end;
    DWORD_PTR midiInId, midiOutId;
    MIDIOUTCAPS midiOutCaps;
    MIDIINCAPS midiInCaps;

    ARGBEGIN
    {
    default:
        usage();
    }
    ARGEND

    if (argc < 2)
        usage();

    arg = argv[0];
    midiInId = strtol(arg, &end, 10);
    if (midiInId < 0 || midiInId > UINT_MAX || !*arg || (*end && *end != ','))
    {
        usage();
    }
    midiOutId = 0;
    if (*end == ',')
    {
        arg = end + 1;
        midiOutId = strtol(arg, &end, 10);
        if (midiOutId < 0 || midiOutId > UINT_MAX || !*arg || *end)
        {
            usage();
        }
    }

    numDevs = midiInGetNumDevs();
    if (midiInId >= numDevs)
    {
        fprintf(stderr, "Invalid MIDI input device ID\n");
        return 1;
    }

    numDevs = midiOutGetNumDevs();
    if (midiOutId >= numDevs)
    {
        fprintf(stderr, "Invalid MIDI output device ID\n");
        return 1;
    }

    err = midiInOpen(&hMidiIn, midiInId, 0, 0, CALLBACK_NULL);
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed: %d\n", err);
        return 1;
    }

    err = midiOutOpen(&hMidiOut, midiOutId, 0, 0, CALLBACK_NULL);
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutOpen failed: %d\n", err);
        return 1;
    }

    err = midiInGetDevCaps(midiInId, &midiInCaps, sizeof(MIDIINCAPS));
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInGetDevCaps failed: %d\n", err);
        return 1;
    }
    setenv("MIDIPORT", midiInCaps.szPname, 1);

    err = midiOutGetDevCaps(midiOutId, &midiOutCaps, sizeof(MIDIOUTCAPS));
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutGetDevCaps failed: %d\n", err);
        return 1;
    }
    setenv("MIDIPORT", midiOutCaps.szPname, 1);

    if (dup2((int)hMidiIn, 6) < 0 || dup2((int)hMidiOut, 7) < 0)
    {
        perror("dup2");
        return 1;
    }
    execvp(argv[1], argv + 1);
    perror("execvp");
    return 1;
}