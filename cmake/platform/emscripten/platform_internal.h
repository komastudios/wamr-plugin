/*
 * platform_internal.h — Emscripten platform definitions for WAMR.
 *
 * Modelled after the linux platform_internal.h but avoids headers and
 * features that are unavailable or conflict under Emscripten (WASI type
 * redefinitions, advanced signal handling, /proc, etc.).
 */

#ifndef _PLATFORM_INTERNAL_H
#define _PLATFORM_INTERNAL_H

/* Skip WAMR's platform_wasi_types.h — Emscripten's sysroot already provides
 * the __wasi_* typedefs and they have different base types (unsigned long vs
 * uint32_t).  Pre-defining the include guard avoids the redefinition errors. */
#define _PLATFORM_WASI_TYPES_H

#include <inttypes.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BH_PLATFORM_EMSCRIPTEN
#define BH_PLATFORM_EMSCRIPTEN
#endif

/* Also define BH_PLATFORM_LINUX so that WAMR's POSIX code paths compile. */
#ifndef BH_PLATFORM_LINUX
#define BH_PLATFORM_LINUX
#endif

/* Stack size of applet threads's native part. */
#define BH_APPLET_PRESERVED_STACK_SIZE (32 * 1024)

/* Default thread priority */
#define BH_THREAD_DEFAULT_PRIORITY 0

typedef pthread_t korp_tid;
typedef pthread_mutex_t korp_mutex;
typedef pthread_cond_t korp_cond;
typedef pthread_t korp_thread;
typedef pthread_rwlock_t korp_rwlock;
typedef sem_t korp_sem;

#define OS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#define os_thread_local_attribute __thread

#define bh_socket_t int

/* No hardware bound check on Emscripten (no signal-based fault handling). */
/* OS_ENABLE_HW_BOUND_CHECK is intentionally NOT defined. */

/* No wakeup-blocking-op support (no pthread_kill / SIGUSR1). */
/* OS_ENABLE_WAKEUP_BLOCKING_OP is intentionally NOT defined. */

#define os_getpagesize getpagesize

typedef int os_file_handle;
typedef DIR *os_dir_stream;
typedef int os_raw_file_handle;

static inline os_file_handle
os_get_invalid_handle(void)
{
    return -1;
}

#ifdef __cplusplus
}
#endif

#endif /* end of _PLATFORM_INTERNAL_H */
