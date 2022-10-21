import sys
import os
import re
import time
import json
import threading
import traceback


def test_Logging( launcher ):

    print( "1. testing replaced stdout", flush=True )
    print( "2. testing \nmulti line \nstdout", flush=True )
    print( "3. testing ", end="", flush=True )
    print( "no ", end="", flush=True )
    print( "line end ", end="", flush=True )
    print( "char", flush=True )


def test_Logging2( launcher ):

    import random

    for i in range(100):
        for j in range(10):
            print( "%10d" % random.randint(1, 10000000), end="" )
        print("")


def test_Exception( launcher ):
    assert False, "testing assertion failure"


def test_SystemInfo( launcher ):
    print( "os.name :", os.name, flush=True )
    print( "sys.platform :", sys.platform, flush=True )
    print( "os.getcwd() :", os.getcwd(), flush=True )
    print( "sys.path :", sys.path, flush=True )
    print( "sys.argv :", sys.argv, flush=True )


def test_OsSystem( launcher ):

    os.system( launcher.params )
    

# Still experimental
def _subprocess_run( commandline ):
    
    import subprocess
    
    print( "commandline", commandline, flush=True )
    
    result = subprocess.run( [ "ls", "-al", "/" ], capture_output=True )
    
    if result.stdout:
        print( result.stdout.decode("utf-8"), flush=True )

    if result.stderr:
        print( result.stderr.decode("utf-8"), flush=True )


def _upload_files( src, dst ):

    import glob

    s3 = boto3.client("s3")

    s3_bucket, s3_prefix = splitS3Path(dst)
    
    for src_filepath in glob.glob(src):

        src_dirname, filename = os.path.split( src_filepath )
        s3_key = os.path.join( s3_prefix, filename )
        
        print( "Uploading", src_filepath, flush=True )
            
        s3.upload_file( src_filepath, s3_bucket, s3_key )

        print( "Uploaded to s3://%s/%s" % (s3_bucket, s3_key), flush=True )
    
    print( "Done.", flush=True )
    

def registerExperimentalCommands( launcher ):

    launcher.registerCommandHandler( "subprocess-run", _subprocess_run, in_main_thread=False )
    launcher.registerCommandHandler( "upload-files", _upload_files, in_main_thread=False )


def test_InfiniteLoop(launcher):
    
    while True:
        time.sleep(1)


def test_Restart( launcher ):
    
    class SigIntThread( threading.Thread ):
    
        def __init__(self):
            threading.Thread.__init__(self)

        def run(self):
    
            import signal
            pid = os.getpid()
    
            try:
                os.kill( pid, signal.SIGINT )
            except KeyboardInterrupt:
                print( "SIGINT caught in SigIntThread", flush=True )

    import signal
    pid = os.getpid()
    
    sigint_thread = SigIntThread()

    try:
        sigint_thread.start()
    
        while True:
            time.sleep(1)

    except KeyboardInterrupt:
        print( "SIGINT caught in main thread", flush=True )
        traceback.print_exc()

    sigint_thread.join()

    
def test_S3Import( launcher ):
    import foo


def test_S3Reload( launcher ):
    import foo
    import bar

    import importlib
    importlib.reload(foo)
    importlib.reload(bar)
    foo.test1()


def test_HttpGet( launcher ):
    import urllib
    with urllib.request.urlopen( launcher.params ) as fd:
        body = fd.read()
        print( body[:100], len(body), flush=True )


def test_S3Download( launcher ):
    
    def splitS3Path( s3_path ):
        re_pattern_s3_path = "s3://([^/]+)/(.*)"
        re_result = re.match( re_pattern_s3_path, s3_path )
        bucket = re_result.group(1)
        key = re_result.group(2)
        return bucket, key

    import boto3
    
    s3 = boto3.client("s3", region_name="us-west-2")

    s3_bucket, s3_key = splitS3Path( launcher.params )
    s3_filename = os.path.split(s3_key)[1]
    s3.download_file( s3_bucket, s3_key, os.path.join( "/panorama/storage", s3_filename ) )


def test_NativeModule( launcher ):

    sys.path.append( "/panorama/dynlibs" )
    #sys.path.append( "/panorama/storage" )
    
    import native_module
    print( native_module.__file__, flush=True )

    print( native_module.hello(), flush=True )
    
