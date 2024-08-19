#!/usr/bin/env python3

import multiprocessing
import socket

import inmememu
import monitor

import os
import time

get_time = lambda f: os.stat(f).st_ctime

def loop(m,fdd):
    time.sleep(0.5)
    if(fdd.idx&0x80):
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

if __name__ == "__main__":
    import sys
    import argparse
    import monitor
    parser = argparse.ArgumentParser(description='floppy image')
    parser.add_argument("--image",'-I', help='image')
    parser.add_argument("--drive",'-d', help='drive',type=lambda x: int(x,0),default=0)

    args = parser.parse_args()
    if (not args.image):
        print("Provide image")
        sys.exit(1)
    

    s=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(('255.255.255.255',5556))

    clients={}

    while True:
        _, addr = s.recvfrom(1024)
        print("New Hello packet",addr[0])
        t = get_time(args.image)
        if addr[0] in clients:
            clients[addr[0]]['process'].terminate() 
            clients[addr[0]]['process'].join()
            if clients[addr[0]]['time'] != t:
                print("modified, reloading")
                clients[addr[0]]['inmenu'] = inmememu.inmememu(
                            clients[addr[0]]['mon'],args.drive,args.image )
                clients[addr[0]]['time'] = t
        else:
            clients[addr[0]]={}
            clients[addr[0]]['mon'] = monitor.monitor(addr[0])
            clients[addr[0]]['inmenu'] = inmememu.inmememu(
                clients[addr[0]]['mon'],args.drive,args.image )
            clients[addr[0]]['time'] = t
        clients[addr[0]]['process'] = multiprocessing.Process(
            target = loop,
            args=(clients[addr[0]]['mon'],clients[addr[0]]['inmenu']))
        clients[addr[0]]['process'].start()


