/**
 * @file main.c
 * @brief Cross-platform main entry point for the OSCMix application
 */

#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "logging.h"
#include "socket.h"
#include "oscmix.h"
#include "arg.h"
#include "util.h"

/* Global variables */
extern int dflag;                   /**< Debug flag: enables verbose logging when set */
static int lflag;                   /**< Level meters flag: disables level meters when set */
static platform_socket_t rfd;       /**< Socket file descriptor for receiving OSC messages */
static platform_socket_t wfd;       /**< Socket file descriptor for sending OSC messages */
static platform_midiin_t midi_in;   /**< MIDI input device handle */
static platform_midiout_t midi_out; /**< MIDI output device handle */
static volatile int running = 1;    /**< Flag to control thread execution */

/* Default addresses for OSC communication */
static char defrecvaddr[] = "udp!127.0.0.1!7222";                    /**< Default address for receiving OSC messages */
static char defsendaddr[] = "udp!127.0.0.1!8222";                    /**< Default address for sending OSC messages */
static char mcastaddr[] = "udp!224.0.0.1!8222";                      /**< Multicast address for sending OSC messages */
static const unsigned char refreshosc[] = "/refresh\0\0\0\0,\0\0\0"; /**< OSC message for triggering a refresh */

/**
 * @brief Prints usage information and exits
 */
static void usage(void)
{
    fprintf(stderr, "usage: oscmix [-dlm] [-r addr] [-s addr] [-p midiport]\n");
    exit(1);
}

/**
 * @brief MIDI callback function
 */
static void midi_callback(void *data, size_t len, void *user_data)
{
    static uint_least32_t payload[2048]; // Buffer for decoded payload

    if (data && len > 0)
    {
        // Process the MIDI data
        handlesysex((const unsigned char *)data, len, payload);
    }
}

/**
 * @brief Thread function for reading MIDI messages from the RME device
 */
static void *midiread_thread(void *arg)
{
    unsigned char buffer[8192];

    // Add a buffer for MIDI input
    platform_midi_add_buffer(midi_in, buffer, sizeof(buffer));

    // This thread mainly exists to keep the application running
    // The actual MIDI processing happens in the callback
    while (running)
    {
        platform_sleep_ms(50);
    }

    return NULL;
}

/**
 * @brief Thread function for reading OSC messages from the network
 */
static void *oscread_thread(void *arg)
{
    platform_socket_t fd = *(platform_socket_t *)arg;
    ssize_t ret;
    unsigned char buf[8192];

    while (running)
    {
        ret = platform_socket_recv(fd, buf, sizeof(buf), 0);
        if (ret < 0)
        {
            log_error("Error receiving OSC data: %s", platform_strerror(platform_errno()));
            platform_sleep_ms(100);
            continue;
        }

        handleosc(buf, ret);
    }

    return NULL;
}

/**
 * @brief Sends MIDI data to the RME device
 */
void writemidi(const void *buf, size_t len)
{
    if (!buf || len == 0)
        return;

    if (platform_midi_send(midi_out, buf, len) != 0)
    {
        log_error("Failed to send MIDI data");
    }
    else
    {
        log_debug("Sent %zu bytes of MIDI data", len);
    }
}

/**
 * @brief Sends OSC data to the network
 */
void writeosc(const void *buf, size_t len)
{
    ssize_t ret = platform_socket_send(wfd, buf, len, 0);

    if (ret < 0)
    {
        log_error("Error sending OSC data: %s", platform_strerror(platform_errno()));
    }
    else if ((size_t)ret != len)
    {
        log_warning("Incomplete OSC data sent: %zd of %zu bytes", ret, len);
    }
}

/**
 * @brief Signal handler for application cleanup
 */
static void signal_handler(int sig)
{
    log_info("Received signal %d, cleaning up...", sig);

    // Stop threads
    running = 0;

    // Exit the application - cleanup will happen via atexit handler
    exit(0);
}

/**
 * @brief Cleanup function called at application exit
 */
static void cleanup_handler(void)
{
    log_info("Performing application cleanup");

    // Close MIDI devices
    platform_midi_close_input(midi_in);
    platform_midi_close_output(midi_out);

    // Close network sockets
    platform_socket_close(rfd);
    platform_socket_close(wfd);

    // Clean up subsystems
    platform_midi_cleanup();
    platform_socket_cleanup();

    // Close logging system
    log_cleanup();

    log_info("Cleanup complete");
}

/**
 * @brief Main entry point for the OSCMix application
 */
int main(int argc, char *argv[])
{
    char *recvaddr, *sendaddr;
    platform_thread_t midireader, oscreader;
    const char *port;
    int result;

    // Initialize app directory
    char app_dir[PLATFORM_MAX_PATH];
    char log_dir[PLATFORM_MAX_PATH];

    if (platform_get_app_data_dir(app_dir, sizeof(app_dir)) == 0)
    {
        // Create OSCMix logs directory
        if (platform_path_join(log_dir, sizeof(log_dir), app_dir, "OSCMix/logs") == 0)
        {
            platform_ensure_directory(log_dir);
        }
    }

    // Initialize logging early
    char log_filename[PLATFORM_MAX_PATH];
    char timestamp[64];

    platform_format_time(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S");
    snprintf(log_filename, sizeof(log_filename), "oscmix_%s.log", timestamp);

    // Create complete log path
    char log_path[PLATFORM_MAX_PATH];
    platform_path_join(log_path, sizeof(log_path), log_dir, log_filename);

    // Parse debug flag early for logging
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] == 'd')
        {
            dflag = 1;
            break;
        }
    }

    result = log_init(log_path, dflag, NULL);
    if (result != 0)
    {
        fprintf(stderr, "Warning: Failed to initialize logging to file, using stderr only\n");
    }

    log_info("OSCMix starting up");
    log_info("Log file: %s", log_path);

    // Set up signal handling and cleanup
    platform_set_signal_handler(signal_handler);
    platform_set_cleanup_handler(cleanup_handler);

    // Initialize socket subsystem
    if (socket_init() != 0)
    {
        log_error("Socket initialization failed");
        return 1;
    }

    // Initialize MIDI subsystem
    if (platform_midi_init() != 0)
    {
        log_error("MIDI initialization failed");
        socket_cleanup();
        return 1;
    }

    // Initialize default values
    recvaddr = defrecvaddr;
    sendaddr = defsendaddr;
    port = NULL;

    // Parse command line arguments
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

    log_info("Receiving OSC on %s", recvaddr);
    log_info("Sending OSC to %s", sendaddr);

    // Open sockets for OSC communication
    rfd = sockopen(recvaddr, 1);
    if (rfd == PLATFORM_INVALID_SOCKET)
    {
        log_error("Failed to open receive socket");
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    wfd = sockopen(sendaddr, 0);
    if (wfd == PLATFORM_INVALID_SOCKET)
    {
        log_error("Failed to open send socket");
        platform_socket_close(rfd);
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    // Get MIDI device port from argument or environment
    if (!port)
    {
        port = getenv("MIDIPORT");
        if (!port)
        {
            log_error("MIDI device not specified; pass -p or set MIDIPORT");
            platform_socket_close(rfd);
            platform_socket_close(wfd);
            platform_midi_cleanup();
            socket_cleanup();
            return 1;
        }
    }

    log_info("Using MIDI device: %s", port);

    // Open MIDI input device
    result = platform_midi_open_input(port, &midi_in);
    if (result != 0)
    {
        log_error("Failed to open MIDI input device: %s", port);
        platform_socket_close(rfd);
        platform_socket_close(wfd);
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    // Open MIDI output device
    result = platform_midi_open_output(port, &midi_out);
    if (result != 0)
    {
        log_error("Failed to open MIDI output device: %s", port);
        platform_midi_close_input(midi_in);
        platform_socket_close(rfd);
        platform_socket_close(wfd);
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    // Set up MIDI callback
    platform_midi_set_callback(midi_in, midi_callback, NULL);

    // Initialize OSCMix core with the specified device
    if (init(port) != 0)
    {
        log_error("OSCMix initialization failed");
        platform_midi_close_input(midi_in);
        platform_midi_close_output(midi_out);
        platform_socket_close(rfd);
        platform_socket_close(wfd);
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    if (register_osc_observers() != 0)
    {
        log_error("Failed to register OSC observers - GUI state updates may be unreliable");

        // Attempt fallback for essential observers
        log_warning("Attempting to register essential observers only");
        if (register_essential_osc_observers() != 0)
        {
            log_error("Critical failure: Cannot register any OSC observers");
        }
        else
        {
            log_warning("Essential observers registered, but some features may be limited");
        }
    }

    // Create thread for reading MIDI messages
    if (platform_thread_create(&midireader, midiread_thread, NULL) != 0)
    {
        log_error("Failed to create MIDI reader thread");
        platform_midi_close_input(midi_in);
        platform_midi_close_output(midi_out);
        platform_socket_close(rfd);
        platform_socket_close(wfd);
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    // Create thread for reading OSC messages
    if (platform_thread_create(&oscreader, oscread_thread, &rfd) != 0)
    {
        log_error("Failed to create OSC reader thread");
        platform_midi_close_input(midi_in);
        platform_midi_close_output(midi_out);
        platform_socket_close(rfd);
        platform_socket_close(wfd);
        platform_midi_cleanup();
        socket_cleanup();
        return 1;
    }

    // Send initial refresh command
    log_info("Sending initial refresh command");
    handleosc(refreshosc, sizeof(refreshosc) - 1);

    // Main event loop
    log_info("Entering main event loop");
    while (running)
    {
        // Process periodic tasks
        handletimer(lflag == 0);

        // Sleep to reduce CPU usage
        platform_sleep_ms(100);
    }

    // Join threads (should not be reached due to signal handler)
    platform_thread_join(midireader, NULL);
    platform_thread_join(oscreader, NULL);

    // Clean up will happen via the atexit handler
    return 0;
}
