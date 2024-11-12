#  PYTHONPATH=$PWD nbdkit -f -v --filter=blocksize python nbd.py ip=127.0.0.1

import nbdkit

import monitor

API_VERSION = 2

ip = None

# Parse the url parameter.
def config(key, value):
    global ip
    if key == "ip":
        ip = value
    else:
        raise RuntimeError("unknown parameter: " + key)


def config_complete():
    global ip
    if ip is None:
        raise RuntimeError("ip parameter is required")
    
class hdd():
    def __init__(self,ip,diskno=0x80):
        self.diskno = diskno
        self.m=monitor.monitor(ip)
        
        r=self.m.emptyregs()
    
        r["ax"] = 0;
        r["dx"] = self.diskno
        self.m.irq(0x13,r)
        if(r["ax"]&0xff !=0 and r["ax"] != 0x01):
            print("Error")
            sys.exit(1)

        r=self.m.emptyregs()
        r["ax"] = 0x0800
        r["dx"] = self.diskno
        self.m.irq(0x13,r)
        if(r["ax"]>>8 !=0):
            print("Error")
            sys.exit(1)
        
        cylinders = ((r["cx"]>>8) | ((r["cx"]&0xC0)<<(8-6)))+1
        sectors = r["cx"]&0x3f
        heads = ((r["dx"] &0xff00)>>8 ) +1
        
        self.chs_st = (cylinders,heads,sectors)
        self.size = cylinders* sectors * heads *512
    def lba_chs(self,lba):
        Temp = lba // (self.chs_st[2])
        Sector = (lba % self.chs_st[2]) + 1
        Head = Temp % (self.chs_st[1])
        Cylinder = Temp // (self.chs_st[1])
        return (Cylinder,Head,Sector)
    
    def read_sector(self,lba):
        chs = self.lba_chs(lba)
        r=self.m.emptyregs()
        r["ax"] = 0x0201
        r["cx"] = chs[2] | ((chs[0]&0x300)>>(8-6)) | ((chs[0]&0xff)<<8)
        r["dx"] = self.diskno | (chs[1]<<8)
        r["es"] = 0x800
        r["bx"] = 0
        self.m.irq(0x13,r)
        if(r["ax"]>>8 !=0):
            raise RuntimeError("Bad sector" + str(lba))
        return self.m.getmem(0x800,0,512)

    def write_sector(self,lba,buff):
        self.m.putmem(0x800,0,buff)
        chs = self.lba_chs(lba)
        r=self.m.emptyregs()
        r["ax"] = 0x0301
        r["cx"] = chs[2] | ((chs[0]&0x300)>>(8-6)) | ((chs[0]&0xff)<<8)
        r["dx"] = self.diskno | (chs[1]<<8)
        r["es"] = 0x800
        r["bx"] = 0
        self.m.irq(0x13,r)
        if(r["ax"]>>8 !=0):
            raise RuntimeError("Bad sector" + str(lba))
        return

   # This is called when a client connects.
def open(readonly):
    return hdd(ip)

def get_size(h):

    return h.size

def pread(h, buf, offset, flags):
    if(len(buf)!=512):
        raise RuntimeError("Read only in 512bytes chunks")
        
    b=h.read_sector(offset//512)
    buf[:] = b

def pwrite(h, buf, offset, flags):
    if(len(buf)!=512):
        raise RuntimeError("Read only in 512bytes chunks")
    h.write_sector(offset//512, buf)

def block_size(h):
    return (512,512,512)
