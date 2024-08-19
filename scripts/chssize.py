
def chssize(size):
    if size == 720*1024:
        return (80,2,9)
    if size == 1440*1024:
        return (80,2,18)
    if size == 262963200:
        return (856,10,60)
    sectors = size//512//(255*63)
    return (sectors,255,63)
