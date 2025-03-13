#!/usr/bin/python3
import getopt
import sys
from io import StringIO

argv = sys.argv[1:] 
  
try: 
    opts, args = getopt.getopt(argv, "i:o:k:") 
      
except: 
    print("Error") 

for opt, arg in opts: 
    if opt in ['-i']: 
        infile  = arg
    elif opt in ['-o']: 
        outfile = arg
    elif opt in ['-k']: 
        kernel = arg

with open(outfile, 'w') as outf:
    with open(infile, 'r') as inf:
        for line in inf:
            line = line.replace("$(KERNEL)",kernel)
            line = line.replace("${KERNEL}",kernel)
            line = line.replace("$KERNEL",kernel)
            outf.write(line)
inf.close()
outf.close()