#define _WIN32_WINNT 0x0601 // Windows 7 or later
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../arg.h"
#include "../sysex.h"

static HMIDIIN hMidiIn;
static HMIDIOUT hMidiOut;
static int sflag;
static int wflag;

static void usage(void)
{
    fprintf(stderr,
            "usage: regtool [-s] client:port\n"
            "       regtool [-s] -w client:port [reg val]...\n");
    exit(1);
}

static void dumpsysex(const char *prefix, const unsigned char *buf, size_t len)
{
    static const unsigned char hdr[] = {0xf0, 0x00, 0x20, 0x0d, 0x10};
    const unsigned char *pos, *end;
    unsigned long regval;
    unsigned reg, val, par;

    pos = buf;
    end = pos + len;
    if (sflag)
    {
        fputs(prefix, stdout);
        for (; pos != end; ++pos)
            printf(" %.2X", *pos);
        fputc('\n', stdout);
    }
    pos = buf;
    --end;
    if (len < sizeof hdr || memcmp(pos, hdr, sizeof hdr) != 0 || (len - sizeof hdr - 2) % 5 != 0)
    {
        printf("skipping unexpected sysex\n");
        return;
    }
    if (pos[5] != 0)
    {
        printf("subid=%d", pos[5]);
        for (pos += sizeof hdr + 1; pos != end; pos += 5)
        {
            regval = getle32_7bit(pos);
            printf("%c%.8lX", pos == buf + sizeof hdr + 1 ? '\t' : ' ', regval);
        }
        fputc('\n', stdout);
        return;
    }
    for (pos += sizeof hdr + 1; pos != end; pos += 5)
    {
        regval = getle32_7bit(pos);
        reg = regval >> 16 & 0x7fff;
        val = regval & 0xffff;
        par = regval ^ regval >> 16 ^ 1;
        par ^= par >> 8;
        par ^= par >> 4;
        par ^= par >> 2;
        par ^= par >> 1;
        printf("%.4X\t%.4X", reg, val);
        if (par & 1)
            printf("bad parity");
        fputc('\n', stdout);
    }
    fflush(stdout);
}

static void CALLBACK midiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    static unsigned char buf[8192];
    static size_t len = 0;
    const unsigned char *evtbuf;
    size_t evtlen;

    if (wMsg != MIM_DATA)
        return;

    evtbuf = (const unsigned char *)&dwParam1;
    evtlen = 3; // MIDI messages are 3 bytes long

    if (evtbuf[0] == 0xf0)
    {
        if (len > 0)
        {
            fprintf(stderr, "dropping incomplete sysex\n");
            len = 0;
        }
    }
    if (evtlen > sizeof buf - len)
    {
        fprintf(stderr, "dropping sysex that is too long\n");
        len = evtbuf[evtlen - 1] == 0xf7 ? 0 : sizeof buf;
        return;
    }
    memcpy(buf + len, evtbuf, evtlen);
    len += evtlen;
    if (buf[len - 1] == 0xf7)
    {
        dumpsysex("<-", buf, len);
        len = 0;
    }
}

static void midiread(void)
{
    MMRESULT err;

    err = midiInOpen(&hMidiIn, 0, (DWORD_PTR)midiInProc, 0, CALLBACK_FUNCTION);
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInOpen failed: %d\n", err);
        exit(1);
    }

    err = midiInStart(hMidiIn);
    if (err != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiInStart failed: %d\n", err);
        exit(1);
    }

    // Keep the application running to process MIDI input
    while (1)
    {
        Sleep(100);
    }

    midiInClose(hMidiIn);
}

static void setreg(unsigned reg, unsigned val)
{
    unsigned par;
    unsigned long regval;
    unsigned char buf[12] = {0xf0, 0x00, 0x20, 0x0d, 0x10, 0x00, [sizeof buf - 1] = 0xf7};
    DWORD bytesWritten;

    reg &= 0x7fff;
    val &= 0xffff;
    par = reg ^ val ^ 1;
    par ^= par >> 8;
    par ^= par >> 4;
    par ^= par >> 2;
    par ^= par >> 1;
    regval = par << 31 | reg << 16 | val;
    putle32_7bit(buf + 6, regval);
    dumpsysex("->", buf, sizeof buf);

    bytesWritten = midiOutShortMsg(hMidiOut, *(DWORD *)buf);
    if (bytesWritten != MMSYSERR_NOERROR)
    {
        fprintf(stderr, "midiOutShortMsg failed: %d\n", bytesWritten);
    }
}

static void midiwrite(void)
{
    unsigned reg, val;
    char str[256];

    while (fgets(str, sizeof str, stdin))
    {
        if (sscanf(str, "%x %x", &reg, &val) != 2)
        {
            fprintf(stderr, "invalid input\n");
            continue;
        }
        setreg(reg, val);
    }
}

int main(int argc, char *argv[])
{
    int err;
    char *end;
    UINT midiInId, midiOutId;

    ARGBEGIN
    {
    case 's':
        sflag = 1;
        break;
    case 'w':
        wflag = 1;
        break;
    default:
        usage();
    }
    ARGEND

    if (argc < 1 || (!wflag && argc != 1) || argc % 2 != 1)
        usage();

    midiInId = strtol(argv[0], &end, 10);
    if (*end != ':')
        usage();
    midiOutId = strtol(end + 1, &end, 10);
    if (*end)
        usage();

    err = midiInOpen(&hMidiIn, midiInId, (DWORD_PTR)midiInProc, 0, CALLBACK_FUNCTION);
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

    if (wflag)
    {
        if (argc > 1)
        {
            int i;
            long reg, val;

            for (i = 1; i < argc; i += 2)
            {
                reg = strtol(argv[i], &end, 16);
                if (*end || reg < 0 || reg > 0x7fff)
                    usage();
                val = strtol(argv[i + 1], &end, 16);
                if (*end || val < -0x8000 || val > 0xffff)
                    usage();
                setreg(reg, val);
            }
        }
        else
        {
            midiwrite();
        }
    }
    else
    {
        midiread();
    }

    midiInClose(hMidiIn);
    midiOutClose(hMidiOut);

    return 0;
}