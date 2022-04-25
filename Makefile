

.PHONY: clean all default

default: optionrom.bin

all: default

%.obj: %.c 
	wcc -0 -wx -zu -s -oas -hd -d2 -ms -za99 -fo$@ $<

%.obj: %.S 
	wasm -0 -wx -ms -fo$@ $<


optionrom.bin: main.obj init.obj print.obj link.lnk
	wlink name $@ @link.lnk
	./checksum.py $@
