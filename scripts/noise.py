#!/usr/bin/env python3

from timebudget import timebudget
timebudget.set_quiet()  # don't show measurements as they happen
timebudget.report_at_exit()  # Generate report when the program exits

if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)
        
    from cga import cga_06
    from PIL import Image
    import monitor
    import numpy as np
    from time import sleep
    
    i = Image.open(args.image)
    cga = cga_06(args.ip)
    
    i=i.resize(cga.resolution())
    w, h=cga.resolution()
    i=i.convert("L")
    while True:
        noise =  Image.fromarray(np.random.randint(0,255,(h,w),dtype=np.dtype('uint8')),mode="L")
        im = Image.blend(i,noise,0.4).convert("1",dither=Image.Dither.NONE)
        cga.display(im) 
        