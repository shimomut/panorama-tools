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

static const int NUM_CHECK_POINT_HISTORY = 30;

struct CheckPoint
{
    CheckPoint( const char * _filename=NULL, const char * _funcname=NULL, int _lineno=0 )
        :
        filename(_filename),
        funcname(_funcname),
        lineno(_lineno)
    {
    }

    const char * filename;
    const char * funcname;
    int lineno;
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

        check_points[next_index] = CheckPoint( _filename, _funcname, _lineno );
        next_index = (next_index+1) % NUM_CHECK_POINT_HISTORY;
    }

    void Print()
    {
        const char header[] = "Check points:\n";
        size_t result = write( STDERR_FILENO, header, sizeof(header)-1 );
        (void)result;

        std::lock_guard<std::recursive_mutex> lock(mtx);

        for( int i=0 ; i<NUM_CHECK_POINT_HISTORY ; ++i )
        {
            int index = (next_index+i) % NUM_CHECK_POINT_HISTORY;
            if( check_points[index].filename )
            {
                size_t result = write( STDERR_FILENO, check_points[index].filename, strlen(check_points[index].filename) );
                (void)result;

                const char hyphen[] = " - ";
                result = write( STDERR_FILENO, hyphen, sizeof(hyphen)-1 );
                (void)result;

                result = write( STDERR_FILENO, check_points[index].funcname, strlen(check_points[index].funcname) );
                (void)result;

                result = write( STDERR_FILENO, hyphen, sizeof(hyphen)-1 );
                (void)result;

                char buf[32];
                int len = snprintf( buf, sizeof(buf)-1, "%d\n", check_points[index].lineno );
                result = write( STDERR_FILENO, buf, len );
                (void)result;
            }
        }
    }

    std::recursive_mutex mtx;

    CheckPoint check_points[NUM_CHECK_POINT_HISTORY];
    int next_index;
};

static CheckPointHistory check_point_history;

#define CHECK_POINT() check_point_history.Check(__FILE__,__func__,__LINE__)

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

extern "C" void * malloc( size_t size )
{
    CHECK_POINT();

    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    CHECK_POINT();

    //const char msg[] = "malloc called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * p = dlmalloc(size);

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * memalign( size_t align, size_t size )
{
    CHECK_POINT();

    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    CHECK_POINT();

    //const char msg[] = "memalign called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * p = dlmemalign( align, size );

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * calloc( size_t n, size_t size )
{
    CHECK_POINT();

    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    CHECK_POINT();

    //const char msg[] = "calloc called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * p = dlcalloc( n, size );

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    CHECK_POINT();

    return p;
}

extern "C" void * realloc( void * old_p, size_t size )
{
    CHECK_POINT();

    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    CHECK_POINT();

    //const char msg[] = "realloc called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    void * new_p = dlrealloc( old_p, size );

    add_malloc_call_history( MallocOperation_Realloc, old_p, new_p, size );

    CHECK_POINT();

    return new_p;
}

extern "C" void free( void * p )
{
    CHECK_POINT();

    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    CHECK_POINT();

    //const char msg[] = "free called\n";
    //write( STDOUT_FILENO, msg, sizeof(msg) );

    add_malloc_call_history( MallocOperation_Free, p, NULL, 0 );

    dlfree(p);

    CHECK_POINT();
}

extern "C" void * aligned_alloc( size_t align, size_t size )
{
    CHECK_POINT();

    const char msg[] = "aligned_alloc called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return NULL;
}

extern "C" void * valloc( size_t size )
{
    CHECK_POINT();

    const char msg[] = "valloc called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return NULL;
}

extern "C" void * pvalloc( size_t size )
{
    CHECK_POINT();

    const char msg[] = "pvalloc called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return NULL;
}

static bool is_power_of_2( size_t n )
{
    if(n==0)
    {
        return false;
    }

    while( n!=1 )
    {
        if( n%2 != 0 )
        {
            return false;
        }

        n = n/2;
    }

    return true;
}

extern "C" int posix_memalign( void **memptr, size_t align, size_t size )
{
    CHECK_POINT();

    if( ! is_power_of_2(align) )
    {
        return EINVAL;
    }

    CHECK_POINT();

    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    CHECK_POINT();

    //const char msg[] = "posix_memalign called\n";
    //ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    //(void)result;

    void * p = dlmemalign( align, size );

    add_malloc_call_history( MallocOperation_Alloc, p, NULL, size );

    *memptr = p;

    CHECK_POINT();

    if(!p)
    {
        return ENOMEM;
    }

    CHECK_POINT();

    return 0;
}

extern "C" size_t malloc_usable_size(void *ptr)
{
    CHECK_POINT();

    const char msg[] = "malloc_usable_size called\n";
    ssize_t result = write( STDOUT_FILENO, msg, sizeof(msg) );
    (void)result;

    abort();

    return 0;
}

// ---

static void _signal_handler(int sig)
{
    check_point_history.Print();

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
        (void)result;
        free(p);
    }
}

int main( int argc, const char * argv[] )
{
    int result = 0;

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

    result = Py_Main(argc, wargv);

    Py_Finalize();

    for( int i=0 ; i<argc ; ++i )
    {
        PyMem_RawFree(wargv[i]);
    }

    malloc_free_trace_stop();

    check_point_history.Print();

    return result;
}
