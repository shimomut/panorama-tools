#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <cstdlib>
#include <thread>
#include <mutex>

#include "Python.h"

//-----

#define REPLACE_MALLOC_FUNCTIONS
#define USE_MALLOC_HISTORY
#define USE_BUILTIN_RETURN_ADDR // backtrace() sometimes doesn't return. Use __builtin_return_address instead.

#define TRACE_LOG_DIRNAME "/tmp/"
//#define TRACE_LOG_DIRNAME "./"

static const size_t NUM_RETURN_ADDR_LEVELS = 1; // This configuration has big impact on the performance.

//-----

// printf like function which doesn't use malloc
static void malloc_trace_printf( const char * fmt, ... )
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
        fd(-1)
    {
    }

    bool enabled;
    std::string output_filename;
    int fd;
};

static Globals g;

// ---

static inline int format_malloc_call_history( char * buf, int bufsize, const MallocCallHistory & entry )
{
    char * p = buf;
    bufsize -= 1;
    int len;

    len = snprintf( p, bufsize, "{\"op\":%d,\"p\":\"%p\",\"p2\":\"%p\",\"size\":%zd,\"return_addr\":[", 
        entry.op,
        entry.p,
        entry.p2,
        entry.size );
    p += len;
    bufsize -= len;

    for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
    {
        const char * format;
        if( level<NUM_RETURN_ADDR_LEVELS-1 )
        {
            format = "\"%p\",";
        }
        else
        {
            format = "\"%p\"";
        }
        len = snprintf( p, bufsize, format, entry.return_addr[level] );
        p += len;
        bufsize -= len;
    }

    len = snprintf( p, bufsize, "]}\n" );
    p += len;
    bufsize -= len;

    return (p - buf);
}

static inline void write_malloc_call_history( MallocOperation op, void * p, void * p2, size_t size, void * return_addr )
{
    if(!g.enabled)
    {
        return;
    }

    MallocCallHistory new_entry;

    new_entry.op = op;
    new_entry.p = p;
    new_entry.p2 = p2;
    new_entry.size = size;

    #if defined(USE_BUILTIN_RETURN_ADDR)
    new_entry.return_addr[0] = return_addr;
    #else //defined(USE_BUILTIN_RETURN_ADDR)
    if(NUM_RETURN_ADDR_LEVELS>0)
    {
        void * bt[NUM_RETURN_ADDR_LEVELS+1] = {0};
        backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
        for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
        {
            new_entry.return_addr[level] = bt[level+1];
        }
    }
    #endif //defined(USE_BUILTIN_RETURN_ADDR)

    {
        char buf[1024];
        int len = format_malloc_call_history( buf, sizeof(buf), new_entry );
        ssize_t result = write( g.fd, buf, len );
        (void)result;
    }
}

#if defined(USE_MALLOC_HISTORY)
#define ADD_MALLOC_CALL_HISTORY(op,p,p2,size) write_malloc_call_history(op,p,p2,size,__builtin_return_address(0))
#else //defined(USE_MALLOC_HISTORY)
#define ADD_MALLOC_CALL_HISTORY(op,p,p2,size) (void)0
#endif //defined(USE_MALLOC_HISTORY)

static void malloc_trace_start( const char * output_filename )
{
    g.output_filename = output_filename;
    g.fd = open( g.output_filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644 );

    g.enabled = true;
}

static void malloc_trace_stop()
{
    g.enabled = false;

    close(g.fd);
    g.fd = -1;
}

// ---

#if defined(REPLACE_MALLOC_FUNCTIONS)

extern "C" void* __libc_malloc(size_t);
extern "C" void* __libc_memalign(size_t, size_t);
extern "C" void* __libc_calloc(size_t, size_t);
extern "C" void* __libc_realloc(void*, size_t);
extern "C" void __libc_free(void*);

extern "C" void * malloc( size_t size )
{
    //malloc_trace_printf( "malloc called: size=%d\n", size );

    void * p = __libc_malloc(size);

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    return p;
}

extern "C" void * memalign( size_t align, size_t size )
{
    //malloc_trace_printf( "memalign called: align=%d, size=%d\n", align, size );

    void * p = __libc_memalign( align, size );

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    return p;
}

extern "C" void * calloc( size_t n, size_t size )
{
    //malloc_trace_printf( "calloc called: n=%d, size=%d\n", n, size );

    void * p = __libc_calloc( n, size );

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    return p;
}

extern "C" void * realloc( void * old_p, size_t size )
{
    //malloc_trace_printf( "realloc called: old_p=%p, size=%d\n", old_p, size );

    void * new_p = __libc_realloc( old_p, size );

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Realloc, old_p, new_p, size );

    return new_p;
}

static inline bool is_power_of_2( size_t x )
{
    return (((x-1) & x)==0);
}

extern "C" int posix_memalign( void **memptr, size_t align, size_t size )
{
    //malloc_trace_printf( "posix_memalign called: align=%d, size=%d\n", align, size );

    if( align % sizeof(void*) != 0
        || !is_power_of_2(align / sizeof(void*))
        || align == 0)
    {
        return EINVAL;
    }

    int result;
    *memptr = __libc_memalign(align, size);

    if(*memptr)
    {
        result = 0;
    }
    else
    {
        result = ENOMEM;
    }

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, *memptr, NULL, size );

    return result;
}

extern "C" void free( void * p )
{
    //malloc_trace_printf( "free called: p=%p\n", p );

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Free, p, NULL, 0 );

    __libc_free(p);
}

extern "C" void * aligned_alloc( size_t align, size_t size )
{
    //malloc_trace_printf( "aligned_alloc called: align=%d, size=%d\n", align, size );

    void * p = __libc_memalign( align, size );

    ADD_MALLOC_CALL_HISTORY( MallocOperation_Alloc, p, NULL, size );

    return p;
}

extern "C" void * valloc( size_t size )
{
    malloc_trace_printf( "valloc called: size=%d\n", size );

    abort();

    return NULL;
}

extern "C" void * pvalloc( size_t size )
{
    malloc_trace_printf( "pvalloc called: size=%d\n", size );

    abort();

    return NULL;
}

extern "C" size_t malloc_usable_size(void *ptr)
{
    malloc_trace_printf( "malloc_usable_size called: p=%p\n", ptr );

    abort();

    return 0;
}

#endif //defined(REPLACE_MALLOC_FUNCTIONS)

// ---

void test_malloc_functions()
{
    malloc_trace_printf("test_malloc_functions starting\n");

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

    malloc_trace_printf("test_malloc_functions ended\n");
}

void test_malloc_functions_multi_threads()
{
    const int num_threads = 10;
    std::thread t[num_threads];

    for( int i=0 ; i<num_threads ; ++i )
    {
        t[i] = std::thread(test_malloc_functions);
    }

    for( int i=0 ; i<num_threads ; ++i )
    {
        t[i].join();
    }
}

int main( int argc, const char * argv[] )
{
    int result = 0;

    // Start tracing malloc/free calls
    {
        char trace_log_filename[256];
        snprintf( trace_log_filename, sizeof(trace_log_filename)-1, TRACE_LOG_DIRNAME "malloc_trace.%d.log", getpid() );
        malloc_trace_printf( "Starting malloc tracing : %s\n", trace_log_filename );
        malloc_trace_start(trace_log_filename);
    }

    if(false)
    {
        test_malloc_functions();
    }

    if(false)
    {
        test_malloc_functions_multi_threads();
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
    {
        malloc_trace_stop();
    }

    return result;
}
