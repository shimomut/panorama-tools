import random

d = []

for i in range(100000):
    size = random.randint( 1, 1024 )
    item = ["a"] * size
    d.append(item)

while d:
    i = random.randint( 0, len(d)-1 )
    del d[i]

print("Done")

