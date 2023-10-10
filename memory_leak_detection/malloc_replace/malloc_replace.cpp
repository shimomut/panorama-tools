#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
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

            len = snprintf( buf, sizeof(buf)-1, "{\"op\":%d,\"p\":\"%p\",\"p2\":\"%p\",\"size\":%zd,\"return_addr\":[", 
                g.malloc_call_history[i].op,
                g.malloc_call_history[i].p,
                g.malloc_call_history[i].p2,
                g.malloc_call_history[i].size );
            write( fd, buf, len );

            for( size_t level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
            {
                Dl_info dl_info;
                dladdr( g.malloc_call_history[i].return_addr[level], &dl_info );

                const char * format;
                if( level<NUM_RETURN_ADDR_LEVELS-1 )
                {
                    format = "{\"addr\":\"%p\",\"module\":\"%s\",\"symbol\":\"%s\"},";
                }
                else
                {
                    format = "{\"addr\":\"%p\",\"module\":\"%s\",\"symbol\":\"%s\"}";
                }
                len = snprintf( buf, sizeof(buf)-1, format, 
                    g.malloc_call_history[i].return_addr[level],
                    dl_info.dli_fname,
                    dl_info.dli_sname);
                write( fd, buf, len );
            }

            const char msg[] = "]}\n";
            write( fd, msg, sizeof(msg)-1 );
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
    size_t num_bt_returned = backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
    for( int level=0 ; level<NUM_RETURN_ADDR_LEVELS ; ++level )
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

int main( int argc, const char * argv[] )
{
    // use backtrace to implicitly initialize libgcc.
    // Are there better solution?
    {
        void * bt[30];
        size_t num_bt_returned = backtrace( bt, sizeof(bt)/sizeof(bt[0]) );
    }

    malloc_free_trace_start("./malloc_trace.log");

    {
        void * p = malloc(100);
        free(p);
    }

    {
        void * p1 = malloc(100);
        void * p2 = malloc(100);
        
        free(p2);
    }

    /*
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
    */

    malloc_free_trace_stop();

    return 0;
}
