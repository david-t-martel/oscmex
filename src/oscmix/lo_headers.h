#cmakedefine PACKAGE_NAME "@PACKAGE_NAME@"
#cmakedefine PACKAGE_VERSION "@PACKAGE_VERSION@"
#define VERSION PACKAGE_VERSION
#cmakedefine LO_SO_VERSION @LO_SO_VERSION @

#cmakedefine HAVE_POLL
#cmakedefine HAVE_SELECT
#cmakedefine HAVE_GETIFADDRS
#cmakedefine HAVE_INET_PTON
#cmakedefine HAVE_LIBPTHREAD
#cmakedefine HAVE_WIN32_THREADS
#cmakedefine ENABLE_THREADS

#define PRINTF_LL "@PRINTF_LL@"

#ifndef LO_ERRORS_H
#define LO_ERRORS_H

#ifdef __cplusplus
extern "C"
{
#endif

#define LO_ENOPATH 9901
#define LO_ENOTYPE 9902
#define LO_UNKNOWNPROTO 9903
#define LO_NOPORT 9904
#define LO_TOOBIG 9905
#define LO_INT_ERR 9906
#define LO_EALLOC 9907
#define LO_EINVALIDPATH 9908
#define LO_EINVALIDTYPE 9909
#define LO_EBADTYPE 9910
#define LO_ESIZE 9911
#define LO_EINVALIDARG 9912
#define LO_ETERM 9913
#define LO_EPAD 9914
#define LO_EINVALIDBUND 9915
#define LO_EINVALIDTIME 9916

#ifdef __cplusplus
}
#endif

#endif

#ifndef LO_MACROS_H
#define LO_MACROS_H

/* macros that have to be defined after function signatures */

#ifdef __cplusplus
extern "C"
{
#endif

/* \brief Left for backward-compatibility.  See
 * LO_DEFAULT_MAX_MSG_SIZE below, and lo_server_max_msg_size().
 */
#define LO_MAX_MSG_SIZE 32768

/* \brief Maximum length of incoming UDP messages in bytes.
 */
#define LO_MAX_UDP_MSG_SIZE 65535

/* \brief Default maximum length of incoming messages in bytes,
 * corresponds to max UDP message size.
 */
#define LO_DEFAULT_MAX_MSG_SIZE LO_MAX_UDP_MSG_SIZE

/* \brief A set of macros to represent different communications transports
 */
#define LO_DEFAULT 0x0
#define LO_UDP 0x1
#define LO_UNIX 0x2
#define LO_TCP 0x4

/* an internal value, ignored in transmission but check against LO_MARKER in the
 * argument list. Used to do primitive bounds checking */
#define LO_MARKER_A (void *)0xdeadbeefdeadbeefLLU
#define LO_MARKER_B (void *)0xf00baa23f00baa23LLU

#define LO_ARGS_END LO_MARKER_A, LO_MARKER_B

#define lo_message_add_varargs(msg, types, list) \
	lo_message_add_varargs_internal(msg, types, list, __FILE__, __LINE__)

#ifdef _MSC_VER
#ifndef USE_ANSI_C
#define USE_ANSI_C
#endif
#endif

#if defined(USE_ANSI_C) || defined(DLL_EXPORT)

	/* In non-GCC compilers, there is no support for variable-argument
	 * macros, so provide "internal" vararg functions directly instead. */

	int lo_message_add(lo_message msg, const char *types, ...);
	int lo_send(lo_address targ, const char *path, const char *types, ...);
	int lo_send_timestamped(lo_address targ, lo_timetag ts, const char *path, const char *types, ...);
	int lo_send_from(lo_address targ, lo_server from, lo_timetag ts, const char *path, const char *types, ...);

#else // !USE_ANSI_C

#define lo_message_add(msg, types...)                       \
	lo_message_add_internal(msg, __FILE__, __LINE__, types, \
							LO_MARKER_A, LO_MARKER_B)

#define lo_send(targ, path, types...)                       \
	lo_send_internal(targ, __FILE__, __LINE__, path, types, \
					 LO_MARKER_A, LO_MARKER_B)

#define lo_send_timestamped(targ, ts, path, types...)                \
	lo_send_timestamped_internal(targ, __FILE__, __LINE__, ts, path, \
								 types, LO_MARKER_A, LO_MARKER_B)

#define lo_send_from(targ, from, ts, path, types...)                \
	lo_send_from_internal(targ, from, __FILE__, __LINE__, ts, path, \
						  types, LO_MARKER_A, LO_MARKER_B)

#endif // USE_ANSI_C

#ifdef __cplusplus
}
#endif

#endif

#ifndef LO_OSC_TYPES_H
#define LO_OSC_TYPES_H

/**
 * \file lo_osc_types.h A liblo header defining OSC-related types and
 * constants.
 */

#include <stdint.h>

/**
 * \addtogroup liblo
 * @{
 */

/**
 * \brief A structure to store OSC TimeTag values.
 */
typedef struct
{
	/** The number of seconds since Jan 1st 1900 in the UTC timezone. */
	uint32_t sec;
	/** The fractions of a second offset from above, expressed as 1/2^32nds
	 * of a second */
	uint32_t frac;
} lo_timetag;

/**
 * \brief An enumeration of bundle element types liblo can handle.
 *
 * The element of a bundle can either be a message or an other bundle.
 */
typedef enum
{
	/** bundle element is a message */
	LO_ELEMENT_MESSAGE = 1,
	/** bundle element is a bundle */
	LO_ELEMENT_BUNDLE = 2
} lo_element_type;

/**
 * \brief An enumeration of the OSC types liblo can send and receive.
 *
 * The value of the enumeration is the typechar used to tag messages and to
 * specify arguments with lo_send().
 */
typedef enum
{
	/* basic OSC types */
	/** 32 bit signed integer. */
	LO_INT32 = 'i',
	/** 32 bit IEEE-754 float. */
	LO_FLOAT = 'f',
	/** Standard C, NULL terminated string. */
	LO_STRING = 's',
	/** OSC binary blob type. Accessed using the lo_blob_*() functions. */
	LO_BLOB = 'b',

	/* extended OSC types */
	/** 64 bit signed integer. */
	LO_INT64 = 'h',
	/** OSC TimeTag type, represented by the lo_timetag structure. */
	LO_TIMETAG = 't',
	/** 64 bit IEEE-754 double. */
	LO_DOUBLE = 'd',
	/** Standard C, NULL terminated, string. Used in systems which
	 * distinguish strings and symbols. */
	LO_SYMBOL = 'S',
	/** Standard C, 8 bit, char variable. */
	LO_CHAR = 'c',
	/** A 4 byte MIDI packet. */
	LO_MIDI = 'm',
	/** Sybol representing the value True. */
	LO_TRUE = 'T',
	/** Sybol representing the value False. */
	LO_FALSE = 'F',
	/** Sybol representing the value Nil. */
	LO_NIL = 'N',
	/** Sybol representing the value Infinitum. */
	LO_INFINITUM = 'I'
} lo_type;

/**
 * \brief Union used to read values from incoming messages.
 *
 * Types can generally be read using argv[n]->t where n is the argument number
 * and t is the type character, with the exception of strings and symbols which
 * must be read with &argv[n]->t.
 */
typedef union
{
	/** 32 bit signed integer. */
	int32_t i;
	/** 32 bit signed integer. */
	int32_t i32;
	/** 64 bit signed integer. */
	int64_t h;
	/** 64 bit signed integer. */
	int64_t i64;
	/** 32 bit IEEE-754 float. */
	float f;
	/** 32 bit IEEE-754 float. */
	float f32;
	/** 64 bit IEEE-754 double. */
	double d;
	/** 64 bit IEEE-754 double. */
	double f64;
	/** Standard C, NULL terminated string. */
	char s;
	/** Standard C, NULL terminated, string. Used in systems which
	 * distinguish strings and symbols. */
	char S;
	/** Standard C, 8 bit, char. */
	unsigned char c;
	/** A 4 byte MIDI packet. */
	uint8_t m[4];
	/** OSC TimeTag value. */
	lo_timetag t;
	/** Blob **/
	struct
	{
		int32_t size;
		char data;
	} blob;
} lo_arg;

/* Note: No struct literals in MSVC */
#ifdef _MSC_VER
#ifndef USE_ANSI_C
#define USE_ANSI_C
#endif
#endif

/** \brief A timetag constant representing "now". */
#if defined(USE_ANSI_C) || defined(DLL_EXPORT)
lo_timetag lo_get_tt_immediate();
#define LO_TT_IMMEDIATE lo_get_tt_immediate()
#else // !USE_ANSI_C
#define LO_TT_IMMEDIATE ((lo_timetag){0U, 1U})
#endif // USE_ANSI_C

/** @} */

#endif

#ifndef LO_SERVERTHREAD_H
#define LO_SERVERTHREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * \file lo_serverthread.h The liblo headerfile declaring thread-related functions.
	 */

	/**
	 * \brief Create a new server thread to handle incoming OSC
	 * messages.
	 *
	 * Server threads take care of the message reception and dispatch by
	 * transparently creating a system thread to handle incoming messages.
	 * Use this if you do not want to handle the threading yourself.
	 *
	 * \param port If NULL is passed then an unused port will be chosen by the
	 * system, its number may be retrieved with lo_server_thread_get_port()
	 * so it can be passed to clients. Otherwise a decimal port number, service
	 * name or UNIX domain socket path may be passed.
	 * \param err_h A function that will be called in the event of an error being
	 * raised. The function prototype is defined in lo_types.h
	 */
	lo_server_thread lo_server_thread_new(const char *port, lo_err_handler err_h);

	/**
	 * \brief Create a new server thread to handle incoming OSC
	 * messages, and join a UDP multicast group.
	 *
	 * Server threads take care of the message reception and dispatch by
	 * transparently creating a system thread to handle incoming messages.
	 * Use this if you do not want to handle the threading yourself.
	 *
	 * \param group The multicast group to join.  See documentation on IP
	 * multicast for the acceptable address range; e.g., http://tldp.org/HOWTO/Multicast-HOWTO-2.html
	 * \param port If NULL is passed then an unused port will be chosen by the
	 * system, its number may be retrieved with lo_server_thread_get_port()
	 * so it can be passed to clients. Otherwise a decimal port number, service
	 * name or UNIX domain socket path may be passed.
	 * \param err_h A function that will be called in the event of an error being
	 * raised. The function prototype is defined in lo_types.h
	 */
	lo_server_thread lo_server_thread_new_multicast(const char *group, const char *port,
													lo_err_handler err_h);

	/**
	 * \brief Create a new server thread instance, and join a UDP
	 * multicast group, optionally specifying which network interface to
	 * use.  Note that usually only one of iface or ip are specified.
	 *
	 * \param group The multicast group to join.  See documentation on IP
	 * multicast for the acceptable address range; e.g., http://tldp.org/HOWTO/Multicast-HOWTO-2.html
	 * \param port If using UDP then NULL may be passed to find an unused port.
	 * Otherwise a decimal port number or service name or may be passed.
	 * If using UNIX domain sockets then a socket path should be passed here.
	 * \param iface A string specifying the name of a network interface to
	 * use, or zero if not specified.
	 * \param ip A string specifying the IP address of a network interface
	 * to use, or zero if not specified.
	 * \param err_h An error callback function that will be called if there is an
	 * error in messge reception or server creation. Pass NULL if you do not want
	 * error handling.
	 */
	lo_server_thread lo_server_thread_new_multicast_iface(const char *group, const char *port,
														  const char *iface, const char *ip,
														  lo_err_handler err_h);

	/**
	 * \brief Create a new server thread to handle incoming OSC
	 * messages, specifying protocol.
	 *
	 * Server threads take care of the message reception and dispatch by
	 * transparently creating a system thread to handle incoming messages.
	 * Use this if you do not want to handle the threading yourself.
	 *
	 * \param port If NULL is passed then an unused port will be chosen by the
	 * system, its number may be retrieved with lo_server_thread_get_port()
	 * so it can be passed to clients. Otherwise a decimal port number, service
	 * name or UNIX domain socket path may be passed.
	 * \param proto The protocol to use, should be one of LO_UDP, LO_TCP or LO_UNIX.
	 * \param err_h A function that will be called in the event of an error being
	 * raised. The function prototype is defined in lo_types.h
	 */
	lo_server_thread lo_server_thread_new_with_proto(const char *port, int proto,
													 lo_err_handler err_h);

	/**
	 * \brief Create a new server thread, taking port and the optional
	 * multicast group IP from an URL string.
	 *
	 * \param url The URL to specify the server parameters.
	 * \param err_h An error callback function that will be called if there is an
	 * error in messge reception or server creation. Pass NULL if you do not want
	 * error handling.
	 * \return A new lo_server_thread instance.
	 */
	lo_server_thread lo_server_thread_new_from_url(const char *url,
												   lo_err_handler err_h);

	/**
	 * \brief Create a new server thread, using a configuration struct.
	 *
	 * \param config A pre-initialized config struct.  A pointer to it will not be kept.
	 * \return A new lo_server_thread instance.
	 */
	lo_server_thread lo_server_thread_new_from_config(lo_server_config *config);

	/**
	 * \brief Free memory taken by a server thread
	 *
	 * Frees the memory, and, if currently running will stop the associated thread.
	 */
	void lo_server_thread_free(lo_server_thread st);

	/**
	 * \brief Add an OSC method to the specifed server thread.
	 *
	 * \param st The server thread the method is to be added to.
	 * \param path The OSC path to register the method to. If NULL is passed the
	 * method will match all paths.
	 * \param typespec The typespec the method accepts. Incoming messages with
	 * similar typespecs (e.g. ones with numerical types in the same position) will
	 * be coerced to the typespec given here.
	 * \param h The method handler callback function that will be called it a
	 * matching message is received
	 * \param user_data A value that will be passed to the callback function, h,
	 * when its invoked matching from this method.
	 */
	lo_method lo_server_thread_add_method(lo_server_thread st, const char *path,
										  const char *typespec, lo_method_handler h,
										  const void *user_data);

	/**
	 * \brief Delete an OSC method from the specifed server thread.
	 *
	 * \param st The server thread the method is to be removed from.
	 * \param path The OSC path of the method to delete. If NULL is passed the
	 * method will match the generic handler.
	 * \param typespec The typespec the method accepts.
	 */
	void lo_server_thread_del_method(lo_server_thread st, const char *path,
									 const char *typespec);

	/**
	 * \brief Delete an OSC method from the specified server thread.
	 *
	 * \param st The server thread the method is to be removed from.
	 * \param m The lo_method identifier returned from lo_server_add_method for
	 *          the method to delete from the server.
	 * \return Non-zero if it was not found in the list of methods for the server.
	 */
	int lo_server_thread_del_lo_method(lo_server_thread st, lo_method m);

	/**
	 * \brief Set an init and/or a cleanup function to the specifed server thread.
	 *
	 * To have any effect, it must be called before the server thread is started.
	 *
	 * \param st The server thread to which the method is to be added.
	 * \param init The init function to be called just after thread start.
	 *             May be NULL.
	 * \param cleanup The cleanup function to be called just before thread
	 *                exit.  May be NULL.
	 * \param user_data A value that will be passed to the callback functions.
	 */
	void lo_server_thread_set_callbacks(lo_server_thread st,
										lo_server_thread_init_callback init,
										lo_server_thread_cleanup_callback cleanup,
										void *user_data);

	/**
	 * \brief Start the server thread
	 *
	 * \param st the server thread to start.
	 * \return Less than 0 on failure, 0 on success.
	 */
	int lo_server_thread_start(lo_server_thread st);

	/**
	 * \brief Stop the server thread
	 *
	 * \param st the server thread to start.
	 * \return Less than 0 on failure, 0 on success.
	 */
	int lo_server_thread_stop(lo_server_thread st);

	/**
	 * \brief Return the port number that the server thread has bound to.
	 */
	int lo_server_thread_get_port(lo_server_thread st);

	/**
	 * \brief Return a URL describing the address of the server thread.
	 *
	 * Return value must be free()'d to reclaim memory.
	 */
	char *lo_server_thread_get_url(lo_server_thread st);

	/**
	 * \brief Return the lo_server for a lo_server_thread
	 *
	 * This function is useful for passing a thread's lo_server
	 * to lo_send_from().
	 */
	lo_server lo_server_thread_get_server(lo_server_thread st);

	/** \brief Return true if there are scheduled events (eg. from bundles) waiting
	 * to be dispatched by the thread */
	int lo_server_thread_events_pending(lo_server_thread st);

	void lo_server_thread_set_error_context(lo_server_thread st, void *user_data);

	/** \brief Pretty-print a lo_server_thread object. */
	void lo_server_thread_pp(lo_server_thread st);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LO_THROW_H
#define LO_THROW_H

#ifdef __cplusplus
extern "C"
{
#endif

	void lo_throw(lo_server s, int errnum, const char *message, const char *path);

	/*! Since the liblo error handler does not provide a context pointer,
	 *  it can be provided by associating it with a particular server
	 *  through this thread-safe API. */

	void *lo_error_get_context(void);

	void lo_server_set_error_context(lo_server s, void *user_data);

#ifdef __cplusplus
}
#endif

#endif

#ifndef LO_TYPES_H
#define LO_TYPES_H

/**
 * \file lo_types.h The liblo headerfile defining types used by this API.
 */

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#include "lo/lo_osc_types.h"

#define LO_DISABLE 0 //!< Disable a boolean option.
#define LO_ENABLE 1	 //!< Enable a boolean option.

	/**
	 * \brief A reference to an OSC service.
	 *
	 * Created by calls to lo_address_new() or lo_address_new_from_url().
	 */
	typedef struct lo_address_ *lo_address;

	/**
	 * \brief A object to store an opaque binary data object.
	 *
	 * Can be passed over OSC using the 'b' type. Created by calls to lo_blob_new().
	 */
	typedef struct lo_blob_ *lo_blob;

	/**
	 * \brief A low-level object used to represent messages passed over OSC.
	 *
	 * Created by calls to lo_message_new(), arguments can be added with calls to
	 * lo_message_add_*().
	 */
	typedef struct lo_message_ *lo_message;

	/**
	 * \brief A low-level object used to represent bundles of messages passed over
	 * OSC.
	 *
	 * Created by calls to lo_bundle_new(), messages can be added with calls to
	 * lo_bundle_add_message().
	 */
	typedef struct lo_bundle_ *lo_bundle;

	/**
	 * \brief An object representing an method on a server.
	 *
	 * Returned by calls to lo_server_thread_add_method() and
	 * lo_server_add_method().
	 */
	typedef struct lo_method_ *lo_method;

	/**
	 * \brief An object representing an instance of an OSC server.
	 *
	 * Created by calls to lo_server_new(). If you wish to have the server
	 * operate in a background thread, use lo_server_thread instead.
	 */
	typedef struct lo_server_ *lo_server;

	/**
	 * \brief An object representing a thread containing an OSC server.
	 *
	 * Created by calls to lo_server_thread_new().
	 */
	typedef struct lo_server_thread_ *lo_server_thread;

	/**
	 * \brief A callback function to receive notification of an error in a server or
	 * server thread.
	 *
	 * On callback the parameters will be set to the following values:
	 *
	 * \param num An error number that can be used to identify this condition.
	 * \param msg An error message describing the condidtion.
	 * \param where A string describing the place the error occured - typically
	 * either a function call or method path.
	 */
	typedef void (*lo_err_handler)(int num, const char *msg, const char *where);

	/**
	 * \brief A callback function to receive notification of matching message
	 * arriving in the server or server thread.
	 *
	 * The return value tells the method dispatcher whether this handler
	 * has dealt with the message correctly: a return value of 0 indicates
	 * that it has been handled, and it should not attempt to pass it on
	 * to any other handlers, non-0 means that it has not been handled and
	 * the dispatcher will attempt to find more handlers that match the
	 * path and types of the incoming message.
	 *
	 * On callback the paramters will be set to the following values:
	 *
	 * \param path That path that the incoming message was sent to
	 * \param types If you specided types in your method creation call then this
	 * will match those and the incoming types will have been coerced to match,
	 * otherwise it will be the types of the arguments of the incoming message.
	 * \param argv An array of lo_arg types containing the values, e.g. if the
	 * first argument of the incoming message is of type 'f' then the value will be
	 * found in argv[0]->f.
	 * \param argc The number of arguments received.
	 * \param msg A structure containing the original raw message as received. No
	 * type coercion will have occured and the data will be in OSC byte order
	 * (bigendian).
	 * \param user_data This contains the user_data value passed in the call to
	 * lo_server_thread_add_method.
	 */
	typedef int (*lo_method_handler)(const char *path, const char *types,
									 lo_arg **argv, int argc, lo_message msg,
									 void *user_data);

	/**
	 * \brief A callback function to receive notification of a bundle being
	 * dispatched by the server or server thread.
	 *
	 * This callback allows applications to be aware of incoming bundles
	 * and preserve ordering and atomicity of messages in bundles.
	 *
	 * If installed with lo_server_add_bundle_handlers, this callback will be
	 * called with \a time set to the time tag of the bundle, and \a user_data
	 * set to the user_data parameter passed to lo_server_add_bundle_handlers.
	 *
	 * Note that bundles may be nested, in which case calls to the bundle start
	 * and end handlers will also be nested.  The application can keep track of
	 * nested bundles in a stack-like manner by treating the start handler as
	 * "push" and the end handler as "pop".  For example, a bundle containing two
	 * bundles would fire 6 callbacks: begin, begin, end, begin, end, end.
	 */
	typedef int (*lo_bundle_start_handler)(lo_timetag time, void *user_data);

	/**
	 * \brief A callback function to receive notification of a bundle dispatch
	 * being completed by the server or server thread.
	 *
	 * If installed with lo_server_add_bundle_handlers, this callback will be
	 * called after all the messages of a bundle have been dispatched with
	 * \a user_data set to the user_data parameter passed to
	 * lo_server_add_bundle_handlers.
	 */
	typedef int (*lo_bundle_end_handler)(void *user_data);

	/**
	 * \brief A callback function to perform initialization when the
	 * server thread is started.
	 *
	 * If installed with lo_server_thread_set_callbacks, this callback
	 * will be called in the server thread, just before the server starts
	 * listening for events. \a user_data is set to the user_data
	 * parameter passed to lo_server_thread_add_functions.
	 *
	 * If the return value is non-zero, the thread start will be aborted.
	 */
	typedef int (*lo_server_thread_init_callback)(lo_server_thread s,
												  void *user_data);

	/**
	 * \brief A callback function to perform cleanup when the server
	 * thread is started.
	 *
	 * If installed with lo_server_thread_set_callbacks, this callback
	 * will be called in the server thread, after the server have stopped
	 * listening and processing events, before it quits. \a user_data is
	 * set to the user_data parameter passed to
	 * lo_server_thread_add_functions.
	 */
	typedef void (*lo_server_thread_cleanup_callback)(lo_server_thread s,
													  void *user_data);

#ifdef __cplusplus
}
#endif

#endif
#ifndef LO_TYPES_H
#define LO_TYPES_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/types.h>
#include <sys/socket.h>
#endif

#ifdef HAVE_POLL
#include <poll.h>
#endif

#if defined(WIN32) || defined(_MSC_VER)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#ifndef ESP_PLATFORM
#define closesocket close
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#ifdef _MSC_VER
typedef SSIZE_T ssize_t;
#endif

#if !defined(UINT_PTR) && !defined(WIN32)
#ifdef HAVE_UINTPTR_T
#include <stdint.h>
#define UINT_PTR uintptr_t
#else
#define UINT_PTR unsigned long
#endif
#endif

#ifdef ENABLE_THREADS
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif
#endif

#include "lo/lo_osc_types.h"

#define LO_HOST_SIZE 1024

/** \brief Bitflags for optional protocol features, set by
 *         lo_address_set_flags(). */
typedef enum
{
	LO_SLIP = 0x01,			/*!< SLIP decoding */
	LO_NODELAY = 0x02,		/*!< Set the TCP_NODELAY socket option. */
	LO_SLIP_DBL_END = 0x04, /*!< Set SLIP encoding to double-END. */
} lo_proto_flags;

/** \brief Bitflags for optional server features. */
typedef enum
{
	LO_SERVER_COERCE = 0x01,  /*!< whether or not to coerce args
							   * during dispatch */
	LO_SERVER_ENQUEUE = 0x02, /*!< whether or not to enqueue early
							   * messages */
} lo_server_flags;

#define LO_SERVER_DEFAULT_FLAGS (LO_SERVER_COERCE | LO_SERVER_ENQUEUE)

typedef void (*lo_err_handler)(int num, const char *msg,
							   const char *path);

struct _lo_method;

typedef struct _lo_inaddr
{
	union
	{
		struct in_addr addr;
		struct in6_addr addr6;
	} a;
	size_t size;
	char *iface;
} *lo_inaddr;

typedef struct _lo_address
{
	char *host;
	int socket;
	int ownsocket;
	char *port;
	int protocol;
	lo_proto_flags flags;
	struct addrinfo *ai;
	struct addrinfo *ai_first;
	int errnum;
	const char *errstr;
	int ttl;
	struct _lo_inaddr addr;
	struct _lo_server *source_server;
	const char *source_path; /* does not need to be freed since it
							  * will always point to stack memory in
							  * dispatch_method() */
} *lo_address;

typedef struct _lo_blob
{
	uint32_t size;
	char *data;
} *lo_blob;

typedef struct _lo_message
{
	char *types;
	size_t typelen;
	size_t typesize;
	void *data;
	size_t datalen;
	size_t datasize;
	lo_address source;
	lo_arg **argv;
	/* timestamp from bundle (LO_TT_IMMEDIATE for unbundled messages) */
	lo_timetag ts;
	int refcount;
} *lo_message;

typedef int (*lo_method_handler)(const char *path, const char *types,
								 lo_arg **argv, int argc,
								 struct _lo_message *msg,
								 void *user_data);

typedef int (*lo_bundle_start_handler)(lo_timetag time, void *user_data);
typedef int (*lo_bundle_end_handler)(void *user_data);

typedef struct _lo_method
{
	const char *path;
	const char *typespec;
	int has_pattern;
	lo_method_handler handler;
	char *user_data;
	struct _lo_method *next;
} *lo_method;

struct socket_context
{
	char *buffer;
	size_t buffer_size;
	unsigned int buffer_msg_offset;
	unsigned int buffer_read_offset;
	int is_slip;	/*!< 1 if slip mode, 0 otherwise, -1 for unknown */
	int slip_state; /*!< state variable for slip decoding */
};

#ifdef HAVE_POLL
typedef struct pollfd lo_server_fd_type;
#else
typedef struct
{
	int fd;
	int revents;
} lo_server_fd_type;
#define POLLIN 0x001
#endif

typedef struct _lo_server
{
	struct addrinfo *ai;
	lo_method first;
	lo_err_handler err_h;
	int port;
	char *hostname;
	char *path;
	int protocol;
	lo_server_flags flags;
	void *queued;
	struct sockaddr_storage addr;
	socklen_t addr_len;
	int sockets_len;
	int sockets_alloc;
	lo_server_fd_type *sockets;

	// Some extra data needed per open socket.  Note that we don't put
	// it in the socket struct, because that layout is needed for
	// passing in the list of sockets to poll().
	struct socket_context *contexts;

	struct _lo_address *sources;
	int sources_len;
	lo_bundle_start_handler bundle_start_handler;
	lo_bundle_end_handler bundle_end_handler;
	void *bundle_handler_user_data;
	struct _lo_inaddr addr_if;
	void *error_user_data;
	int max_msg_size;
} *lo_server;

#ifdef ENABLE_THREADS
struct _lo_server_thread;
typedef int (*lo_server_thread_init_callback)(struct _lo_server_thread *s,
											  void *user_data);
typedef void (*lo_server_thread_cleanup_callback)(struct _lo_server_thread *s,
												  void *user_data);
typedef struct _lo_server_thread
{
	lo_server s;
#ifdef HAVE_LIBPTHREAD
	pthread_t thread;
#else
#ifdef HAVE_WIN32_THREADS
	HANDLE thread;
#endif
#endif
	volatile int active;
	volatile int done;
	lo_server_thread_init_callback cb_init;
	lo_server_thread_cleanup_callback cb_cleanup;
	void *user_data;
} *lo_server_thread;
#else
typedef void *lo_server_thread;
#endif

typedef struct _lo_bundle *lo_bundle;

typedef struct _lo_element
{
	lo_element_type type;
	union
	{
		lo_bundle bundle;
		struct
		{
			lo_message msg;
			const char *path;
		} message;
	} content;
} lo_element;

struct _lo_bundle
{
	size_t size;
	size_t len;
	lo_timetag ts;
	lo_element *elmnts;
	int refcount;
};

typedef struct _lo_strlist
{
	char *str;
	struct _lo_strlist *next;
} lo_strlist;

typedef union
{
	int32_t i;
	float f;
	char c;
	uint32_t nl;
} lo_pcast32;

typedef union
{
	int64_t i;
	double f;
	uint64_t nl;
} lo_pcast64;

extern struct lo_cs
{
	int udp;
	int tcp;
} lo_client_sockets;

#endif
