#!/usr/bin/env python3

import socket
import struct
import copy

class monitor():
    def __init__(self,ip):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.s.connect((ip,5555))
        self.s.settimeout(5)
    def msgout(self,payload):
        self.s.send(payload)
    def msg(self,payload):
        self.msgout(payload)
        return self.s.recv(1522)
    def ping(self,data):
        return self.msg(bytes([0])+data)
    def outb(self,port,val):
        self.msg(struct.pack("<BHB",1,port,val))
        return
    def inb(self,port):
        r=self.msg(struct.pack("<BH",2,port))
        return r[0]
    def putmem_page(self,seg,addr,data):
        self.msg(struct.pack("<BHH",3,seg,addr)+data)
        return
    def putmem(self,seg,addr,data):
        [self.putmem_page(seg,addr+i,data[i:i+1024]) for i in range(0, len(data), 1024)]
        return
    def getmem_page(self,seg,addr,l):
        return self.msg(struct.pack("<BHHH",4,seg,addr, l))
    def getmem(self,seg,addr,l):
        return b"".join([self.getmem_page(seg,addr+i,min(1024,l-i)) for i in range(0, l, 1024)])
    def emptyregs(self):
        return copy.copy({"ax":0,"bx":0,"cx":0,"dx":0,
                "si":0,"di":0,"fl":0,
                "ds":0,"es":0})
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
    def putmem_page_flipped(self,seg,addr,data):
        self.msg(struct.pack("<BHHB",6,seg,addr,0)+data)
        return
    def putmem_flipped(self,seg,addr,data):
        if(len(data)%2):
            raise RuntimeError("flipped needs len % 2")
        data[0::2], data[1::2] = data[1::2], data[0::2]
        [self.putmem_page_flipped(seg,addr+i,data[i:i+1024]) for i in range(0, len(data), 1024)]
        return
    def continue_boot(self):
        self.msgout(struct.pack("<B",7))
    def get_called_params(self):
        ret=self.msg(struct.pack("<B",8))
        keys = ("irq","ax","cx","dx","bx","bp","si","di","ds","es","fl")
        values = struct.unpack("<HHHHHHHHHHH",ret)
        values = [ hex(v) for v in values]
        d = dict(zip(keys,values))
        return d