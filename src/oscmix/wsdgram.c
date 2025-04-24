/**
 * @file wsdgram.c
 * @brief WebSocket to UDP datagram bridge for OSC communication
 */

#include "platform.h" // Use platform abstraction layer
#include "logging.h"  // Use unified logging
#include "base64.h"
#include "http.h"
#include "intpack.h"
#include "sha1.h"
#include "util.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// WebSocket operation codes
enum
{
	CLOSE = 0x8,
	PING = 0x9,
	PONG = 0xa,
};

// Connection state
static platform_socket_t rfd;			// Socket for receiving
static platform_socket_t wfd;			// Socket for sending
static bool closing;					// Flag to indicate connection is closing
static platform_thread_t writer_thread; // Writer thread handle
static platform_mutex_t write_mutex;	// Mutex for thread-safe writing

/**
 * @brief Print usage information and exit
 */
static void usage(void)
{
	fprintf(stderr, "usage: wsdgram [-m] [-s addr] [-r addr]\n");
	exit(1);
}

/**
 * @brief Perform WebSocket handshake with client
 *
 * @param rd Input stream for reading client request
 * @param wr Output stream for writing response
 */
static void handshake(platform_stream_t *rd, platform_stream_t *wr)
{
	static const char response[] =
		"HTTP/1.1 101 Switching Protocols\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n";
	struct http_request req;
	struct http_header hdr;
	bool websocket, upgrade, havekey;
	long version;
	char *token;
	unsigned char sha1buf[20];
	char accept[(sizeof sha1buf + 2) / 3 * 4], buf[2048];

	// Read initial request line
	if (!platform_gets(buf, sizeof(buf), rd))
	{
		log_error("Failed to read WebSocket handshake request");
		goto fail;
	}

	if (http_request(buf, strlen(buf), &req) != 0)
	{
		log_error("Invalid HTTP request: %s", buf);
		goto fail;
	}

	if (req.method != HTTP_GET)
	{
		log_error("WebSocket handshake requires GET method");
		goto fail;
	}

	// Initialize flags
	upgrade = false;
	websocket = false;
	havekey = false;
	version = 0;

	// Process headers
	for (;;)
	{
		if (!platform_gets(buf, sizeof(buf), rd))
		{
			log_error("Failed to read WebSocket handshake headers");
			goto fail;
		}

		if (http_header(buf, strlen(buf), &hdr) != 0)
		{
			log_error("Invalid HTTP header: %s", buf);
			goto fail;
		}

		if (!hdr.name)
			break;

		if (strcmp(hdr.name, "Upgrade") == 0)
		{
			for (token = strtok(hdr.value, " \t,"); token; token = strtok(NULL, " \t,"))
			{
				if (strcmp(token, "websocket") == 0)
				{
					websocket = true;
					break;
				}
			}
		}
		else if (strcmp(hdr.name, "Connection") == 0)
		{
			for (token = strtok(hdr.value, " \t,"); token; token = strtok(NULL, " \t,"))
			{
				if (strcmp(token, "Upgrade") == 0)
				{
					upgrade = true;
					break;
				}
			}
		}
		else if (strcmp(hdr.name, "Sec-WebSocket-Key") == 0)
		{
			static const char guid[36] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
			sha1_context ctx;

			if (hdr.value_len != (16 + 2) / 3 * 4)
			{
				log_error("Invalid WebSocket key length");
				goto fail;
			}

			// Calculate the accept key
			sha1_init(&ctx);
			sha1_update(&ctx, hdr.value, hdr.value_len);
			sha1_update(&ctx, guid, sizeof guid);
			sha1_out(&ctx, sha1buf);
			base64_encode(accept, sha1buf, sizeof sha1buf);
			havekey = true;
		}
		else if (strcmp(hdr.name, "Sec-WebSocket-Version") == 0)
		{
			char *end;

			version = strtol(hdr.value, &end, 10);
			if (hdr.value_len == 0 || hdr.value_len != end - hdr.value)
			{
				log_error("Invalid WebSocket version");
				goto fail;
			}
		}
	}

	// Verify required headers
	if (!upgrade || !websocket || !havekey || version != 13)
	{
		log_error("WebSocket handshake missing required fields: upgrade=%d, websocket=%d, key=%d, version=%ld",
				  upgrade, websocket, havekey, version);
		goto fail;
	}

	// Send successful handshake response
	platform_write(response, sizeof(response) - 1, wr);
	platform_printf(wr, "Sec-WebSocket-Accept: %s\r\n\r\n", accept);
	platform_flush(wr);

	log_info("WebSocket connection established");
	return;

fail:
	http_error(wr, 400, "Bad Request", NULL, 0);
	log_error("WebSocket handshake failed");
	exit(1);
}

/**
 * @brief Send a WebSocket frame
 *
 * @param wr Output stream
 * @param op Operation code
 * @param buf Data buffer
 * @param len Data length
 */
static void writeframe(platform_stream_t *wr, int op, const void *buf, size_t len)
{
	unsigned char hdr[10];
	int hdrlen;

	// Format the WebSocket frame header
	hdr[0] = 0x80 | op;
	if (len < 126)
	{
		hdr[1] = len;
		hdrlen = 2;
	}
	else if (len <= 0xffff)
	{
		hdr[1] = 126;
		putbe16(hdr + 2, len);
		hdrlen = 4;
	}
	else
	{
		hdr[1] = 127;
		putbe64(hdr + 2, len);
		hdrlen = 10;
	}

	// Use platform mutex for thread safety
	platform_mutex_lock(&write_mutex);

	if (!closing)
	{
		// Write the frame using platform I/O functions
		if (platform_write(hdr, hdrlen, wr) != hdrlen ||
			platform_write(buf, len, wr) != len ||
			platform_flush(wr) != 0)
		{
			log_error("Failed to write WebSocket frame: %s", platform_strerror(platform_errno()));
			exit(1);
		}
	}

	platform_mutex_unlock(&write_mutex);
}

/**
 * @brief Send a WebSocket close frame
 *
 * @param wr Output stream
 * @param code Close status code
 */
static void writeclose(platform_stream_t *wr, int code)
{
	unsigned char buf[2];

	platform_mutex_lock(&write_mutex);

	if (!closing)
	{
		putbe16(buf, code);
		writeframe(wr, CLOSE, buf, sizeof buf);
		closing = true;
		log_info("WebSocket connection closing with code %d", code);
	}

	platform_mutex_unlock(&write_mutex);
}

/**
 * @brief Thread function to read from UDP and write to WebSocket
 *
 * @param wr Output WebSocket stream
 */
static void writer(platform_stream_t *wr)
{
	unsigned char buf[4096];
	ssize_t ret;

	log_debug("WebSocket writer thread started");

	for (;;)
	{
		// Read from UDP socket using platform socket functions
		ret = platform_socket_recv(rfd, buf, sizeof(buf), 0);

		if (ret < 0)
		{
			log_error("Socket read error: %s", platform_strerror(platform_errno()));
			writeclose(wr, 1011);
			break;
		}

		if (ret == 0)
		{
			log_info("Socket connection closed");
			writeclose(wr, 1001);
			break;
		}

		// Send the data as a binary WebSocket frame
		writeframe(wr, 2, buf, ret);
		log_debug("Sent %zd bytes to WebSocket", ret);
	}

	log_debug("WebSocket writer thread exiting");
}

/**
 * @brief Thread entry point for writer function
 *
 * @param arg Thread argument (WebSocket stream)
 * @return Thread result
 */
static void *writermain(void *arg)
{
	writer(arg);
	return NULL;
}

/**
 * @brief Unmask WebSocket data
 *
 * @param buf Data buffer
 * @param len Data length
 * @param key Masking key (4 bytes)
 */
static void unmask(unsigned char *buf, size_t len, unsigned char key[static 4])
{
	size_t i;

	for (i = 0; i < len; ++i)
		buf[i] ^= key[i & 3];
}

/**
 * @brief Read from WebSocket and write to UDP
 *
 * @param rd Input WebSocket stream
 * @param wr Output WebSocket stream (for control frames)
 */
static void reader(platform_stream_t *rd, platform_stream_t *wr)
{
	unsigned char hdr[2], lenbuf[8], key[4], ctl[125], msg[4096];
	int msglen;
	unsigned long long len;
	bool masked, skip;

	log_debug("WebSocket reader thread started");

	skip = false;
	msglen = 0;

	for (;;)
	{
		// Read WebSocket frame header
		if (platform_read(hdr, sizeof(hdr), rd) != sizeof(hdr))
			break;

		len = hdr[1] & 0x7f;
		if (len == 126)
		{
			if (platform_read(lenbuf, 2, rd) != 2)
				break;
			len = getbe16(lenbuf);
		}
		else if (len == 127)
		{
			if (platform_read(lenbuf, 8, rd) != 8)
				break;
			len = getbe64(lenbuf);
		}

		masked = hdr[1] & 0x80;
		if (masked && platform_read(key, sizeof(key), rd) != sizeof(key))
			break;

		if (hdr[0] & 0x8)
		{
			/* Control frame */
			if ((hdr[0] & 0x80) == 0 || len >= 126)
			{
				log_error("Invalid control frame");
				writeclose(wr, 1002);
				exit(1);
			}

			if (platform_read(ctl, len, rd) != len)
				break;

			if (masked)
				unmask(ctl, len, key);

			switch (hdr[0] & 0xf)
			{
			case CLOSE:
				log_info("Received WebSocket close frame");
				platform_mutex_lock(&write_mutex);
				writeframe(wr, CLOSE, ctl, len > 2 ? 2 : len);
				platform_mutex_unlock(&write_mutex);
				exit(0);
				break;

			case PING:
				log_debug("Received WebSocket ping");
				writeframe(wr, PONG, ctl, len);
				break;

			case PONG:
				log_debug("Received WebSocket pong");
				/* ignore */
				break;
			}
		}
		else
		{
			/* Data frame */
			if (!skip && sizeof(msg) - msglen < len)
			{
				log_error("WebSocket message too large (max: %zu bytes)", sizeof(msg));
				writeclose(wr, 1009);
				skip = true;
			}

			if (skip)
			{
				msglen = 0;
				while (len > sizeof(msg))
				{
					if (platform_read(msg, sizeof(msg), rd) != sizeof(msg))
						goto done;
					len -= sizeof(msg);
				}
			}

			if (platform_read(msg + msglen, len, rd) != len)
				break;

			msglen += len;

			if (hdr[0] & 0x80)
			{
				ssize_t ret;

				if (skip)
				{
					skip = false;
					continue;
				}

				if (masked)
					unmask(msg, msglen, key);

				// Send to UDP socket using platform socket functions
				ret = platform_socket_send(wfd, msg, msglen, 0);

				if (ret <= 0)
				{
					if (platform_errno() != PLATFORM_ECONNREFUSED)
					{
						log_error("Socket write error: %s", platform_strerror(platform_errno()));
						writeclose(wr, 1011);
					}
				}
				else if (ret != msglen)
				{
					log_error("Incomplete socket write: %zd of %d bytes", ret, msglen);
					writeclose(wr, 1011);
				}
				else
				{
					log_debug("Sent %zd bytes to UDP", ret);
				}

				msglen = 0;
			}
		}
	}

done:
	if (platform_error(rd))
	{
		log_error("Read error: %s", platform_strerror(platform_errno()));
		exit(1);
	}

	log_info("WebSocket connection closed");
	exit(0);
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[])
{
	static char mcastaddr[] = "udp!224.0.0.1!8222";
	static char defrecvaddr[] = "udp!127.0.0.1!8222";
	static char defsendaddr[] = "udp!127.0.0.1!7222";
	int err;
	char *recvaddr, *sendaddr;
	platform_stream_t *stdin_stream, *stdout_stream;

	// Initialize logging
	char log_file[256];
	if (platform_get_app_data_dir(log_file, sizeof(log_file)) == 0)
	{
		// Create logs directory
		char log_dir[512];
		if (platform_path_join(log_dir, sizeof(log_dir), log_file, "OSCMix/logs") == 0)
		{
			platform_ensure_directory(log_dir);
			// Create log filename
			char timestamp[32];
			platform_format_time(timestamp, sizeof(timestamp), "%Y%m%d");
			char filename[128];
			snprintf(filename, sizeof(filename), "wsdgram_%s.log", timestamp);
			platform_path_join(log_file, sizeof(log_file), log_dir, filename);
			log_init(log_file, 0, NULL);
		}
	}

	log_info("WebSocket bridge starting up");

	recvaddr = defrecvaddr;
	sendaddr = defsendaddr;

	// Parse command-line arguments
	int i = 1;
	while (i < argc)
	{
		if (argv[i][0] == '-')
		{
			switch (argv[i][1])
			{
			case 'r':
				if (i + 1 < argc)
				{
					recvaddr = argv[i + 1];
					i += 2;
				}
				else
				{
					usage();
				}
				break;
			case 's':
				if (i + 1 < argc)
				{
					sendaddr = argv[i + 1];
					i += 2;
				}
				else
				{
					usage();
				}
				break;
			case 'm':
				recvaddr = mcastaddr;
				i++;
				break;
			default:
				usage();
			}
		}
		else
		{
			usage();
		}
	}

	log_info("Receiving from: %s", recvaddr);
	log_info("Sending to: %s", sendaddr);

	// Initialize platform mutex
	if (platform_mutex_init(&write_mutex) != 0)
	{
		log_error("Failed to initialize mutex");
		return 1;
	}

	// Open UDP sockets using platform socket functions
	rfd = platform_socket_open(recvaddr, 1);
	if (rfd == PLATFORM_INVALID_SOCKET)
	{
		log_error("Failed to open receive socket: %s", platform_strerror(platform_errno()));
		return 1;
	}

	wfd = platform_socket_open(sendaddr, 0);
	if (wfd == PLATFORM_INVALID_SOCKET)
	{
		log_error("Failed to open send socket: %s", platform_strerror(platform_errno()));
		platform_socket_close(rfd);
		return 1;
	}

	// Get platform stream wrappers for stdin/stdout
	stdin_stream = platform_get_stdin();
	stdout_stream = platform_get_stdout();

	// Perform WebSocket handshake
	handshake(stdin_stream, stdout_stream);

	// Create writer thread
	err = platform_thread_create(&writer_thread, writermain, stdout_stream);
	if (err)
	{
		log_error("Failed to create writer thread: %s", platform_strerror(err));
		platform_socket_close(rfd);
		platform_socket_close(wfd);
		return 1;
	}

	// Run reader on main thread
	reader(stdin_stream, stdout_stream);

	// Clean up (though we typically exit before here)
	platform_thread_join(writer_thread, NULL);
	platform_mutex_destroy(&write_mutex);
	platform_socket_close(rfd);
	platform_socket_close(wfd);
	log_cleanup();

	return 0;
}
