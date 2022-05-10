#!/usr/bin/env python3

from tqdm import tqdm
import monitor

if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')
    parser.add_argument("--no",'-n',type=int,default=0x80, help='disk no')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)

    m=monitor.monitor(args.ip)
    r=m.emptyregs()
    
    r["ax"] = 0;
    r["dx"] = args.no
    m.irq(0x13,r)
    if(r["ax"]&0xff !=0):
        print("Error")
        sys.exit(1)

    r=m.emptyregs()
    r["ax"] = 0x0800
    r["dx"] = args.no
    m.irq(0x13,r)
    if(r["ax"]>>8 !=0):
        print("Error")
        sys.exit(1)
        
    cylinders = ((r["cx"]>>8) | ((r["cx"]&0xC0)<<(8-6)))+1
    sectors = r["cx"]&0x3f
    heads = ((r["dx"] &0xff00)>>8 ) +1
    
    finfo=open(args.image+".nfo","w")
    finfo.write("C: %d H: %d S: %d\n"%(cylinders,heads,sectors))    
    chs_st = (cylinders,heads,sectors)
    max_lba = cylinders* sectors * heads
    def lba_chs(lba,chs_st):
        Temp = lba // (chs_st[2])
        Sector = (lba % chs_st[2]) + 1
        Head = Temp % (chs_st[1])
        Cylinder = Temp // (chs_st[1])
        return (Cylinder,Head,Sector)
    
    fraw=open(args.image+".raw","wb")
    
    for sector in tqdm(range(max_lba)):
        chs = lba_chs(sector,chs_st)
        for i in range(3):
            r=m.emptyregs()
            r["ax"] = 0x0201
            r["cx"] = chs[2] | ((chs[0]&0x300)>>(8-6)) | ((chs[0]&0xff)<<8)
            r["dx"] = args.no | (chs[1]<<8)
            r["es"] = 0x800
            r["bx"] = 0
            m.irq(0x13,r)
            if(r["ax"]>>8 ==0):
                break
        if (r["ax"]>>8 !=0):
            finfo.write("Bad sector %d C:%d H:%d S:%d\n"%
                (sector,chs[0],chs[1],chs[2]))
            fraw.write(bytes(512))
        else:
            fraw.write(m.getmem(0x800,0,512))       
        
        