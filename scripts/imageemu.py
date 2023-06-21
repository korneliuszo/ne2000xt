#!/usr/bin/env python3
import int13handler
import chssize

class imageemu(int13handler.int13h): 
    def __init__(self,m,idx,image):
        super().__init__(m,idx)
        self.image = open(image,"r+b")

    def get_chs(self):
        return chssize.chssize(self.get_size())
    def get_size(self):
        self.image.seek(0,2)
        return self.image.tell()
    def read(self,sector,len):
        self.image.seek(sector*512)
        return self.image.read(len*512)
    def write(self,sector,data):
        self.image.seek(sector*512)        
        self.image.write(data)

if __name__ == "__main__":
    import sys
    import argparse
    import monitor
    parser = argparse.ArgumentParser(description='floppy image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')
    parser.add_argument("--drive",'-d', help='drive',type=lambda x: int(x,0),default=0)
    
    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)

    m=monitor.monitor(args.ip)
    fdd=imageemu(m,args.drive,args.image)
    if(args.drive&0x80):
        data = bytearray(m.getmem(0,0x475,1))
        data[0]+=1;
        m.putmem(0,0x475,data)

    m.install_13h()
    m.continue_boot()
    while True:
        m.wait_for_isr()
        params=m.get_called_params()
        if fdd.process(params):
            continue  
        m.continue_boot()
    
