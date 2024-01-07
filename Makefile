

.PHONY: clean all default

default: optionrom.bin

all: default

%.obj: %.c delay.h inlines.h biosint.h dp8390reg.h eth.h
	wcc -0 -wx -s -oatx -hd -d0 -ms -zu -za99  -fo$@ $<

%.obj: %.S 
	wasm -0 -wx -ms -fo$@ $<


optionrom.bin: main.obj init.obj print.obj link.lnk delay_c.obj delay.obj eth.obj insw.obj
	wlink name $@ @link.lnk
	./checksum.py $@
