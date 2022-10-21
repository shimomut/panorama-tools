#include <stdio.h>
#include <signal.h>
#include <execinfo.h>

#include "Python.h"

//-----

#define MODULE_NAME "stacktrace_native"

//-----

static PyObject * _hello( PyObject * self, PyObject * args )
{
    return PyUnicode_FromString( "Hello World!" );
}

static void _signal_handler(int sig)
{
    void *array[30];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace( array, sizeof(array)/sizeof(array[0]) ) ;

    // print out all the frames to stderr
    fprintf(stderr, "Error : " MODULE_NAME " caught signal %d:\n", sig );
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    exit(1);
}

static PyObject * _install_signal_handler(PyObject* self, PyObject* args, PyObject * kwds)
{
    int signalnum;

    static const char * kwlist[] = {
        "signalnum",
        NULL
    };

    if( ! PyArg_ParseTupleAndKeywords( args, kwds, "i", const_cast<char**>(kwlist), &signalnum ) )
    {
        return NULL;
    }

    signal( signalnum, _signal_handler);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * _crash1( PyObject * self, PyObject * args )
{
    if( ! PyArg_ParseTuple(args, "" ) )
    {
        return NULL;
    }

    int * p = 0;
    p[0] = 1;
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * _crash2( PyObject * self, PyObject * args )
{
    if( ! PyArg_ParseTuple(args, "" ) )
    {
        return NULL;
    }
        
    char s[10];
    strcpy( s, "ThisIsAStringLongerThanCopyDestination." );
    
    return PyUnicode_FromString(s);
}

static PyObject * _crash3( PyObject * self, PyObject * args )
{
    if( ! PyArg_ParseTuple(args, "" ) )
    {
        return NULL;
    }
    
    typedef void (*F)(int);
    
    char wrong_function_address[1];
    
    F f = (F)(wrong_function_address);
    (*f)(1);
        
    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject * _crash4( PyObject * self, PyObject * args )
{
    if( ! PyArg_ParseTuple(args, "" ) )
    {
        return NULL;
    }
	
	/*	
	float f1 = 123.0f;
	float f2 = 0.0f;
	float f3 = f1 / f2;

	PyObject * pyret = Py_BuildValue("f", f3);
	return pyret;
	*/

	int i1 = 123;
	int i2 = 0;
	int i3 = i1 / i2;
        
	PyObject * pyret = Py_BuildValue("i", i3);
	return pyret;
}

static PyMethodDef stacktrace_native_funcs[] =
{
    { "hello", _hello, METH_VARARGS, "Return 'hello' string." },
    { "install_signal_handler", (PyCFunction)_install_signal_handler, METH_VARARGS|METH_KEYWORDS, "Install signal handler to print stack trace." },
    { "crash1", _crash1, METH_VARARGS, "Null pointer access." },
    { "crash2", _crash2, METH_VARARGS, "Stack overflow." },
    { "crash3", _crash3, METH_VARARGS, "Invalid function pointer." },
    { "crash4", _crash4, METH_VARARGS, "Zero division." },
    {NULL, NULL, 0, NULL}
};

static PyModuleDef stacktrace_native_module =
{
    PyModuleDef_HEAD_INIT,
    MODULE_NAME,
    "stacktrace_native module.",
    -1,
    stacktrace_native_funcs,
    NULL, NULL, NULL, NULL
};

//-----

extern "C" PyMODINIT_FUNC PyInit_stacktrace_native(void)
{
    PyObject * m;

    m = PyModule_Create(&stacktrace_native_module);
    if(m == NULL) return NULL;
    
    return m;
}

