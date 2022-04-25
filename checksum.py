#!/usr/bin/env python3

import sys

SIZE=16*1024

a=open(sys.argv[1],"rb").read()

a=a[:SIZE-1]
a=a+b"\x00"*(SIZE-len(a)-1)
a=a+bytes([(-sum(a))%256])

open(sys.argv[1],"wb").write(a)
