#!/usr/bin/env python3

import socket
import struct

class monitor():
    def __init__(self,ip):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.connect((ip,5555))
        self.s.settimeout(2)
    def msgout(self,payload):
        self.s.send(payload)
    def msg(self,payload):
        self.msgout(payload)
        return self.s.recv(1522)
    def ping(self,data):
        return self.msg(bytes([0])+data)
    def putmem_page(self,seg,addr,data):
        self.msg(struct.pack("<BHH",3,seg,addr)+data)
        return
    def putmem(self,seg,addr,data):
        [self.putmem_page(seg,addr+i,data[i:i+1024]) for i in range(0, len(data), 1024)]
        return
    def getmem_page(self,seg,addr,l):
        return self.conn.msg(struct.pack("<BHHH",4,seg,addr, l))
    def getmem(self,seg,addr,l):
        return b"".join([self.getmem_page(seg,addr+i,min(1024,l-i)) for i in range(0, l, 1024)])
    def emptyregs(self):
        return {"ax":0,"bx":0,"cx":0,"dx":0,
                "si":0,"di":0,"fl":0,
                "ds":0,"es":0}
    def irq(self,irq,regs):
        cmd = bytes([5]) + struct.pack("<BHHHHHHHHH",
                irq,
                regs["ax"],
                regs["bx"],
                regs["cx"],
                regs["dx"],
                regs["di"],
                regs["si"],
                regs["fl"],
                regs["ds"],
                regs["es"])
        ret = self.msg(cmd)   
        keys = ("ax","bx","cx","dx","di","si","fl")
        values = struct.unpack("<HHHHHHH",ret)
        d = dict(zip(keys,values))
        return regs.update(d)

