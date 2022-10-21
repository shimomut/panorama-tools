
print( __name__, 1, flush=True )

import bar

def test1():
    print( "test1()", flush=True )
    bar.test2()

print( __name__, 9, flush=True )

