#!/usr/bin/env python3
import int13handler
import chssize

class inmememu(int13handler.int13h): 
    def __init__(self,m,idx,image):
        super().__init__(m,idx)
        self.image = bytearray(open(image,"rb").read())

    def get_chs(self):
        return chssize.chssize(len(self.image))
    def read(self,sector,len):
        return self.image[sector*512:(sector+len)*512]
    def write(self,sector,data):
        l = len(data)
        self.image[sector*512:sector*512+l] = data

if __name__ == "__main__":
    import sys
    import argparse
    import monitor
    parser = argparse.ArgumentParser(description='floppy image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')
    
    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)

    m=monitor.monitor(args.ip)
    fdd=inmememu(m,0x00,args.image)
    m.install_13h()
    m.continue_boot()
    while True:
        m.wait_for_isr()
        params=m.get_called_params()
        if fdd.process(params):
            continue
        m.continue_boot()
    