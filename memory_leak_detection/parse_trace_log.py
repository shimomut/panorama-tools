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
        print(d)

        op = d["op"]
        p = d["p"]

        if op==1: # alloc
            assert p not in allocated_memories
            allocated_memories[p] = ( d["size"], d["module"], d["symbol"] )

        elif op==2: # free

            if p=="(nil)":
                continue
            
            if p not in allocated_memories:
                print(f"Warning : freeing unknown memory {p}")
                continue

            del allocated_memories[p]
        
        else:
            assert f"Unknown operation : {op}"

print("Remaining memory allocations:")
for k, v in allocated_memories.items():
    print( k, v )
