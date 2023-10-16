### Overview

py_malloc_trace is a sample solution to troubleshoot memory leaking / memory accumulating issues. It replaces libc's malloc/free functions (and their family functions such as memalign, realloc, etc), and trace memory allocation activities and write in JSON lines file. You can apply post-process on the log and analyze why memory usage is increasing.

### Usage

1. Build `py_malloc_trace` executable.
1. Include `py_malloc_trace` in the application container image by editing Dockerfile.
1. Build & Package & Deploy the application to your device. Please use PanoJupyter enabled environment.
1. On PanoJupyter, run your application with `py_malloc_trace`. Use `py_malloc_trace` instead of `python3` in your command line. `py_malloc_trace` writes `/tmp/malloc_trace.{pid}.log`.
    ``` bash
    py_malloc_trace myapp.py --other-args ...
    ```
1. While your application is running, run following command, and dump memory mapping information.
    ``` bash
    cat /proc/{pid}/maps > memory_map.txt
    ```
1. Terminate your application.
1. Run `parse_malloc_trace_log.py` script and check the output.
    ``` bash
    python3 parse_malloc_trace_log.py --mapfile memory_map.txt --logfile malloc_trace.{pid}.log
    ```

### Limitations

* This solution can trace malloc/free calls but cannot trace memory allocations by system calls (e.g. mmep()).
* This solution captures backtrace of malloc/free calls, but the depth of the backtrace is limited to one.
* `parse_malloc_trace_log.py` has to be executed on real Panorama environment, because this script uses `readelf` to read symbol tables from executable files.
* If memory is allocated from *.so and the *.so is unloaded without free-ing the memory, caller name of the memory cannot be resolved. In order to solve this issue, you need to make sure *.so are not unloaded.