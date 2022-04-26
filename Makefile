

.PHONY: clean all default

default: optionrom.bin

all: default

%.obj: %.c delay.h inlines.h biosint.h
	wcc -0 -wx -zu -s -oas -hd -d2 -ms -za99 -fo$@ $<

%.obj: %.S 
	wasm -0 -wx -ms -fo$@ $<


optionrom.bin: main.obj init.obj print.obj link.lnk delay_c.obj delay.obj
	wlink name $@ @link.lnk
	./checksum.py $@
