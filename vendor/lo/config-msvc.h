/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define this to enable ipv6. */
/* #undef ENABLE_IPV6 */

/* Define this to enable network tests. */
#define ENABLE_NETWORK_TESTS 1

/* Define this to enable threads. */
#cmakedefine ENABLE_THREADS

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if inet_aton() is available. */
/* #undef HAVE_INET_ATON */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `pthread' library (-lpthread). */
#cmakedefine HAVE_LIBPTHREAD

/* Define to 1 if you want to use `win32' threads. */
#cmakedefine HAVE_WIN32_THREADS

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netdb.h> header file. */
/* #undef HAVE_NETDB_H */

/* Define to 1 if you have the <netinet/in.h> header file. */
/* #undef HAVE_NETINET_IN_H */

/* Define to 1 if poll() is available. */
/* #undef HAVE_POLL */

/* Define to 1 if select() is available. */
#define HAVE_SELECT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #undef HAVE_SYS_SOCKET_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* If machine is bigendian */
#define LO_BIGENDIAN "0"

/* Libtool compatibility version */
#define LO_SO_VERSION @LO_SO_VERSION @

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Name of package */
#define PACKAGE "liblo"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "liblo-devel@lists.sourceforge.net"

/* Define to the full name of this package. */
#define PACKAGE_NAME "liblo"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "liblo " \
					   "@VERSION@"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "liblo"

/* Define to the version of this package. */
#define PACKAGE_VERSION "@VERSION@"

/* printf code for type long long int */
#define PRINTF_LL "@PRINTF_LL@"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "@VERSION@"

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
