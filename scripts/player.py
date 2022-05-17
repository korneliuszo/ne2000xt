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
    parser.add_argument("--type",'-t', help='monitor type')
    parser.add_argument("--drop",'-d',action=argparse.BooleanOptionalAction)
   
    args = parser.parse_args()
    if (not args.ip):
        print("Provide ip")
        sys.exit(1)
    if (not args.image):
        print("Provide image")
        sys.exit(1)
    if (not args.type):
        args.type = "cga"  
        
    from cga import cga_06
    from hga import hga
    from PIL import Image
    import monitor 
    import cv2
    import time
    
    cga = {"cga":cga_06,
            "hga": hga}[args.type](args.ip)
    
    cap = cv2.VideoCapture(args.image)
    fps = cap.get(cv2.CAP_PROP_FPS)

    display_time = time.time()

    while True:
        ret, frame = cap.read()
        if ret == False:
            break
        display_time+=1/fps
        if args.drop and display_time < time.time():
            continue
        color_coverted = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

        i = Image.fromarray(color_coverted)
        i=i.resize(cga.resolution())
        cga.display(i.convert("1",dither=Image.Dither.NONE))
    
    
    
    