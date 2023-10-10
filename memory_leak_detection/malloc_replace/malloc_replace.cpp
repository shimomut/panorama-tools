#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <execinfo.h>

#include <cstdlib>
#include <thread>
#include <mutex>

#include "Python.h"

#include "dlmalloc.h"

//-----

static const size_t BUFFER_SIZE = 100000;

static const size_t NUM_RETURN_ADDR_LEVELS = 2;

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
        malloc_call_history_size(0)
    {
        memset( malloc_call_history, 0, sizeof(malloc_call_history) );
    }

    std::recursive_mutex mtx;

    bool enabled;

    MallocCallHistory malloc_call_history[BUFFER_SIZE];
    size_t malloc_call_history_size;

    std::string output_filename;
};

static Globals g;

// ---

static void flush_malloc_call_history()
{
    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    if( g.malloc_call_history_size > 0 )
    {
        int fd = open( g.output_filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644 );

        for( size_t i=0 ; i<g.malloc_call_history_size ; ++i )
        {
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
                Dl_info dl_info;
                dladdr( g.malloc_call_history[i].return_addr[level], &dl_info );

                const char * format;
                if( level<NUM_RETURN_ADDR_LEVELS-1 )
                {
                    format = "{\"module\":\"%s\",\"symbol\":\"%s\"},";
                }
                else
                {
                    format = "{\"module\":\"%s\",\"symbol\":\"%s\"}";
                }
                len = snprintf( buf, sizeof(buf)-1, format, 
                    dl_info.dli_fname,
                    dl_info.dli_sname);
                
                ssize_t result = write( fd, buf, len );
                (void)result;
            }

            {
                const char msg[] = "]}\n";
                
                ssize_t result = write( fd, msg, sizeof(msg)-1 );
                (void)result;
            }
        }

        close(fd);

        g.malloc_call_history_size = 0;
    }
}

static inline void add_malloc_call_history( MallocOperation op, void * p, void * p2, size_t size )
{
    if(!g.enabled)
    {
        return;
    }

    if( g.malloc_call_history_size==BUFFER_SIZE )
    {
        flush_malloc_call_history();
    }

    g.malloc_call_history[g.malloc_call_history_size].op = op;
    g.malloc_call_history[g.malloc_call_history_size].p = p;
    g.malloc_call_history[g.malloc_call_history_size].p2 = p2;
    g.malloc_call_history[g.malloc_call_history_size].size = size;

    void * bt[NUM_RETURN_ADDR_LEVELS+1] = {0};
    backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
    for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
    {
        g.malloc_call_history[g.malloc_call_history_size].return_addr[level] = bt[level+1];
    }

    g.malloc_call_history_size ++;
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

void * malloc( size_t size )
{
    g.mtx.lock();

    //const char msg[] = "malloc called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * p = dlmalloc(size);

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    g.mtx.unlock();

    return p;
}

void * memalign( size_t align, size_t size )
{
    g.mtx.lock();

    //const char msg[] = "memalign called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * p = dlmemalign( align, size );

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    g.mtx.unlock();

    return p;
}

void * calloc( size_t n, size_t size )
{
    g.mtx.lock();

    //const char msg[] = "calloc called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * p = dlcalloc( n, size );

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    g.mtx.unlock();

    return p;
}

void * realloc( void * old_p, size_t size )
{
    g.mtx.lock();

    //const char msg[] = "realloc called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * new_p = dlrealloc( old_p, size );

    add_malloc_call_history( MallocOperation_Realloc, old_p, new_p, size );

    g.mtx.unlock();

    return new_p;
}

void free( void * p )
{
    g.mtx.lock();

    //const char msg[] = "free called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    add_malloc_call_history( MallocOperation_Free, p, NULL, 0 );

    dlfree(p);

    g.mtx.unlock();
}

void * aligned_alloc( size_t align, size_t size )
{
    const char msg[] = "aligned_alloc called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return NULL;
}

void * valloc( size_t size )
{
    const char msg[] = "valloc called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return NULL;
}

void * pvalloc( size_t size )
{
    const char msg[] = "pvalloc called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return NULL;
}

int posix_memalign( void **memptr, size_t align, size_t size )
{
    g.mtx.lock();

    //const char msg[] = "posix_memalign called\n";
    //ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    //(void)result;

    void * p = dlmemalign( align, size );

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    *memptr = p;

    g.mtx.unlock();

    return 0;
}

size_t malloc_usable_size(void *ptr)
{
    const char msg[] = "malloc_usable_size called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return 0;
}

// ---

static void _signal_handler(int sig)
{
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
    {
        void * p = malloc(100);
        free(p);
    }

    {
        void * p = memalign(64,100);
        free(p);
    }

    {
        void * p = calloc(10,100);
        free(p);
    }

    {
        void * p = malloc(100);
        void * p2 = realloc(p,200);
        free(p2);
    }

    {
        void * p1 = malloc(100);
        void * p2 = malloc(100);

        // intentionally cause memory leak
        p1 = NULL;
        
        free(p1);
        free(p2);
    }

    {
        void * p = 0;
        int result = posix_memalign( &p, 128, 1024 );
        printf( "posix_memalign result : %d, %p\n", result, p );
    }
}

int main( int argc, const char * argv[] )
{
    // use backtrace to implicitly initialize libgcc.
    // Are there better solution?
    {
        void * bt[30];
        backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
    }

    install_signal_handler();

    malloc_free_trace_start("./malloc_trace.log");

    //test();

    wchar_t * wargv[100];
    for( int i=0 ; i<argc ; ++i )
    {
        wargv[i] = Py_DecodeLocale( argv[i], NULL );
    }

    Py_Initialize();

    int result = Py_Main(argc, wargv);

    Py_Finalize();

    for( int i=0 ; i<argc ; ++i )
    {
        PyMem_RawFree(wargv[i]);
    }

    malloc_free_trace_stop();

    return result;
}
