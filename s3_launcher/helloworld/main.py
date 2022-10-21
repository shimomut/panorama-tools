import importlib
import hello

def run( launcher ):
    
    print( "params :", launcher.params, flush=True )
    
    importlib.reload(hello)
    hello.hello()

