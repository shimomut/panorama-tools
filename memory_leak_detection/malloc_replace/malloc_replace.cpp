#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <execinfo.h>
#include <gnu/lib-names.h>
#include <pthread.h>

#include <cstdlib>
#include <thread>
#include <mutex>

#include "Python.h"

//-----

#define REPLACE_MALLOC_FREE
#define USE_MALLOC_HISTORY
#define USE_LOCK_IN_MALLOC
#define USE_CHECK_POINT_HISTORY
#define USE_BUILTIN_RETURN_ADDR // backtrace() sometimes doesn't return. Use __builtin_return_address instead.

static const size_t MALLOC_CALL_HISTORY_SIZE = 100000;
static const size_t NUM_RETURN_ADDR_LEVELS = 1; // This configuration has big impact on the performance.
static const size_t PRELIMINARY_HEAP_SIZE = 10 * 1024 * 1024;
static const size_t NUM_CHECK_POINT_HISTORY = 50;

//-----

static void my_printf( const char * fmt, ... )
{
    char buf[1024];

    va_list args;
    va_start(args, fmt);

    int len = vsnprintf( buf, sizeof(buf)-1, fmt, args );

    va_end(args);

    ssize_t result = write( STDOUT_FILENO, buf, len );
    (void)result;
}

//-----

static char preliminary_heap_buffer[PRELIMINARY_HEAP_SIZE];

typedef void * mspace;
extern "C" mspace create_mspace_with_base(void* base, size_t capacity, int locked);
extern "C" void* mspace_malloc(mspace msp, size_t bytes);
extern "C" void* mspace_memalign(mspace msp, size_t alignment, size_t bytes);
extern "C" void* mspace_calloc(mspace msp, size_t n, size_t bytes);
extern "C" void* mspace_realloc(mspace msp, void* mem, size_t newsize);
extern "C" void mspace_free(mspace msp, void* mem);

static mspace g_msp;

static void preliminary_heap_init_once()
{
    if(!g_msp)
    {
        g_msp = create_mspace_with_base( preliminary_heap_buffer, sizeof(preliminary_heap_buffer), 1 );
    }
}

static inline bool preliminary_heap_in_range( void * p )
{
    return ( preliminary_heap_buffer<=p && p<preliminary_heap_buffer + sizeof(preliminary_heap_buffer) );
}

//-----

struct CheckPoint
{
    CheckPoint( const char * _filename=NULL, const char * _funcname=NULL, int _lineno=0, pthread_t _thread_id=0 )
        :
        filename(_filename),
        funcname(_funcname),
        lineno(_lineno),
        thread_id(_thread_id)
    {
    }

    const char * filename;
    const char * funcname;
    int lineno;
    pthread_t thread_id;
};

struct CheckPointHistory
{
    CheckPointHistory()
        :
        next_index(0)
    {
    }

    void Check( const char * _filename, const char * _funcname, int _lineno )
    {
        std::lock_guard<std::recursive_mutex> lock(mtx);

        check_points[next_index] = CheckPoint( _filename, _funcname, _lineno, pthread_self() );
        next_index = (next_index+1) % NUM_CHECK_POINT_HISTORY;
    }

    void Print()
    {
        my_printf("Check points:\n");

        std::lock_guard<std::recursive_mutex> lock(mtx);

        for( size_t i=0 ; i<NUM_CHECK_POINT_HISTORY ; ++i )
        {
            int index = (next_index+i) % NUM_CHECK_POINT_HISTORY;
            if( check_points[index].filename )
            {
                my_printf("%s - %s - %d - %p\n", check_points[index].filename, check_points[index].funcname, check_points[index].lineno, check_points[index].thread_id );
            }
        }
    }

    std::recursive_mutex mtx;

    CheckPoint check_points[NUM_CHECK_POINT_HISTORY];
    int next_index;
};

static CheckPointHistory check_point_history;

#if defined(USE_CHECK_POINT_HISTORY)
#define CHECK_POINT() check_point_history.Check(__FILE__,__func__,__LINE__)
#else //defined(USE_CHECK_POINT_HISTORY)
#define CHECK_POINT() ((void)0)
#endif //defined(USE_CHECK_POINT_HISTORY)

//-----

typedef void * (*MallocFunc)(size_t);
typedef void * (*MemalignFunc)(size_t,size_t);
typedef void * (*CallocFunc)(size_t,size_t);
typedef void * (*ReallocFunc)(void*,size_t);
typedef void (*FreeFunc)(void*);
typedef int (*PosixMemalignFunc)(void**,size_t,size_t);
typedef void * (*AlignedAllocFunc)(size_t,size_t);

static MallocFunc p_malloc;
static MemalignFunc p_memalign;
static CallocFunc p_calloc;
static ReallocFunc p_realloc;
static FreeFunc p_free;
static PosixMemalignFunc p_posix_memalign;
static AlignedAllocFunc p_aligned_alloc;


enum MallocOperation
{
    MallocOperation_Alloc = 1,
    MallocOperation_Free = 2,
    MallocOperation_Realloc = 3
};

struct MallocCallHistory
{
    MallocOperation op;
    void * p;
    void * p2;
    size_t size;
    void * return_addr[NUM_RETURN_ADDR_LEVELS];
};

struct Globals
{
    Globals()
        :
        enabled(false),
        malloc_call_history_size(0)
    {
        memset( malloc_call_history, 0, sizeof(malloc_call_history) );
    }

    std::recursive_mutex mtx;

    bool enabled;

    MallocCallHistory malloc_call_history[MALLOC_CALL_HISTORY_SIZE];
    size_t malloc_call_history_size;

    std::string output_filename;
};

static Globals g;

// ---

static void flush_malloc_call_history()
{
    CHECK_POINT();
    
    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    if( g.malloc_call_history_size > 0 )
    {
        CHECK_POINT();
    
        int fd = open( g.output_filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644 );

        for( size_t i=0 ; i<g.malloc_call_history_size ; ++i )
        {
            CHECK_POINT();

            char buf[1024];
            int len;

            {
                len = snprintf( buf, sizeof(buf)-1, "{\"op\":%d,\"p\":\"%p\",\"p2\":\"%p\",\"size\":%zd,\"return_addr\":[", 
                    g.malloc_call_history[i].op,
                    g.malloc_call_history[i].p,
                    g.malloc_call_history[i].p2,
                    g.malloc_call_history[i].size );

                ssize_t result = write( fd, buf, len );
                (void)result;
            }

            for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
            {
                CHECK_POINT();
                
                const char * format;
                if( level<NUM_RETURN_ADDR_LEVELS-1 )
                {
                    format = "\"%p\",";
                }
                else
                {
                    format = "\"%p\"";
                }
                len = snprintf( buf, sizeof(buf)-1, format, g.malloc_call_history[i].return_addr[level] );
                
                ssize_t result = write( fd, buf, len );
                (void)result;
            }

            {
                const char msg[] = "]}\n";
                
                ssize_t result = write( fd, msg, sizeof(msg)-1 );
                (void)result;
            }
        }

        CHECK_POINT();

        close(fd);

        g.malloc_call_history_size = 0;
    }

    CHECK_POINT();
}

static inline void add_malloc_call_history( MallocOperation op, void * p, void * p2, size_t size )
{
    if(!g.enabled)
    {
        return;
    }

    if( g.malloc_call_history_size==MALLOC_CALL_HISTORY_SIZE )
    {
        flush_malloc_call_history();
    }

    g.malloc_call_history[g.malloc_call_history_size].op = op;
    g.malloc_call_history[g.malloc_call_history_size].p = p;
    g.malloc_call_history[g.malloc_call_history_size].p2 = p2;
    g.malloc_call_history[g.malloc_call_history_size].size = size;

    if(NUM_RETURN_ADDR_LEVELS>0)
    {
        CHECK_POINT();

        #if defined(USE_BUILTIN_RETURN_ADDR)
        for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
        {
            g.malloc_call_history[g.malloc_call_history_size].return_addr[level] = __builtin_return_address(0);
        }
        #else //defined(USE_BUILTIN_RETURN_ADDR)
        void * bt[NUM_RETURN_ADDR_LEVELS+1] = {0};
        backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
        for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
        {
            g.malloc_call_history[g.malloc_call_history_size].return_addr[level] = bt[level+1];
        }
        #endif //defined(USE_BUILTIN_RETURN_ADDR)
    }

    g.malloc_call_history_size ++;
}

#if defined(USE_MALLOC_HISTORY)
#define ADD_MALLOC_CALL_HISTORY(op,p,p2,size) add_malloc_call_history(op,p,p2,size)
#else //defined(USE_MALLOC_HISTORY)
#define ADD_MALLOC_CALL_HISTORY(op,p,p2,size) (void)0
#endif //defined(USE_MALLOC_HISTORY)

static void get_glibc_malloc()
{
    void * libc = dlopen(LIBC_SO, RTLD_LAZY);
    if(libc)
    {
        p_malloc = (MallocFunc)dlsym( libc, "malloc");
        p_memalign = (MemalignFunc)dlsym( libc, "memalign");
        p_calloc = (CallocFunc)dlsym( libc, "calloc");
        p_realloc = (ReallocFunc)dlsym( libc, "realloc");
        p_posix_memalign = (PosixMemalignFunc)dlsym( libc, "posix_memalign");
        p_aligned_alloc = (AlignedAllocFunc)dlsym( libc, "aligned_alloc");
        p_free = (FreeFunc)dlsym( libc, "free");
    }
}

static void malloc_free_trace_start( const char * output_filename )
{
    g.output_filename = output_filename;
    g.enabled = true;
}

static void malloc_free_trace_stop()
{
    g.enabled = false;
    flush_malloc_call_history();
}

// ---

#if defined(REPLACE_MALLOC_FREE)

#if defined(USE_LOCK_IN_MALLOC)
#define LOCK() std::lock_guard<std::recursive_mutex> lock(g.mtx)
#else //defined(USE_LOCK_IN_MALLOC)
#define LOCK() (void)0
#endif //defined(USE_LOCK_IN_MALLOC)

extern "C" void * malloc( size_t size )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "malloc called: size=%d\n", size );

    void * p;

    if(p_malloc)
    {
        p = (*p_malloc)(size);
    }
    else
    {
        preliminary_heap_init_once();
        p = mspace_malloc(g_msp,size);
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * memalign( size_t align, size_t size )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "memalign called: align=%d, size=%d\n", align, size );

    void * p;

    if(p_memalign)
    {
        p = (*p_memalign)( align, size );
    }
    else
    {
        preliminary_heap_init_once();
        p = mspace_memalign( g_msp, align, size );
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * calloc( size_t n, size_t size )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "calloc called: n=%d, size=%d\n", n, size );

    void * p;

    if(p_calloc)
    {
        p = (*p_calloc)( n, size );
    }
    else
    {
        preliminary_heap_init_once();
        p = mspace_calloc( g_msp, n, size );
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * realloc( void * old_p, size_t size )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "realloc called: old_p=%p, size=%d\n", old_p, size );

    void * new_p;

    if(p_realloc)
    {
        if( preliminary_heap_in_range(old_p) )
        {
            new_p = mspace_realloc( g_msp, old_p, size );
        }
        else
        {
            new_p = (*p_realloc)( old_p, size );
        }
    }
    else
    {
        preliminary_heap_init_once();
        new_p = mspace_realloc( g_msp, old_p, size );
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Realloc, old_p, new_p, size );

    CHECK_POINT();

    return new_p;
}

extern "C" int posix_memalign( void **memptr, size_t align, size_t size )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "posix_memalign called: align=%d, size=%d\n", align, size );

    int result;
    if(p_posix_memalign)
    {
        result = (*p_posix_memalign)( memptr, align, size );
    }
    else
    {
        preliminary_heap_init_once();
        *memptr = mspace_memalign( g_msp, align, size );

        if(*memptr)
        {
            result = 0;
        }
        else
        {
            result = ENOMEM;
        }
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, *memptr, NULL, size );

    CHECK_POINT();

    return result;
}

extern "C" void free( void * p )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "free called: p=%p\n", p );

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Free, p, NULL, 0 );

    if( preliminary_heap_in_range(p) )
    {
        CHECK_POINT();

        mspace_free( g_msp, p );
    }
    else
    {
        CHECK_POINT();

        (*p_free)(p);
    }

    CHECK_POINT();
}

extern "C" void * aligned_alloc( size_t align, size_t size )
{
    CHECK_POINT();

    LOCK();

    CHECK_POINT();

    //my_printf( "aligned_alloc called: align=%d, size=%d\n", align, size );

    void * p;

    if(p_aligned_alloc)
    {
        p = (*p_aligned_alloc)( align, size );
    }
    else
    {
        if( size % align == 0 )
        {
            preliminary_heap_init_once();
            p = mspace_memalign( g_msp, align, size );
        }
        else
        {
            p = NULL;
        }
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * valloc( size_t size )
{
    CHECK_POINT();

    my_printf( "valloc called: size=%d\n", size );

    abort();

    return NULL;
}

extern "C" void * pvalloc( size_t size )
{
    CHECK_POINT();

    my_printf( "pvalloc called: size=%d\n", size );

    abort();

    return NULL;
}

extern "C" size_t malloc_usable_size(void *ptr)
{
    CHECK_POINT();

    my_printf( "malloc_usable_size called: p=%p\n", ptr );

    abort();

    return 0;
}

#endif //defined(REPLACE_MALLOC_FREE)

// ---

static void _signal_handler(int sig)
{
    #if defined(USE_CHECK_POINT_HISTORY)
    check_point_history.Print();
    #endif //defined(USE_CHECK_POINT_HISTORY)

    // Be careful to use only signal-safe functions
    // https://man7.org/linux/man-pages/man7/signal-safety.7.html

    // get backtrace in the array
    void *array[30];
    size_t size;
    size = backtrace( array, sizeof(array)/sizeof(array[0]) );

    // print heading error message line
    const char msg[] = "Error: caught signal - ";
    size_t result = write( STDERR_FILENO, msg, sizeof(msg)-1 );
    (void)result;

    const char * signal_name = strsignal(sig);
    result = write( STDERR_FILENO, signal_name, strlen(signal_name) );
    (void)result;

    const char msg2[] = ":\n";
    result = write( STDERR_FILENO, msg2, sizeof(msg2)-1 );
    (void)result;

    // print backtrace to stderr
    backtrace_symbols_fd(array, size, STDERR_FILENO);

    // exit the process with signal-safe version of exit function
    //_exit(1);
}

void install_signal_handler()
{
    int signalnums[] = {
        SIGABRT,
        SIGBUS,
        SIGFPE,
        SIGHUP,
        SIGILL,
        SIGINT,
        SIGKILL,
        SIGSEGV,
        SIGTERM
    };

    for( size_t i=0 ; i<sizeof(signalnums)/sizeof(signalnums[0]) ; ++i  )
    {
        signal( signalnums[i], _signal_handler);
    }
}

// ---

void test()
{
    for( int i=0 ; i<10000 ; ++i )
    {
        {
            void * p = malloc(100);
            if(!p){abort();}
            free(p);
        }

        {
            void * p = memalign(64,100);
            if(!p){abort();}
            free(p);
        }

        {
            void * p = calloc(10,100);
            if(!p){abort();}
            free(p);
        }

        {
            void * p = malloc(100);
            if(!p){abort();}
            void * p2 = realloc(p,200);
            if(!p2){abort();}
            free(p2);
        }

        {
            void * p1 = malloc(100);
            if(!p1){abort();}
            void * p2 = malloc(100);
            if(!p2){abort();}

            // intentionally cause memory leak
            p1 = NULL;
            
            free(p1);
            free(p2);
        }

        {
            void * p = 0;
            int result = posix_memalign( &p, 128, 1024 );
            (void)result;
            if(!p){abort();}
            free(p);
        }
    }
}

int main( int argc, const char * argv[] )
{
    int result = 0;

    // Use backtrace to implicitly initialize libgcc.
    // Otherwise, backtrace() calls cause recursive malloc calls.
    // Is there better solution?
    {
        void * bt[30];
        backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
    }

    // Get glibc's malloc/free functions as pointers
    get_glibc_malloc();

    // Install signal handler for troubleshooting
    install_signal_handler();

    // Start tracing malloc/free calls
    malloc_free_trace_start("./malloc_trace.log");

    if(false)
    {
        test();
    }

    // Run python
    if(true)
    {
        wchar_t * wargv[100];
        for( int i=0 ; i<argc ; ++i )
        {
            wargv[i] = Py_DecodeLocale( argv[i], NULL );
        }

        Py_Initialize();

        result = Py_Main(argc, wargv);

        Py_Finalize();

        for( int i=0 ; i<argc ; ++i )
        {
            PyMem_RawFree(wargv[i]);
        }
    }

    // Stop tracing malloc/free calls, and flush the history
    malloc_free_trace_stop();

    #if defined(USE_CHECK_POINT_HISTORY)
    check_point_history.Print();
    #endif //defined(USE_CHECK_POINT_HISTORY)

    return result;
}
