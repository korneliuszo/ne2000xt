import struct

class int13h(object):
    def __init__(self,m,idx):
        self.m = m
        self.idx = idx
    def get_chs(self):
        raise NotImplementedError
    def get_size(self):
        raise NotImplementedError
    def read(self,sector,len):
        raise NotImplementedError
    def write(self,sector,data):
        raise NotImplementedError
    def tolba(self,chs):
        c,h,s = self.get_chs()
        return (chs[0]*h + chs[1])*s + chs[2] - 1
    def paramstochs(self,params):
        c = params["cx"]>>8 | ((params["cx"]&0xC0)<<(8-6))
        h = ((params["dx"] &0xff00)>>8 )
        s = params["cx"]&0x3f
        return (c,h,s)
    def read_cmd(self,params):
        l = params["ax"]&0xff
        data = self.read(self.tolba(self.paramstochs(params)),l)
        self.m.putmem(params["es"],params["bx"],data)
        params["ax"] = l | (0x00<<8)
        params["rf"] &= ~0x0001
        return True
    def write_cmd(self,params):
        l = params["ax"]&0xff
        data = self.m.getmem(params["es"],params["bx"],l*512)
        self.write(self.tolba(self.paramstochs(params)),data)
        params["ax"] = l | (0x00<<8)
        params["rf"] &= ~0x0001
        return True
    def get_chs_cmd(self,params):
        c,h,s = self.get_chs()
        if c > 1024:
            c=1024
        h -= 1
        c -= 1
        params["bx"] = 0
        params["cx"] = ((c&0xff) <<8) | ((c&0x300)>>(8-6)) | s
        params["dx"] = (h << 8) | 0x01
        if not (self.idx & 0x80): # hard disks DON'T touch that 
            params["di"] = 0 # I hope DBT is not needed for now
            params["es"] = 0
        params["ax"] &= 0x00ff
        params["rf"] &= ~0x0001
        return True
    def dasd_cmd(self,params):
        params["ax"] = 0x0003
        lba = self.get_size()//512
        params["cx"] = lba>>16
        params["dx"] = lba &0xffff
        params["rf"] &= ~0x0001        
        return True
    def ext_check(self,params):
        params["bx"] = 0xAA55
        params["cx"] = 0x01;
        params["ax"] = 0x2100 | (params["ax"]&0xff)
        params["rf"] &= ~0x0001        
        return True
    def read_dap(self,params):
        data = self.m.getmem(params["ds"],params["si"],12)
        return dict(zip(("len","u","sectors","off","seg","lbal"),
                        struct.unpack("<BBHHHI",data)))
    def update_dap(self,params,dap):
        data = struct.pack("<BBHHHI",*(dap.values()))
        self.m.putmem(params["ds"],params["si"],data)
    def ext_read(self,params):
        dap=self.read_dap(params)
        data = self.read(dap["lbal"],dap["sectors"])
        self.m.putmem(dap["seg"],dap["off"],data)
        params["ax"] &= 0x00ff
        params["rf"] &= ~0x0001
        return True
    def ext_write(self,params):
        dap=self.read_dap(params)
        data = self.m.getmem(dap["seg"],dap["off"],dap["sectors"]*512)
        self.write(dap["lbal"],data)
        params["ax"] &= 0x00ff
        params["rf"] &= ~0x0001
        return True
    def ext_params(self,params):
        c,h,s=self.get_chs()
        lba = self.get_size()//512        
        data = struct.pack("<HHIIIQHI",
                           0x1E,
                           0,
                           c,h,s,lba,512,0)
        self.m.putmem(params["ds"],params["si"],data)
        params["ax"] &= 0x00ff
        params["rf"] &= ~0x0001
        return True
    def reset_cmd(self,params):
        params["ax"] = 0x0000
        params["rf"] &= ~0x0001
        return True
    def not_implemented_no_error(self,params):
        params["ax"] = 0x0101
        params["rf"] |= 0x0001
        return True
    def implemented_noop(self,params):
        params["ax"] = 0x0000
        params["rf"] &= ~0x0001
        return True
    def process(self,params):
        if params["irq"] != 1:
            return False
        if params["dx"] & 0xff != self.idx:
            return False
        try:
            ret = {
                0x00: self.reset_cmd,
                0x02: self.read_cmd,
                0x03: self.write_cmd,
                0x08: self.get_chs_cmd,
                0x15: self.dasd_cmd,
                0x23: self.not_implemented_no_error, #set features
                0x24: self.not_implemented_no_error, #set multiple blocks
                0x25: self.not_implemented_no_error, #identify drive
                0x01: self.implemented_noop, # status
                0x09: self.implemented_noop, # init disk
                0x0d: self.implemented_noop, # areset
                0x0c: self.implemented_noop, # seek
                0x47: self.implemented_noop, # eseek
                0x10: self.implemented_noop, # driveready
                0x11: self.implemented_noop, # recalibrate
                0x14: self.implemented_noop, # diagnostic
                0x04: self.implemented_noop, # verify
                0x41: self.ext_check,
                0x42: self.ext_read,
                0x43: self.ext_write,
                0x44: self.implemented_noop, #ext_verify
                0x48: self.ext_params,

            }[params["ax"]>>8](params)
        except KeyError:
            print("Error: ",hex(params["ax"]>>8))
            params["ax"] = 0x0101
            params["rf"] |= 0x0001
            ret=True
        if ret:
            self.m.set_called_params(params)
            self.m.isr_handled()
        return ret
            
            
