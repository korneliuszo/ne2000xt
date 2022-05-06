#!/usr/bin/env python3

import socket

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
