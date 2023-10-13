import sys
import argparse
import json
import re
import subprocess

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
        return f"Symbol( {self.addr_range}, {self.name} )"

class MemoryMap:

    def __init__( self, addr_range, offset, filename ):
        self.addr_range = addr_range
        self.offset = offset
        self.filename = filename
        self.symbols = None

    def __lt__(self,other):
        return self.addr_range < other.addr_range

    def __repr__(self):
        return f"MemoryMap( {self.addr_range}, {self.offset}, {self.filename} )"

class SymbolResolver:

    def __init__(self):
        self.maps = []
        self.cache = {}

    def load_mapfile( self, mapfile ):

        """
        aaaab8081000-aaaab8089000 r-xp 00000000 103:01 1304916                   /home/ubuntu/panorama-tools/memory_leak_detection/malloc_replace/malloc_replace
        """

        with open(mapfile,"r") as fd:
            for line in fd:
                line = line.strip()

                re_result = re.match( r"([0-9a-f]+)\-([0-9a-f]+) ([a-z\-]+) ([0-9a-f]+) [0-9]+\:[0-9]+ [0-9]+[ ]+(.*)", line )
                if re_result:
                    addr_range = int( re_result.group(1), 16 ), int( re_result.group(2), 16 )
                    mode = re_result.group(3)
                    offset = int( re_result.group(4), 16 )
                    filename = re_result.group(5)

                    if 'x' in mode:
                        self.maps.append( MemoryMap(addr_range, offset, filename) )

        self.maps.sort()

    def resolve_symbol( self, addr ):

        if addr in self.cache:
            return self.cache[addr]

        for memory_map in self.maps:

            if memory_map.addr_range[0] <= addr < memory_map.addr_range[1]:
                
                addr_offset_in_module = addr - memory_map.offset

                if memory_map.symbols is None:
                    memory_map.symbols = self.load_symbol_table( memory_map.filename )

                for symbol in memory_map.symbols:
                    if symbol.addr_range[0] <= addr_offset_in_module < symbol.addr_range[1]:
                        self.cache[addr] = symbol.name
                        return symbol.name
                    elif addr_offset_in_module < symbol.addr_range[0]:
                        break

                self.cache[addr] = memory_map.filename
                return memory_map.filename

            elif addr < memory_map.addr_range[0]:
                break
        
        if 0:
            print("Symbol not found :", hex(addr) )
            sys.exit(1)

        unknown = "(unknown)"
        self.cache[addr] = unknown
        return unknown

    def load_symbol_table( self, filename ):

        """
          154: 0000000000008ce0   604 FUNC    GLOBAL DEFAULT   13 mspace_independent_comall
        """

        print( "Loading symbol table :", filename )

        symbols = []

        cmd = [ "readelf", "-s", filename ]
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

        return symbols


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

        with open( filename, "r" ) as fd:

            for line in fd:
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
                
                elif op==3: # realloc

                    p2 = d["p2"]

                    if p=="(nil)":
                        pass
                    elif p not in self.allocated_memories:
                        print(f"Warning : [realloc] freeing unknown memory {p}")
                    else:
                        del self.allocated_memories[p]

                    if p2 in self.allocated_memories:
                        print("Warning : [realloc] already allocated :", p2, self.allocated_memories[p2], (d["size"], d["return_addr"]) )

                    self.allocated_memories[p2] = ( d["size"], resolve_return_addr_list(d["return_addr"]) )
                
                else:
                    assert f"Unknown operation : {op}"

        print("---")
        print("Remaining memory blocks:")
        for p, (size,return_addr) in self.allocated_memories.items():
            print( p, size,return_addr )

            if return_addr not in self.stats:
                self.stats[return_addr] = [ 0, 0 ]
            
            self.stats[return_addr][0] += 1 # number of blocks
            self.stats[return_addr][1] += size # total size

        print("---")
        print("Num remaining memory blocks and total size:")
        total_size = 0
        for caller in sorted(self.stats.keys()):
            num_blocks, size = self.stats[caller]
            total_size += size
            print( caller, ": num blocks:", num_blocks, ": total size:", size )

        print("---")
        print("Total remaining size:", total_size)
        
        

symbol_resolver = SymbolResolver()
symbol_resolver.load_mapfile( args.mapfile )

parser = MallocTraceLogParser(symbol_resolver)
parser.parse( args.logfile )
