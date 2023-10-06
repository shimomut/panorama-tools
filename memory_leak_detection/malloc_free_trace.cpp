#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>

#include <cstdlib>
#include <thread>
#include <mutex>

static const size_t BUFFER_SIZE = 100000;
//static const size_t BUFFER_SIZE = 10;

enum MallocOperation
{
    MallocOperation_Alloc = 1,
    MallocOperation_Free = 2
};

struct MallocCallHistory
{
    MallocOperation op;
    void * p;
    size_t size;
    const char * module_name;
    const char * symbol_name;
};

struct Globals
{
    Globals()
        :
        old_malloc_hook(0),
        old_memalign_hook(0),
        old_realloc_hook(0),
        old_free_hook(0),
        malloc_call_history_size(0)
    {
        memset( malloc_call_history, 0, sizeof(malloc_call_history) );
    }

    std::recursive_mutex mtx;

    void* (*old_malloc_hook)(size_t, const void*);
    void* (*old_memalign_hook)(size_t, size_t, const void*);
    void* (*old_realloc_hook)(void*, size_t, const void*);
    void (*old_free_hook)(void*, const void*);

    MallocCallHistory malloc_call_history[BUFFER_SIZE];
    size_t malloc_call_history_size;

    std::string output_filename;
};

static Globals g;

static void * my_malloc_hook (size_t size, const void *caller);
static void * my_memalign_hook (size_t align, size_t size, const void *caller);
static void * my_realloc_hook (void * old_p, size_t size, const void *caller);
static void my_free_hook (void *ptr, const void *caller);

static inline void backup_hooks()
{
    g.old_malloc_hook = __malloc_hook;
    g.old_memalign_hook = __memalign_hook;
    g.old_realloc_hook = __realloc_hook;
    g.old_free_hook = __free_hook;
}

static inline void enable_hooks()
{
    __malloc_hook = my_malloc_hook;
    __memalign_hook = my_memalign_hook;
    __realloc_hook = my_realloc_hook;
    __free_hook = my_free_hook;
}

static inline void disable_hooks()
{
    __malloc_hook = g.old_malloc_hook;
    __memalign_hook = g.old_memalign_hook;
    __realloc_hook = g.old_realloc_hook;
    __free_hook = g.old_free_hook;
}

static void flush_malloc_call_history()
{
    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    if( g.malloc_call_history_size > 0 )
    {
        int fd = open( g.output_filename.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644 );

        for( size_t i=0 ; i<g.malloc_call_history_size ; ++i )
        {
            char buf[1024];
            int len = snprintf( buf, sizeof(buf)-1, "{\"op\":%d,\"p\":\"%p\",\"size\":%zd,\"module\":\"%s\",\"symbol\":\"%s\"}\n", 
                g.malloc_call_history[i].op,
                g.malloc_call_history[i].p,
                g.malloc_call_history[i].size,
                g.malloc_call_history[i].module_name,
                g.malloc_call_history[i].symbol_name );
            
            write( fd, buf, len );
        }

        close(fd);

        g.malloc_call_history_size = 0;
    }
}

static inline void add_malloc_call_history( MallocOperation op, void * p, size_t size, const void * caller )
{
    if( g.malloc_call_history_size==BUFFER_SIZE )
    {
        flush_malloc_call_history();
    }

    Dl_info dl_info;
    dladdr( caller, &dl_info );

    g.malloc_call_history[g.malloc_call_history_size].op = op;
    g.malloc_call_history[g.malloc_call_history_size].p = p;
    g.malloc_call_history[g.malloc_call_history_size].size = size;
    g.malloc_call_history[g.malloc_call_history_size].module_name = dl_info.dli_fname;
    g.malloc_call_history[g.malloc_call_history_size].symbol_name = dl_info.dli_sname;

    g.malloc_call_history_size ++;
}

static void * my_malloc_hook (size_t size, const void *caller)
{
    g.mtx.lock();

    disable_hooks();

    // Call recursively
    void * p = malloc(size);

    add_malloc_call_history( MallocOperation_Alloc, p, size, caller );

    enable_hooks();

    g.mtx.unlock();

    return p;
}

static void * my_memalign_hook (size_t align, size_t size, const void *caller)
{
    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    disable_hooks();
    
    // Call recursively
    void * p = memalign(align,size);

    add_malloc_call_history( MallocOperation_Alloc, p, size, caller );

    enable_hooks();

    return p;
}

static void * my_realloc_hook (void * old_p, size_t size, const void *caller)
{
    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    disable_hooks();
    
    // Call recursively
    void * p = realloc(old_p,size);

    add_malloc_call_history( MallocOperation_Free, old_p, 0, caller );
    add_malloc_call_history( MallocOperation_Alloc, p, size, caller );

    enable_hooks();

    return p;
}

static void my_free_hook(void * p, const void *caller)
{
    std::lock_guard<std::recursive_mutex> lock(g.mtx);

    disable_hooks();

    add_malloc_call_history( MallocOperation_Free, p, 0, caller );

    // Call recursively
    free(p);

    enable_hooks();
}

void allocate_and_free_many_times()
{
    for( int i=0 ; i<1000 ; ++i )
    {
        size_t size = std::rand() % (1024 * 1024);
        
        void * p = malloc(size);
        printf("allocated %p\n", p);

        usleep( std::rand() % (1000) );
        
        printf("freeing %p\n", p);
        free(p);
    }
}

static void malloc_free_trace_start( const char * output_filename )
{
    g.output_filename = output_filename;

    backup_hooks();
    enable_hooks();
}

static void malloc_free_trace_stop()
{
    disable_hooks();

    flush_malloc_call_history();
}

int main( int argc, const char * argv[] )
{
    malloc_free_trace_start( "./trace.log" );
    
    std::thread t1( allocate_and_free_many_times );
    std::thread t2( allocate_and_free_many_times );
    std::thread t3( allocate_and_free_many_times );
    std::thread t4( allocate_and_free_many_times );

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    malloc_free_trace_stop();

    return 0;
}

