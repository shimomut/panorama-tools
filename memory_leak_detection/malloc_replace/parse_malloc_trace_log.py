import argparse
import json

# ---

argparser = argparse.ArgumentParser( description='parse malloc/free trace log and detect issues' )
argparser.add_argument('logfile', action='store', help='trace log filename')
args = argparser.parse_args()

# ---

allocated_memories = {}

with open( args.logfile, "r" ) as fd:
    for line in fd:
        line = line.strip()
        d = json.loads(line)
        #print(d)

        op = d["op"]
        p = d["p"]

        if op==1: # alloc
            
            if p in allocated_memories:
                print("Warning : [alloc] already allocated :", p, allocated_memories[p], (d["size"], d["return_addr"]) )
            
            allocated_memories[p] = ( d["size"], d["return_addr"] )

        elif op==2: # free

            if p=="(nil)":
                continue
            
            if p not in allocated_memories:
                print(f"Warning : [free] freeing unknown memory {p}")
                continue

            del allocated_memories[p]
        
        elif op==3: # realloc

            p2 = d["p2"]

            if p=="(nil)":
                pass            
            elif p not in allocated_memories:
                print(f"Warning : [realloc] freeing unknown memory {p}")
            else:
                del allocated_memories[p]

            if p2 in allocated_memories:
                print("Warning : [realloc] already allocated :", p2, allocated_memories[p2], (d["size"], d["return_addr"]) )

            allocated_memories[p2] = ( d["size"], d["return_addr"] )
        
        else:
            assert f"Unknown operation : {op}"

stats = {}

print("---")
print("Remaining memory blocks:")
for p, (size,return_addr) in allocated_memories.items():
    print( p, size,return_addr )

    return_addr_s = json.dumps(return_addr)

    if return_addr_s not in stats:
        stats[return_addr_s] = [ 0, 0 ]
    
    stats[return_addr_s][0] += 1 # number of blocks
    stats[return_addr_s][1] += size # total size

print("---")
print("Num remaining memory blocks and total size:")
for caller, (num_blocks,total_size) in stats.items():
    print( caller, ": num blocks:", num_blocks, ": total size:", total_size )

