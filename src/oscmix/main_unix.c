/**
 * @file main_unix.c
 * @brief Main entry point for OSCMix on POSIX systems (Linux/macOS)
 *
 * This file implements the main application logic and platform-specific code
 * for running OSCMix on POSIX-compliant systems. It handles:
 * - Command line argument parsing
 * - Socket initialization for OSC communication
 * - MIDI device connection
 * - Threading for MIDI and OSC message handling
 * - Signal handling for timers and cleanup
 */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <process.h>
typedef HANDLE thread_t;
#define thread_create(t, f, a) ((*t = _beginthreadex(NULL, 0, f, a, 0, NULL)) == 0)
#define thread_join(t) WaitForSingleObject(t, INFINITE)
#define sleep_ms(ms) Sleep(ms)
#else
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
typedef pthread_t thread_t;
#define thread_create(t, f, a) pthread_create(t, NULL, f, a)
#define thread_join(t) pthread_join(t, NULL)
#define sleep_ms(ms) usleep((ms) * 1000)
#endif

#include "socket.h"
#include "oscmix.h"
#include "arg.h"
#include "util.h"

extern int dflag;	 /**< Debug flag: enables verbose logging when set */
static int lflag;	 /**< Level meters flag: disables level meters when set */
static socket_t rfd; /**< Socket file descriptor for receiving OSC messages */
static socket_t wfd; /**< Socket file descriptor for sending OSC messages */

/**
 * @brief Prints usage information and exits
 */
static void
usage(void)
{
	fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr]\n");
	exit(1);
}

#ifdef _WIN32
/**
 * @brief Thread function for reading MIDI messages from the RME device (Windows)
 *
 * This is a placeholder for Windows - actual implementation is in main_old.c
 *
 * @param arg Thread argument (unused)
 * @return Thread return value (unused)
 */
static unsigned __stdcall midiread(void *arg)
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
#endif
{
	unsigned char data[8192], *datapos, *dataend, *nextpos;
	uint_least32_t payload[sizeof data / 4];
	ssize_t ret;

#ifndef _WIN32
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
#else
	// On Windows, MIDI reading is handled in main_old.c
	for (;;)
	{
		sleep_ms(100);
	}
#endif
	return 0;
}

#ifdef _WIN32
/**
 * @brief Thread function for reading OSC messages from the network (Windows)
 *
 * @param arg Pointer to the socket file descriptor
 * @return Thread return value (unused)
 */
static unsigned __stdcall oscread(void *arg)
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
#endif
{
	socket_t fd;
	ssize_t ret;
	unsigned char buf[8192];

#ifdef _WIN32
	fd = *(socket_t *)arg;
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
#else
	fd = *(int *)arg;
	for (;;)
	{
		ret = read(fd, buf, sizeof buf);
		if (ret < 0)
		{
			perror("recv");
			break;
		}
		handleosc(buf, ret);
	}
#endif
	return 0;
}

/**
 * @brief Sends MIDI data to the RME device
 *
 * Writes MIDI data to file descriptor 7, which is connected to the MIDI output
 * of the device. This function is called by the oscmix core logic to send
 * SysEx commands to the device.
 *
 * @param buf Pointer to the MIDI data to send
 * @param len Length of the MIDI data in bytes
 */
void writemidi(const void *buf, size_t len)
{
#ifdef _WIN32
	// Windows implementation is in main_old.c
	// Stub to avoid linker errors
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

/**
 * @brief Main entry point for the OSCMix application
 *
 * Parses command line arguments, initializes sockets and MIDI connections,
 * creates threads for MIDI and OSC handling, and enters the main event loop.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit code (0 on success, non-zero on failure)
 */
int main(int argc, char *argv[])
{
	static char defrecvaddr[] = "udp!127.0.0.1!7222";					 /**< Default address for receiving OSC messages */
	static char defsendaddr[] = "udp!127.0.0.1!8222";					 /**< Default address for sending OSC messages */
	static char mcastaddr[] = "udp!224.0.0.1!8222";						 /**< Multicast address for sending OSC messages */
	static const unsigned char refreshosc[] = "/refresh\0\0\0\0,\0\0\0"; /**< OSC message for triggering a refresh */
	char *recvaddr, *sendaddr;
	thread_t midireader, oscreader;
	const char *port;

#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		fatal("WSAStartup failed");
	}
#else
	int err, sig;
	struct itimerval it;
	sigset_t set;

	if (fcntl(6, F_GETFD) < 0)
		fatal("fcntl 6:");
	if (fcntl(7, F_GETFD) < 0)
		fatal("fcntl 7:");
#endif

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
	case 'p':
		port = EARGF(usage());
		break;
	default:
		usage();
		break;
	}
	ARGEND

	/* Open sockets for OSC communication */
	rfd = sockopen(recvaddr, 1);
	wfd = sockopen(sendaddr, 0);

	/* Get MIDI device port from argument or environment */
	if (!port)
	{
		port = getenv("MIDIPORT");
		if (!port)
			fatal("device is not specified; pass -p or set MIDIPORT");
	}

	/* Initialize OSCMix core with the specified device */
	if (init(port) != 0)
		return 1;

#ifdef _WIN32
	/* Create thread for reading MIDI messages (Windows) */
	midireader = (HANDLE)_beginthreadex(NULL, 0, midiread, &wfd, 0, NULL);
	if (midireader == NULL)
		fatal("CreateThread failed");

	/* Create thread for reading OSC messages (Windows) */
	oscreader = (HANDLE)_beginthreadex(NULL, 0, oscread, &rfd, 0, NULL);
	if (oscreader == NULL)
		fatal("CreateThread failed");

	/* Send initial refresh command and enter main loop (Windows) */
	handleosc(refreshosc, sizeof refreshosc - 1);
	for (;;)
	{
		Sleep(100);
		handletimer(lflag == 0);
	}
#else
	/* Block all signals in main thread */
	sigfillset(&set);
	pthread_sigmask(SIG_SETMASK, &set, NULL);

	/* Create thread for reading MIDI messages (POSIX) */
	err = pthread_create(&midireader, NULL, midiread, &wfd);
	if (err)
		fatal("pthread_create: %s", strerror(err));

	/* Create thread for reading OSC messages (POSIX) */
	err = pthread_create(&oscreader, NULL, oscread, &rfd);
	if (err)
		fatal("pthread_create: %s", strerror(err));

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
}
