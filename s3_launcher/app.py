import sys
import os
import re
import time
import json
import signal
import threading
import socket
import telnetlib
import importlib.abc
import importlib.util
import traceback

import boto3

# ---

region_name = "us-east-1"
#region_name = "us-west-2"

# set False for local development
ignore_keyboard_interrupt_at_top_level = True

# ---

s3 = boto3.client("s3", region_name=region_name)

# Telnet client thread to relay stdout/stderr to host machine
class TelnetThread(threading.Thread):

    instance = None

    def __init__( self, addr, port = 23 ):

        threading.Thread.__init__(self)

        self.lock = threading.Lock()

        self.addr = addr
        self.port = port
        self.telnet = None
        self.buf = []
        
        self.canceled = False

        TelnetThread.instance = self

    def _runSingleConnection(self):

        try:
            # establish connection
            while True:
                if self.canceled : break
                try:
                    self.telnet = telnetlib.Telnet( self.addr, self.port, timeout=1 )
                    break
                except ( TimeoutError, socket.timeout ):
                    continue

            # communicate
            while True:
                if self.canceled : break

                if self.buf:
                    self.lock.acquire()
                    try:
                        s = "".join(self.buf)
                        del self.buf[:]
                    finally:
                        self.lock.release()

                    self.telnet.write( s.encode("utf-8") )
                else:
                    time.sleep(0.1)

        finally:
            if self.telnet:
                self.telnet.close()
                self.telnet = None

    def run(self):
        try:
            self._runSingleConnection()
        except ConnectionError as e:
            print( e, flush=True )

    def cancel(self):
        self.canceled = True

    def write( self, s ):
        if self.telnet:
            self.lock.acquire()
            try:
                self.buf.append(s)
            finally:
                self.lock.release()


def startTelnet( addr, port ):

    if TelnetThread.instance:
        stopTelnet()

    print( "Starting telnet thread - ", addr, port, flush=True )

    telnet_thread = TelnetThread( addr, port )
    telnet_thread.start()
    
    class StdOutErrorWithTelnet:
    
        def __init__( self, original_stdouterr ):
            self.original_stdouterr = original_stdouterr

        def write( self, s ):
            self.original_stdouterr.write(s)
            self.original_stdouterr.flush()
            telnet_thread.write(s)
                    
        def flush(self):
            self.original_stdouterr.flush()

    # replace 
    sys.stdout = StdOutErrorWithTelnet( sys.__stdout__ )
    sys.stderr = StdOutErrorWithTelnet( sys.__stderr__ )

    print( "Telnet enabled", flush=True )


def stopTelnet():
    if TelnetThread.instance:
        print( "Terminating telnet thread", flush=True )
        TelnetThread.instance.cancel()
        TelnetThread.instance.join()
        TelnetThread.instance = None
        print( "Terminated telnet thread", flush=True )

def splitS3Path( s3_path ):
    re_pattern_s3_path = "s3://([^/]+)/(.*)"
    re_result = re.match( re_pattern_s3_path, s3_path )
    bucket = re_result.group(1)
    key = re_result.group(2)
    return bucket, key


def readS3Object( s3_path ):
    bucket, key = splitS3Path(s3_path)
    response = s3.get_object( Bucket = bucket, Key = key )
    data = response["Body"].read()
    return data


class S3ModuleLoader(importlib.abc.Loader):
    
    def __init__( self, s3_bucket, s3_key ):
        self.s3_bucket = s3_bucket
        self.s3_key = s3_key
        
    def create_module( self, spec ):
        return None

    def exec_module( self, module ):

        try:
            response = s3.get_object( Bucket=self.s3_bucket, Key=self.s3_key )
            self.data = response["Body"].read().decode("utf-8")
        except Exception as e:
            raise ImportError

        code = compile( self.data, self.s3_key, 'exec' )
        exec( code, module.__dict__, module.__dict__ )


class S3ModuleFinder(importlib.abc.MetaPathFinder):
    
    # singleton
    instance = None
    
    def __init__( self, s3_path ):
        self.s3_bucket, self.s3_prefix = splitS3Path(s3_path)

    def find_spec( self, fullname, path, target=None ):
    
        #print( "Finding", [ fullname, path, target ], flush=True )
        
        module_name = fullname.split(".")[-1]
        s3_key = os.path.join( self.s3_prefix, "%s.py" % module_name )
        
        try:
            response = s3.head_object( Bucket = self.s3_bucket, Key = s3_key )
        except s3.exceptions.ClientError as e:
            return None

        #print( "Loading :", s3_bucket, s3_key, flush=True )
        
        return importlib.util.spec_from_loader( fullname, S3ModuleLoader( self.s3_bucket, s3_key ) )


def enableS3ModuleLoader( s3_path ):

    if S3ModuleFinder.instance:
        sys.meta_path.remove(S3ModuleFinder.instance)
        S3ModuleFinder.instance = None
    
    s3_module_finder = S3ModuleFinder( s3_path )
    
    sys.meta_path.append(s3_module_finder)


# SQS based message queue to communicate with host machine
class MessageQueueThread( threading.Thread ):
    
    def __init__(self):
    
        threading.Thread.__init__(self)
        
        self.queue = None
        
        self.sqs_client = boto3.client( "sqs", region_name=region_name )
        self.sqs = boto3.resource( "sqs", region_name=region_name )
        
        try:
            app_id = os.environ["AppGraph_Uid"]
        except KeyError:
            app_id = ""

        queue_name = "panorama-sqs-queue-" + app_id
        print( "SQS queue name :", queue_name, flush=True )
    
        try:
            self.queue = self.sqs.get_queue_by_name( QueueName = queue_name )
        except self.sqs_client.exceptions.QueueDoesNotExist:
            self.queue = self.sqs.create_queue( QueueName = queue_name )
        
        self.makeQueueEmpty()
        
        self.is_canceled = False
        self.messages = []
        self.command_handlers = {}

    def cancel(self):
        self.is_canceled = True
    
    def destroy(self):
        
        self.queue.delete()
        self.queue = None
    
    def makeQueueEmpty(self):
        messages = self.queue.receive_messages()
        for msg in messages:
            msg.delete()
    
    def getMessage(self):
        
        if not self.messages:
            return None
        
        msg = self.messages.pop(0)
        return msg

    def run(self):
    
        while True:

            params = {
                "WaitTimeSeconds" : 1
            }

            messages = self.queue.receive_messages( **params )
            if messages:
                for sqs_msg in messages:
                    sqs_msg.delete()
                    
                    try:
                        msg = json.loads(sqs_msg.body)
                    except json.decoder.JSONDecodeError as e:
                        print( "Json decode error :", e, flush=True )
                        continue
                    
                    if "command" not in msg:
                        print( "command name not included in the payload", flush=True )
                        continue

                    command = msg["command"]

                    if (command,True) in self.command_handlers:
                        # run this command in main thread
                        self.messages.append(msg)

                    elif (command,False) in self.command_handlers:

                        # run this command in message queue thread
                        command_handler = self.command_handlers[ command, False ]

                        args = msg.copy()
                        del args["command"]

                        try:
                            command_handler(**args)
                        except KeyboardInterrupt:
                            pass
                        except:
                            traceback.print_exc()
                            sys.stderr.flush()

                    else:
                        print( "Unknown command name", command, flush=True )
                        continue
        
            if self.is_canceled:
                break


# S3 code launcher to accelerate development iterations
class S3Launcher:

    def __init__(self):

        self.message_queue = MessageQueueThread()
        self.message_queue.start()
        
        # commands to run in main thread
        self.registerCommandHandler( "run", self.command_Run, in_main_thread = True )
        self.registerCommandHandler( "quit", self.command_Quit, in_main_thread = True )
        
        # commands to run in message queue thread
        self.registerCommandHandler( "interrupt", self.command_Interrupt, in_main_thread = False )
        self.registerCommandHandler( "telnet", self.command_Telnet, in_main_thread = False )
        self.registerCommandHandler( "subprocess-run", self.command_SubprocessRun, in_main_thread=False )
        self.registerCommandHandler( "upload-files", self.command_UploadFiles, in_main_thread=False )
        self.registerCommandHandler( "num-objects", self.command_NumObjects, in_main_thread=False )
        self.registerCommandHandler( "referer-tree", self.command_RefererTree, in_main_thread=False )
        
        self.quit = False

    def destroy(self):
        self.message_queue.cancel()
        self.message_queue.join()

    def registerCommandHandler( self, name, handler, in_main_thread=False ):
        self.message_queue.command_handlers[ name, in_main_thread ] = handler

    def loop(self):

        while True:
            try:
                self.waitAndProcessMessage(self.message_queue)
            except KeyboardInterrupt:
                if ignore_keyboard_interrupt_at_top_level:
                    print( "No command is being executed. Ignoring KeyboardInterrupt.", flush=True )
                else:
                    break
            except:
                traceback.print_exc()
                sys.stderr.flush()
                
            print("-----", flush=True)
            print("", flush=True)

            if self.quit:
                break

            time.sleep(0.1)

    def waitAndProcessMessage(self,message_queue):
    
        print( "Waiting for launcher command ...", flush=True )
        
        while True:
            msg = message_queue.getMessage()
            if msg:
                break
            time.sleep(0.1)
    
        print( "Received message:", msg, flush=True )
    
        try:
            command = msg["command"]
            args = msg.copy()
            del args["command"]
            
            command_handler = self.message_queue.command_handlers[ command, True ]
        
        except KeyError as e:
            print( "Message error : Attribute not found :", e, flush=True )
            return
        
        command_handler( **args )
    
    def command_Run( self, s3_path, funcname, params ):

        namespace = {}
    
        name = os.path.basename(s3_path)
        fileimage = readS3Object(s3_path)

        enableS3ModuleLoader( os.path.split(s3_path)[0] )
        
        print("-----", flush=True)
        
        try:
            # compile and run
            code = compile( fileimage, name, 'exec' )
            exec( code, namespace, namespace )
        
            if funcname:
                # call function
                func = namespace[funcname]
                self.params = params
                func(self)

        except KeyboardInterrupt:
            traceback.print_exc()
            sys.stderr.flush()
    
    def command_Quit( self ):
        self.quit = True
        
    def command_Interrupt( self ):

        print( "Sending SIGINT", flush=True )

        pid = os.getpid()
        os.kill( pid, signal.SIGINT )

    def command_Telnet( self, addr, port ):

        startTelnet( addr, port )

    def command_SubprocessRun( self, commandline ):
    
        import subprocess
    
        print( "subprocess-run :", commandline, flush=True )
    
        result = subprocess.run( commandline, shell=True, capture_output=True )
    
        if result.stdout:
            print( result.stdout.decode("utf-8"), flush=True )

        if result.stderr:
            print( result.stderr.decode("utf-8"), flush=True )

    def command_UploadFiles( self, src, dst ):

        print( "upload-files :", [src, dst], flush=True )

        import glob

        s3_bucket, s3_prefix = splitS3Path(dst)
        
        src_filepath_list = glob.glob(src)
        if not src_filepath_list:
            print( f"No file found which match the pattern [{src}]" )
            return
        
        for src_filepath in src_filepath_list:

            src_dirname, filename = os.path.split( src_filepath )
            s3_key = os.path.join( s3_prefix, filename )
        
            print( "Uploading", src_filepath, flush=True )
            
            s3.upload_file( src_filepath, s3_bucket, s3_key )

            print( "Uploaded to s3://%s/%s" % (s3_bucket, s3_key), flush=True )
    
        print( "Done.", flush=True )

    def command_NumObjects(self):
    
        import gc
    
        print( 'Num objects :', flush=True )

        gc.collect()
        objs = gc.get_objects()
        stat = {}
    
        for obj in objs:
            str_type = str(type(obj))
            if str_type not in stat:
                stat[str_type] = 0
            stat[str_type] += 1

        max_len = 10
        for k in sorted(stat):
            max_len = max( max_len, len(k) )

        for k in sorted(stat):
            print( "  %s%s : %d" % ( k, ' '*(max_len-len(k)), stat[k] ), flush=True )

        print( 'Done.\n', flush=True )

    def command_RefererTree( self, pattern, depth ):

        import gc
        import fnmatch

        gc.collect()
        objs = gc.get_objects()
    
        def _dumpReferers( obj, current_depth, known_ids ):
        
            if id(obj) in known_ids:
                return
            known_ids.add( id(obj) )
        
            print( "   " * current_depth, str(type(obj)) )

            if current_depth>=depth: return

            referers = gc.get_referrers(obj)
            for referer in tuple(referers):
                _dumpReferers( referer, current_depth+1, known_ids )
    
        print( 'Referers to objects [%s] :' % pattern, flush=True )
        pattern = "*" + pattern + "*"
    
        for obj in tuple(objs):
            if fnmatch.fnmatch( repr(type(obj)), pattern ):
                known_ids = set()
                _dumpReferers( obj, 0, known_ids )

        print( 'Done.\n', flush=True )

   
print( "Starting S3 launcher", flush=True )

s3_launcher = S3Launcher()
s3_launcher.loop()

print( "Terminating S3 launcher", flush=True )

s3_launcher.destroy()

if TelnetThread.instance:
    stopTelnet()

