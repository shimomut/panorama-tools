### Overview

**py_malloc_trace** is a sample solution to troubleshoot memory leaking / memory accumulating issues. It replaces libc's malloc/free functions (and their family functions such as memalign, realloc, etc), trace memory allocation activities, and write in JSON lines file. You can apply post-process on the activity log to analyze why memory usage is increasing.


### Prerequisite

* Aarch64-linux based build environment (e.g EC2)
* Python header/lib
* readelf program to look up symbols


### Build

1. Build `py_malloc_trace` executable by `make` command.
1. (Optional) Test it by `make run` command.
1. Edit Dockerfile to include `py_malloc_trace` in the application container image. Make sure the file has executable permission.
1. Build & Package & Deploy the application to your device. Please use PanoJupyter enabled environment.

### Run

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
1. As-needed, compare multiple versions of outputs to see who's memory is increasing.


### Example output


**trace log**
``` json
    :
{"op":1,"p":"0x55c751ca30","p2":"(nil)","size":2208,"return_addr":["0x7fa863ae80"]}
{"op":2,"p":"0x55c751c5d0","p2":"(nil)","size":0,"return_addr":["0x7fa864d3e0"]}
{"op":1,"p":"0x55c751d2e0","p2":"(nil)","size":3536,"return_addr":["0x7fa863ae80"]}
{"op":1,"p":"0x55c751c380","p2":"(nil)","size":576,"return_addr":["0x7fa863ae80"]}
{"op":1,"p":"0x55c751c5d0","p2":"(nil)","size":576,"return_addr":["0x7fa863b008"]}
{"op":2,"p":"0x55c751c5d0","p2":"(nil)","size":0,"return_addr":["0x7fa8668930"]}
{"op":1,"p":"0x55c751c5d0","p2":"(nil)","size":576,"return_addr":["0x7fa863ae80"]}
{"op":1,"p":"0x55c751e0c0","p2":"(nil)","size":640,"return_addr":["0x7fa863ae80"]}
{"op":1,"p":"0x55c752aa40","p2":"(nil)","size":576,"return_addr":["0x7fa863ae80"]}
{"op":1,"p":"0x55c752ac90","p2":"(nil)","size":1112,"return_addr":["0x7fa863ae80"]}
{"op":2,"p":"0x55c752aa40","p2":"(nil)","size":0,"return_addr":["0x7fa864d3e0"]}
{"op":1,"p":"0x55c7519550","p2":"(nil)","size":40,"return_addr":["0x7fa8549988"]}
{"op":2,"p":"0x55c7519550","p2":"(nil)","size":0,"return_addr":["0x7fa85e84fc"]}
{"op":1,"p":"0x55c752b0f0","p2":"(nil)","size":2208,"return_addr":["0x7fa863ae80"]}
    :
```

**post-process**
```
Num remaining memory blocks and total size:
('/lib/aarch64-linux-gnu/ld-2.27.so::(unknown)',) : num blocks: 1472 : total size: 91109
('/lib/aarch64-linux-gnu/ld-2.27.so::_dl_exception_create@@GLIBC_PRIVATE',) : num blocks: 4 : total size: 208
('/lib/aarch64-linux-gnu/ld-2.27.so::_dl_mcount@@GLIBC_2.17',) : num blocks: 72 : total size: 5664
('/lib/aarch64-linux-gnu/libc-2.27.so::(unknown)',) : num blocks: 14 : total size: 5953
('/lib/aarch64-linux-gnu/libc-2.27.so::_IO_vfscanf@@GLIBC_2.17',) : num blocks: 4 : total size: 6224
('/lib/aarch64-linux-gnu/libc-2.27.so::__gai_sigqueue@@GLIBC_PRIVATE',) : num blocks: 16 : total size: 851
('/lib/aarch64-linux-gnu/libc-2.27.so::__gconv_create_spec@@GLIBC_PRIVATE',) : num blocks: 1 : total size: 167
('/lib/aarch64-linux-gnu/libc-2.27.so::__gconv_transliterate@@GLIBC_PRIVATE',) : num blocks: 2 : total size: 416
('/lib/aarch64-linux-gnu/libc-2.27.so::__libc_dynarray_resize@@GLIBC_PRIVATE',) : num blocks: 1 : total size: 124
('/lib/aarch64-linux-gnu/libc-2.27.so::__libc_scratch_buffer_set@@GLIBC_PRIVATE',) : num blocks: 1 : total size: 32
('/lib/aarch64-linux-gnu/libc-2.27.so::__newlocale@@GLIBC_2.17',) : num blocks: 8 : total size: 312
('/lib/aarch64-linux-gnu/libc-2.27.so::__nss_database_lookup@@GLIBC_2.17',) : num blocks: 16 : total size: 338
('/lib/aarch64-linux-gnu/libc-2.27.so::__resolv_context_put@@GLIBC_PRIVATE',) : num blocks: 1 : total size: 88
('/lib/aarch64-linux-gnu/libc-2.27.so::__tsearch@@GLIBC_PRIVATE',) : num blocks: 8 : total size: 192
('/lib/aarch64-linux-gnu/libc-2.27.so::__vasprintf_chk@@GLIBC_2.17',) : num blocks: 504 : total size: 5693
('/lib/aarch64-linux-gnu/libc-2.27.so::__wcscoll_l@@GLIBC_2.17',) : num blocks: 1 : total size: 32
('/lib/aarch64-linux-gnu/libc-2.27.so::getprotobyname_r@@GLIBC_2.17',) : num blocks: 1 : total size: 1024
('/lib/aarch64-linux-gnu/libc-2.27.so::ntp_gettimex@@GLIBC_2.17',) : num blocks: 2 : total size: 65632
('/lib/aarch64-linux-gnu/libc-2.27.so::on_exit@@GLIBC_2.17',) : num blocks: 358 : total size: 372320
('/lib/aarch64-linux-gnu/libc-2.27.so::qsort_r@@GLIBC_2.17',) : num blocks: 1 : total size: 296
('/lib/aarch64-linux-gnu/libc-2.27.so::setlocale@@GLIBC_2.17',) : num blocks: 1 : total size: 776
('/lib/aarch64-linux-gnu/libc-2.27.so::strcspn@@GLIBC_2.17',) : num blocks: 34477 : total size: 3623174
('/lib/aarch64-linux-gnu/libc-2.27.so::textdomain@@GLIBC_2.17',) : num blocks: 4 : total size: 130
('/lib/aarch64-linux-gnu/libc-2.27.so::wcsxfrm_l@@GLIBC_2.17',) : num blocks: 2 : total size: 65
('/lib/aarch64-linux-gnu/libdl-2.27.so::dlerror@@GLIBC_2.17',) : num blocks: 7 : total size: 224
('/lib/aarch64-linux-gnu/libpthread-2.27.so::pthread_barrier_wait@@GLIBC_2.17',) : num blocks: 24 : total size: 384
('/opt/nvidia/deepstream/deepstream-6.0/lib/gst-plugins/libgstnvvideoconvert.so::(unknown)',) : num blocks: 69 : total size: 49880
('/opt/nvidia/deepstream/deepstream-6.0/lib/gst-plugins/libnvdsgst_infer.so::(unknown)',) : num blocks: 72 : total size: 50185
('/opt/nvidia/deepstream/deepstream-6.0/lib/gst-plugins/libnvdsgst_multistream.so::(unknown)',) : num blocks: 74 : total size: 50753
('/opt/nvidia/deepstream/deepstream-6.0/lib/gst-plugins/libnvdsgst_tracker.so::(unknown)',) : num blocks: 74 : total size: 50217
('/opt/nvidia/deepstream/deepstream-6.0/lib/libnvds_infer.so::(unknown)',) : num blocks: 134 : total size: 53773
('/opt/nvidia/deepstream/deepstream-6.0/lib/libnvds_inferutils.so::(unknown)',) : num blocks: 5 : total size: 5120072
('/opt/nvidia/deepstream/deepstream-6.0/lib/libnvds_nvmultiobjecttracker.so::(unknown)',) : num blocks: 1248 : total size: 287448
('/opt/nvidia/vpi1/lib64/libnvvpi.so.1.0.15::(unknown)',) : num blocks: 1820 : total size: 202227
('/usr/lib/aarch64-linux-gnu/libEGL.so.1.0.0::(unknown)',) : num blocks: 7 : total size: 1833
('/usr/lib/aarch64-linux-gnu/libEGL.so.1.0.0::eglGetProcAddress',) : num blocks: 8 : total size: 431
  :
```

### Tips

* You can run `parse_malloc_trace_log.py` on PanoJupyter, but if you prefer to run this script on other environments such as EC2, you can take following steps.
    1. Make sure that the environment is aarch64-linux platform, and `readelf` program is installed.
    1. Copy malloc_trace.{pid}.log file, and memory_map.txt file to the environment.
    1. Copy *.so files from the real execution environment to ./symbols/ directory.
    1. Run `parse_malloc_trace_log.py` script.


### Limitations

* This solution can trace malloc/free calls but cannot trace memory allocations by system calls (e.g. mmep()).
* In order to identify callers of malloc/free functions, this solution captures the return address of the functions, but the depth is limited to one.
* If memory is allocated from *.so and the *.so is unloaded without free-ing the memory, symbol name of the caller cannot be resolved.
