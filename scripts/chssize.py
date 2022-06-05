
def chssize(size):
    if size == 1440*1024:
        return (80,2,18)
    sectors = size/(16*63)
    return (sectors,16,63)