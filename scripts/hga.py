#!/usr/bin/env python3

import monitor
from PIL import Image
from timebudget import timebudget

class hga():
    def write_b(self,idx,val):
        self.m.outb(0x3b4,idx)
        self.m.outb(0x3b5,val)
        
    def __init__(self,ip):
        self.m= monitor.monitor(ip)
        self.m.outb(0x3ba,0);
        self.m.outb(0x3bf,0x01)
        self.m.outb(0x3b8,0x0A)
        
        self.write_b(0x00,0x35)
        self.write_b(0x01,0x2d)
        self.write_b(0x02,0x2e)
        self.write_b(0x03,0x0f)
        self.write_b(0x04,0x5b)
        self.write_b(0x05,0x02)
        self.write_b(0x06,0x57)
        self.write_b(0x07,0x57)
        self.write_b(0x08,0x02)
        self.write_b(0x09,0x03)
        self.write_b(0x0a,0x00)
        self.write_b(0x0b,0x00)
        self.write_b(0x0c,0x00)
        self.write_b(0x0d,0x00)
        self.write_b(0x0e,0x00)
        self.write_b(0x0f,0x00)

        
    def resolution(self):
        return (720,348)   
    def display(self,i):
        re=bytearray(0x8000)
        for y in range(348):
            for x in range(720):
                if i.getpixel((x,y)):
                    re[(y%4)*0x2000 +(y//4)*90+x//8]|=1<<(7-x%8)
                
        with timebudget("io"):
            self.m.putmem(0xB000,0,re)
    
if __name__ == "__main__":
    import sys
    import argparse
    timebudget.set_quiet()  # don't show measurements as they happen
    timebudget.report_at_exit()  # Generate report when the program exits
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

    i = Image.open(args.image)
    cga = hga(args.ip)
    
    i=i.resize(cga.resolution())
    i=i.convert("1")
    cga.display(i)        
        
    