import os
import malloc_free_trace

filename = "trace.log"

if os.path.exists(filename):
    os.unlink(filename)

malloc_free_trace.start(filename)

a = [ "1234567890" ]
b = a * 100
print(b)

malloc_free_trace.stop()
