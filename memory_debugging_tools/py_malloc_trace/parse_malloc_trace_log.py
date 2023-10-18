import os
import sys
import argparse
import json
import re
import subprocess
import pprint

# ---

argparser = argparse.ArgumentParser( description='parse malloc/free trace log and detect issues' )
argparser.add_argument('--mapfile', action='store', required=True, help='memory map filename (/proc/{pid}/maps format)')
argparser.add_argument('--logfile', action='store', required=True, help='trace log filename')
args = argparser.parse_args()

# ---

class Symbol:

    def __init__( self, addr_range, name ):
        self.addr_range = addr_range
        self.name = name

    def __lt__(self,other):
        return self.addr_range < other.addr_range

    def __repr__(self):
        return f"Symbol( [{hex(self.addr_range[0])}, {hex(self.addr_range[1])}], {self.name} )"

class MemoryMap:

    def __init__( self, addr_range, offset, filename ):
        self.addr_range = addr_range
        self.offset = offset
        self.filename = filename
        self.symbols = None

    def __lt__(self,other):
        return self.addr_range < other.addr_range

    def __repr__(self):
        return f"MemoryMap( [{hex(self.addr_range[0])}, {hex(self.addr_range[1])}], {self.offset}, {self.filename} )"

class SymbolResolver:

    def __init__(self):
        self.maps = []
        self.cache = {}
        self.unresolved = []

    def load_mapfile( self, mapfile ):

        """
        aaaab8081000-aaaab8089000 r-xp 00000000 103:01 1304916                   /home/ubuntu/panorama-tools/memory_leak_detection/py_malloc_trace/py_malloc_trace
        """

        print( "Loading memory map info :", mapfile )

        with open(mapfile,"r") as fd:
            for line in fd:
                line = line.strip()

                re_result = re.match( r"([0-9a-f]+)\-([0-9a-f]+) ([a-z\-]+) ([0-9a-f]+) [0-9a-f]+\:[0-9a-f]+ [0-9]+[ ]+(.*)", line )
                if re_result:
                    addr_range = int( re_result.group(1), 16 ), int( re_result.group(2), 16 )
                    mode = re_result.group(3)
                    offset = int( re_result.group(4), 16 )
                    filename = re_result.group(5)

                    if 'x' in mode:
                    
                        if offset != 0:
                            print(line)
                        assert offset==0

                        self.maps.append( MemoryMap(addr_range, offset, filename) )

        self.maps.sort()

        pprint.pprint(self.maps)

    def resolve_symbol( self, addr ):

        if addr in self.cache:
            return self.cache[addr]

        for memory_map in self.maps:

            if memory_map.addr_range[0] <= addr < memory_map.addr_range[1]:
                
                addr_offset_in_module = addr - memory_map.addr_range[0]

                if memory_map.symbols is None:
                    memory_map.symbols = self.load_symbol_table( memory_map.filename )

                for symbol in memory_map.symbols:
                    if symbol.addr_range[0] <= addr_offset_in_module < symbol.addr_range[1]:
                        name = memory_map.filename + "::" + symbol.name
                        self.cache[addr] = name
                        return name
                    elif addr_offset_in_module < symbol.addr_range[0]:
                        break

                name = memory_map.filename + "::" + "(unknown)"
                self.cache[addr] = name
                return name

            elif addr < memory_map.addr_range[0]:
                break
        
        if len(self.unresolved) < 10:
            self.unresolved.append(addr)

        name = "(unknown)" + "::" + "(unknown)"
        self.cache[addr] = name
        return name

    def load_symbol_table( self, filename ):

        """
          154: 0000000000008ce0   604 FUNC    GLOBAL DEFAULT   13 mspace_independent_comall
        """

        local_symbol_filename = os.path.join( "./symbols", filename.lstrip("/") )
        if os.path.exists(local_symbol_filename):
            symbol_filename = local_symbol_filename
        else:
            symbol_filename = filename

        print( "Loading symbol table :", symbol_filename )

        symbols = []

        cmd = [ "readelf", "-s", symbol_filename ]
        result = subprocess.run( cmd, capture_output=True )

        for line in result.stdout.decode("utf-8").splitlines():
            line = line.strip()

            re_result = re.match( "[0-9]+\: ([0-9a-f]+)[ ]+([0-9a-f]+) [A-Z]+[ ]+[A-Z]+[ ]+[A-Z]+[ ]+[0-9A-Z]+ (.*)", line )
            if re_result:
                addr = int( re_result.group(1), 16 )
                size = int( re_result.group(2), 16 )
                name = re_result.group(3)

                symbols.append( Symbol( (addr, addr+size), name ) )

        symbols.sort()

        print( f"Found {len(symbols)} symbols" )

        return symbols

    def load_symbol_table_all(self):
        for memory_map in self.maps:
            if memory_map.symbols is None:
                memory_map.symbols = self.load_symbol_table( memory_map.filename )
    
    def print_unresolved(self):

        if self.unresolved:
            print("")
            print("Unresolved addresses (first 10):")
            for addr in sorted(self.unresolved):
                print( hex(addr) )


class MallocTraceLogParser:

    def __init__( self, symbol_resolver ):
        self.symbol_resolver = symbol_resolver
        self.allocated_memories = {}
        self.stats = {}

    def parse( self, filename ):

        def resolve_return_addr_list(return_addr_list):
            result = []
            for return_addr in return_addr_list:
                name = self.symbol_resolver.resolve_symbol( int(return_addr,16) )
                result.append(name)
            return tuple(result)

        print("")
        print( "Parsing trace log :", filename )
        
        with open( filename, "r" ) as fd:

            for lineno, line in enumerate(fd):

                if lineno % 100000==0:
                    print(".", end="", flush=True)
                
                line = line.strip()
                try:
                    d = json.loads(line)
                except json.decoder.JSONDecodeError:
                    print( "Malformed JSON :", [line] )
                    continue

                #print(d)

                op = d["op"]
                p = d["p"]

                if op==1: # alloc
                    
                    if p in self.allocated_memories:
                        print("Warning : [alloc] already allocated :", p, self.allocated_memories[p], (d["size"], d["return_addr"]) )
                    
                    self.allocated_memories[p] = ( d["size"], resolve_return_addr_list(d["return_addr"]) )

                elif op==2: # free

                    if p=="(nil)":
                        continue
                    
                    if p not in self.allocated_memories:
                        print(f"Warning : [free] freeing unknown memory {p}")
                        continue

                    del self.allocated_memories[p]
                
                else:
                    assert f"Unknown operation : {op}"

        print("\n")
        print("Num remaining memory blocks and total size:")
        for p, (size,return_addr) in self.allocated_memories.items():
            
            #print( p, size,return_addr )

            if return_addr not in self.stats:
                self.stats[return_addr] = [ 0, 0 ]
            
            self.stats[return_addr][0] += 1 # number of blocks
            self.stats[return_addr][1] += size # total size

        total_size = 0
        for caller in sorted(self.stats.keys()):
            num_blocks, size = self.stats[caller]
            total_size += size
            print( caller, ": num blocks:", num_blocks, ": total size:", size )

        print("")
        print("Total remaining size:", total_size)
        

symbol_resolver = SymbolResolver()
symbol_resolver.load_mapfile( args.mapfile )
symbol_resolver.load_symbol_table_all()

parser = MallocTraceLogParser(symbol_resolver)
parser.parse( args.logfile )

symbol_resolver.print_unresolved()
