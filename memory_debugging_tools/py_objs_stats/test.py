import os
import time
import random

import py_objs_stats
py_objs_stats.PyObjsStatsThread.instance().start()

d = []

while True:

    print("Adding elements in list")

    for i in range(100000):
        size = random.randint( 1, 1024 )
        item = ["a"] * size
        d.append(item)

    time.sleep(1)

    print("Removing elements from list")

    while d:
        i = random.randint( 0, len(d)-1 )
        del d[i]

    time.sleep(1)
