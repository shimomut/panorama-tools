import os
import gc
import time
import threading

class PyObjsStatsThread(threading.Thread):

    _instance = None

    interval = 60  # in seconds

    output_dirname = "/tmp"
    #output_dirname = "."

    @staticmethod
    def instance():
        if not PyObjsStatsThread._instance:
            PyObjsStatsThread._instance = PyObjsStatsThread()
        return PyObjsStatsThread._instance

    def __init__(self):

        super().__init__(daemon=True)
        self.canceled = False
        self.stats_number = 0
        self.pid = os.getpid()

    def run(self):

        while True:

            self.write_stats()

            for i in range(self.interval):
                
                if self.canceled:
                    return

                time.sleep(1)

    def write_stats(self):

        self.stats_number += 1

        gc.collect()
        objs = gc.get_objects()
        stat = {}
        
        for obj in objs:
            type_s = str(type(obj))
        
            if type_s not in stat:
                stat[type_s] = 0
            stat[type_s] += 1
        
        filename = os.path.join( self.output_dirname, f"py_objs_stats.{self.pid}.{self.stats_number}.txt" )
        #print("Writing",filename)

        with open( filename, "w" ) as fd:
            for k in sorted(stat.keys()):
                fd.write( f"{k} : {stat[k]}\n" )
