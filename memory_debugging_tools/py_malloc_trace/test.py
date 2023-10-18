import os
import time
import random
import subprocess

d = []

for i in range(100000):
    size = random.randint( 1, 1024 )
    item = ["a"] * size
    d.append(item)

while d:
    i = random.randint( 0, len(d)-1 )
    del d[i]

# Dump memory map information
pid = os.getpid()
cmd = ["cat", f"/proc/{pid}/maps" ]
result = subprocess.run(cmd, capture_output=True)
with open(f"./memory_map.{pid}.txt","wb") as fd:
    fd.write(result.stdout)

print("Done")

