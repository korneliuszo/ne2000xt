

class int13h(object):
    def __init__(self,m,idx):
        self.m = m
        self.idx = idx
    def get_chs(self):
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
        len = params["ax"]&0xff
        data = self.read(self.tolba(self.paramstochs(params)),len)
        self.m.putmem(params["es"],params["bx"],data)
        params["ax"] = len | (0x00<<8)
    def write_cmd(self,params):
        len = params["ax"]&0xff
        data = self.m.getmem(params["es"],params["bx"],len*512)
        self.write(self.tolba(self.paramstochs(params)),data)
        params["ax"] = len | (0x00<<8)
    def get_chs_cmd(self,params):
        c,h,s = self.get_chs()
        h -= 1
        c -= 1
        params["bx"] = 0
        params["cx"] = ((c&0xff) <<8) | ((c&0x300)>>(8-6)) | s
        params["dx"] = (h << 8) | 0x01
        params["di"] = 0 # I hope DBT is not needed for now
        params["es"] = 0 
        params["ax"] = 0x0000
    def dasd_cmd(self,params):
        params["ax"] = 0x0100
    def reset_cmd(self,params):
        params["ax"] = 0x0000
    def process(self,params):
        if params["irq"] != 1:
            return False
        if params["dx"] & 0xff != self.idx:
            return False
        try:
            {
                0x00: self.reset_cmd,
                0x02: self.read_cmd,
                0x03: self.write_cmd,
                0x08: self.get_chs_cmd,
                0x15: self.dasd_cmd,
            }[params["ax"]>>8](params)
        except KeyError:
            print("Error: ",hex(params["ax"]>>8))
            params["ax"] = 0x0101
            params["rf"] |= 0x0001
        self.m.set_called_params(params)
        self.m.isr_handled()
        return True
            
            