import sys
import os
import time
import signal


# In order to import stacktrace_native, add a library path to sys.path.

if os.path.exists("/panorama/dynlibs"):
    # real hardware
    native_module_location = "/panorama/dynlibs"
else:
    # test on PC
    native_module_location = os.path.abspath("./dynlibs")

if native_module_location not in sys.path:
    sys.path.insert( 0, native_module_location )

import stacktrace_native


def test_native_signal_handler( launcher ):

    signals = [
        signal.SIGABRT,
        signal.SIGBUS,
        signal.SIGFPE,
        signal.SIGHUP,
        signal.SIGILL,
        signal.SIGINT,
        signal.SIGKILL,
        signal.SIGSEGV,
        signal.SIGTERM,
    ]
    
    for s in signals:
        stacktrace_native.install_signal_handler(s)
        
    stacktrace_native.crash1() # null pointer access
    #stacktrace_native.crash2() # stack overflow
    #stacktrace_native.crash3() # jump to invalid function pointer
    #stacktrace_native.crash4() # integer zero div


test_native_signal_handler(None)

