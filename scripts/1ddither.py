#!/usr/bin/env python3

from timebudget import timebudget
timebudget.set_quiet()  # don't show measurements as they happen
timebudget.report_at_exit()  # Generate report when the program exits

if __name__ == "__main__":
    import sys
    import argparse
    parser = argparse.ArgumentParser(description='Display image')
    parser.add_argument("--ip",'-i', help='ip')
    parser.add_argument("--image",'-I', help='image')

    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)
        
    from cga import cga_06
    from PIL import Image
    import monitor
    import numpy as np
    from time import sleep
    import queue
    import threading
    
    i = Image.open(args.image)
    cga = cga_06(args.ip)
    
    i=i.resize(cga.resolution())
    w, h=cga.resolution()
    i=i.convert("L")
    q= queue.Queue(10)
    
    def send_display():
        while True:
            item = q.get()
            cga.display(item) 
            q.task_done()
        
    threading.Thread(target=send_display, daemon=True).start()
    
    while True:
        ia = np.array(i,"float")
        with timebudget("dithering"):
            for y in range(h):
                quantization_error = np.random.randint(255)
                for x in range(w):
                    old_pixel = ia[y, x]
                    old_pixel += quantization_error
                    new_pixel = 255.0 if old_pixel > 127.0 else 0.0
                    quantization_error = old_pixel - new_pixel
                    ia[y, x] = new_pixel
            im = Image.fromarray(np.array(ia,"uint8"),mode="L").convert("1")
        q.put(im)