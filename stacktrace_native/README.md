# stacktrace_native

## Overview

When Panorama application stopped due to uncaught Python exception, Python interpreter prints exception information to the standard error, and application developers can see what caused the error. But when the application crashed in native world, it doesn't print any helpful information for troubleshooting.

`stacktrace_native` is a Python module you can import in your Panorama Python process to enable stack trace when crash happened in native world.


## How to build

1. Make sure your environment have cross-compiler `aarch64-linux-gnu-gcc` installed.

1. Open `makefile`, enable either `PLATFORM = arm_64` or `PLATFORM = x86_64`

    For Panorama real hardware, choose `arm_64`.

    ``` makefile
    #PLATFORM = x86_64
    PLATFORM = arm_64
    ```

1. Run `make` command

    ``` shell
    $ make
    ```

    Make sure you get newly built *.so file under `./dynlib` directory.


## How to use

> See also `test.py`.

1. In your Python process, import `stacktrace_native` module. Before importing this module, depending on where this *.so file is located, you need to add the path to sys.path.

    ``` python
    if os.path.exists("/panorama/dynlibs"):
        # real hardware
        native_module_location = "/panorama/dynlibs"
    else:
        # test on PC
        native_module_location = os.path.abspath("./dynlibs")

    if native_module_location not in sys.path:
        sys.path.insert( 0, native_module_location )

    import stacktrace_native
    ```

1. Call stacktrace_native.install_signal_handler() function for signal codes you want to enable stack trace.

    ``` python
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
    ```

1. Run the main part of your application. 

1. If you want to test stack trace actually works, you can use `stacktrace_native.crash1()` function which causes null pointer access (SIGSEGV).

    ``` python
    stacktrace_native.crash1() # null pointer access
    ```

1. Make sure you get stack trace in the stderr output, like following:

    ```
    Error : stacktrace_native caught signal 11:
    /home/shimomut/project/Panorama/stacktrace_native/dynlibs/stacktrace_native.so(+0xbf1)[0x7f8aa90a7bf1]
    /lib/x86_64-linux-gnu/libc.so.6(+0x3f040)[0x7f8aaa4d4040]
    /home/shimomut/project/Panorama/stacktrace_native/dynlibs/stacktrace_native.so(+0xb19)[0x7f8aa90a7b19]
    python3[0x50a22f]
    python3(_PyEval_EvalFrameDefault+0x444)[0x50bf44]
    python3[0x5096c8]
    python3[0x50a3fd]
    python3(_PyEval_EvalFrameDefault+0x444)[0x50bf44]
    python3[0x507cd4]
    python3(PyEval_EvalCode+0x23)[0x50ae13]
    python3[0x635262]
    python3(PyRun_FileExFlags+0x97)[0x635317]
    python3(PyRun_SimpleFileExFlags+0x17f)[0x638acf]
    python3(Py_Main+0x591)[0x639671]
    python3(main+0xe0)[0x4b0e40]
    /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0xe7)[0x7f8aaa4b6bf7]
    python3(_start+0x2a)[0x5b2f0a]
    ```


## How to deploy

1. For Panorama real hardware, copy the compiled *.so file to your Panorama application code package.

    ``` shell
    $ mkdir ../your_app/packages/123456789012-your_app_code-1.0/src/dynlibs
    $ cp ./dynlibs/stacktrace_native.cpython-37m-aarch64-linux-gnu.so ../your_app/packages/123456789012-your_app_code-1.0/src/dynlibs/
    ```

1. Make sure the `stacktrace_native.cpython-37m-aarch64-linux-gnu.so` is located properly under the code package, and it will go to `/panorama/dynlib`. Make sure it has **executable** permission.

    ``` shell
    $ ls -al src/dynlibs/
    total 56
    drwxr-xr-x 2 shimomut shimomut  4096 Mar 17 17:05 .
    drwxr-xr-x 3 shimomut shimomut  4096 Mar 17 23:56 ..
    -rwxr-xr-x 1 shimomut shimomut 46832 Mar 17 23:25 stacktrace_native.cpython-37m-aarch64-linux-gnu.so

    $ cat Dockerfile
    FROM public.ecr.aws/panorama/panorama-application
    COPY src /panorama
    RUN pip3 install boto3
    ```

1. Using regular steps, build the container image, upload packages, and deploy to your device.

