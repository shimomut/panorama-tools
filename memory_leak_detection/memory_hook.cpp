#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>

static void* (*old_malloc_hook)(size_t, const void*);
static void* (*old_memalign_hook)(size_t, size_t, const void*);
static void* (*old_realloc_hook)(void*, size_t, const void*);
static void (*old_free_hook)(void*, const void*);

//static void * my_malloc_hook (size_t size, const void *caller);
static void my_free_hook (void *ptr, const void *caller);

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

static MallocCallHistory malloc_call_history[100000];
static size_t malloc_call_history_size = 0;

static int in_hook = 0;

static inline void add_malloc_call_history( MallocOperation op, void * p, size_t size, const void * caller )
{
    Dl_info dl_info;
    dladdr( caller, &dl_info );

    malloc_call_history[malloc_call_history_size].op = op;
    malloc_call_history[malloc_call_history_size].p = p;
    malloc_call_history[malloc_call_history_size].size = size;
    malloc_call_history[malloc_call_history_size].module_name = dl_info.dli_fname;
    malloc_call_history[malloc_call_history_size].symbol_name = dl_info.dli_sname;

    malloc_call_history_size ++;
}

static void * my_malloc_hook (size_t size, const void *caller)
{
    if(in_hook){abort();}
    in_hook++;

    // Restore all old hooks
    __malloc_hook = old_malloc_hook;
    
    // Call recursively
    void * p = malloc(size);

    add_malloc_call_history( MallocOperation_Alloc, p, size, caller );

    // Restore our own hooks */
    __malloc_hook = my_malloc_hook;

    in_hook--;

    return p;
}

static void * my_memalign_hook (size_t align, size_t size, const void *caller)
{
    if(in_hook){abort();}
    in_hook++;

    // Restore all old hooks
    __memalign_hook = old_memalign_hook;
    
    // Call recursively
    void * p = memalign(align,size);

    add_malloc_call_history( MallocOperation_Alloc, p, size, caller );

    // Restore our own hooks */
    __memalign_hook = my_memalign_hook;

    in_hook--;

    return p;
}

static void * my_realloc_hook (void * old_p, size_t size, const void *caller)
{
    if(in_hook){abort();}
    in_hook++;

    // Restore all old hooks
    __realloc_hook = old_realloc_hook;
    
    // Call recursively
    void * p = realloc(old_p,size);

    add_malloc_call_history( MallocOperation_Free, old_p, 0, caller );
    add_malloc_call_history( MallocOperation_Alloc, p, size, caller );

    // Restore our own hooks */
    __realloc_hook = my_realloc_hook;

    in_hook--;

    return p;
}

static void my_free_hook(void * p, const void *caller)
{
    if(in_hook){abort();}
    in_hook++;

    // Restore all old hooks
    __free_hook = old_free_hook;

    add_malloc_call_history( MallocOperation_Free, p, 0, caller );

    // Call recursively
    free(p);

    // Restore our own hooks
    __free_hook = my_free_hook;

    in_hook--;
}

int main( int argc, const char * argv[] )
{
    // install hook functions
    old_malloc_hook = __malloc_hook;
    old_memalign_hook = __memalign_hook;
    old_realloc_hook = __realloc_hook;
    old_free_hook = __free_hook;
    __malloc_hook = my_malloc_hook;
    __memalign_hook = my_memalign_hook;
    __realloc_hook = my_realloc_hook;
    __free_hook = my_free_hook;
    
    printf("Hello malloc hook test\n");

    for( int i=0 ; i<10 ; ++i )
    {
        //void * p = malloc( 1024 * 1024 );
        //void * p = memalign( 64, 1024 * 1024 );
        void * p = calloc( 10, 1024 * 1024 );
        if(p==0){abort();}

        void * p2 = realloc( p, 2 * 1024 * 1024 );
        if(p2==0){abort();}
        p = NULL;

        if(p2)
        {
            free(p2);
        }

        if(p)
        {
            free(p);
        }
    }

    // uninstall hook functions
    __malloc_hook = old_malloc_hook;
    __memalign_hook = old_memalign_hook;
    __realloc_hook = old_realloc_hook;
    __free_hook = old_free_hook;

    // print malloc operation history
    for( size_t i=0 ; i<malloc_call_history_size ; ++i )
    {
        printf( "%d, %p, %zd, %s, %s\n", 
            malloc_call_history[i].op,
            malloc_call_history[i].p,
            malloc_call_history[i].size,
            malloc_call_history[i].module_name,
            malloc_call_history[i].symbol_name );
    }

    return 0;
}

