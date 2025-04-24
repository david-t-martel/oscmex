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

#include "platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"

/* Project includes */
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
 *
 * This function is called when MIDI data is received.
 * It processes both short MIDI messages and SysEx messages, and dispatches them
 * to the appropriate handlers.
 *
 * @param data Pointer to the MIDI data
 * @param len Length of the MIDI data
 * @param user_data User data passed to the callback
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
 *
 * @param arg Thread argument (unused)
 * @return Thread return value (unused)
 */
#ifdef _WIN32
static unsigned __stdcall midiread_thread(void *arg)
#else
static void *midiread_thread(void *arg)
#endif
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

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/**
 * @brief Thread function for reading OSC messages from the network
 *
 * Continuously reads OSC messages from the network socket,
 * and dispatches them to handleosc() for processing.
 *
 * @param arg Pointer to the socket file descriptor
 * @return Thread return value (unused)
 */
#ifdef _WIN32
static unsigned __stdcall oscread_thread(void *arg)
#else
static void *oscread_thread(void *arg)
#endif
{
    platform_socket_t fd = *(platform_socket_t *)arg;
    int ret;
    unsigned char buf[8192];

    while (running)
    {
        ret = platform_socket_recv(fd, buf, sizeof(buf));
        if (ret < 0)
        {
            fprintf(stderr, "Error receiving OSC data\n");
            platform_sleep_ms(100);
            continue;
        }

        handleosc(buf, ret);
    }

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/**
 * @brief Sends MIDI data to the RME device
 *
 * @param buf Pointer to the MIDI data to send
 * @param len Length of the MIDI data in bytes
 */
void writemidi(const void *buf, size_t len)
{
    if (!buf || len == 0)
        return;

    platform_midi_send(midi_out, buf, len);
}

/**
 * @brief Sends OSC data to the network
 *
 * @param buf Pointer to the OSC data to send
 * @param len Length of the OSC data in bytes
 */
void writeosc(const void *buf, size_t len)
{
    int ret = platform_socket_send(wfd, buf, len);

    if (ret < 0)
    {
        fprintf(stderr, "Error sending OSC data\n");
    }
    else if ((size_t)ret != len)
    {
        fprintf(stderr, "Incomplete OSC data sent: %d != %zu\n", ret, len);
    }
}

/**
 * @brief Signal handler for application cleanup
 *
 * @param sig Signal number
 */
static void signal_handler(int sig)
{
    fprintf(stderr, "\nReceived signal %d, cleaning up...\n", sig);

    // Stop threads
    running = 0;

    // Close MIDI devices
    platform_midi_close_input(midi_in);
    platform_midi_close_output(midi_out);

    // Close network sockets
    platform_socket_close(rfd);
    platform_socket_close(wfd);

    // Clean up socket subsystem
    platform_socket_cleanup();

    // Exit the application
    exit(0);
}

/**
 * @brief Cleanup function called at application exit
 */
static void cleanup_handler(void)
{
    // Close logging system
    log_cleanup();

    // Final cleanup actions that should happen regardless of exit path
    platform_midi_cleanup();
    platform_socket_cleanup();
}

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
    int result;

    // Initialize logging early
    char log_filename[512];
    if (log_create_filename(log_filename, sizeof(log_filename), "oscmix") != 0)
    {
        fprintf(stderr, "Failed to create log filename\n");
        return 1;
    }

    result = log_init(log_filename, argc > 1 && strcmp(argv[1], "--debug") == 0, NULL);
    if (result != 0)
    {
        fprintf(stderr, "Failed to initialize logging system\n");
        return 1;
    }

    log_info("OSCMix v%s starting up", VERSION);
    log_info("Log file: %s", log_filename);

    // Initialize socket subsystem
    if (platform_socket_init() != 0)
    {
        fprintf(stderr, "Socket initialization failed\n");
        return 1;
    }

    // Initialize logging system
    char log_filename[256];
    snprintf(log_filename, sizeof(log_filename), "oscmix_%d.log", (int)time(NULL));
    if (log_init(log_filename, dflag, NULL) != 0)
    {
        fprintf(stderr, "Warning: Failed to initialize logging system\n");
    }

    // Set up signal handling
    platform_set_signal_handler(signal_handler);
    platform_set_cleanup_handler(cleanup_handler);

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

    // Open sockets for OSC communication
    rfd = sockopen(recvaddr, 1);
    wfd = sockopen(sendaddr, 0);

    // Get MIDI device port from argument or environment
    if (!port)
    {
        port = getenv("MIDIPORT");
        if (!port)
        {
            fprintf(stderr, "MIDI device not specified; pass -p or set MIDIPORT\n");
            platform_socket_cleanup();
            return 1;
        }
    }

    // Initialize MIDI subsystem
    if (platform_midi_init() != 0)
    {
        fprintf(stderr, "MIDI initialization failed\n");
        platform_socket_cleanup();
        return 1;
    }

    // Open MIDI input device
    result = platform_midi_open_input(port, &midi_in);
    if (result != 0)
    {
        fprintf(stderr, "Failed to open MIDI input device: %s\n", port);
        platform_midi_cleanup();
        platform_socket_cleanup();
        return 1;
    }

    // Open MIDI output device
    result = platform_midi_open_output(port, &midi_out);
    if (result != 0)
    {
        fprintf(stderr, "Failed to open MIDI output device: %s\n", port);
        platform_midi_close_input(midi_in);
        platform_midi_cleanup();
        platform_socket_cleanup();
        return 1;
    }

    // Set up MIDI callback
    platform_midi_set_callback(midi_in, midi_callback, NULL);

    // Initialize OSCMix core with the specified device
    if (init(port) != 0)
    {
        fprintf(stderr, "OSCMix initialization failed\n");
        platform_midi_close_input(midi_in);
        platform_midi_close_output(midi_out);
        platform_midi_cleanup();
        platform_socket_cleanup();
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
    platform_thread_create(&midireader, midiread_thread, NULL);

    // Create thread for reading OSC messages
    platform_thread_create(&oscreader, oscread_thread, &rfd);

    // Send initial refresh command
    handleosc(refreshosc, sizeof(refreshosc) - 1);

    // Main event loop
    while (running)
    {
        // Process periodic tasks
        handletimer(lflag == 0);

        // Sleep to reduce CPU usage
        platform_sleep_ms(100);
    }

    // Join threads (should not be reached due to signal handler)
    platform_thread_join(midireader);
    platform_thread_join(oscreader);

    // Clean up
    platform_midi_close_input(midi_in);
    platform_midi_close_output(midi_out);
    platform_midi_cleanup();
    platform_socket_close(rfd);
    platform_socket_close(wfd);
    platform_socket_cleanup();

    // Before exiting:
    log_cleanup();
    return 0;
}
