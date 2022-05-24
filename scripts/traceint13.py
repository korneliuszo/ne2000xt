#!/usr/bin/env python3

import monitor

if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
        
    m=monitor.monitor(args.ip)
    m.install_13h()
    m.continue_boot()
    while True:
        m.wait_for_isr()
        print(m.get_called_params())
        m.continue_boot()
        
        
        