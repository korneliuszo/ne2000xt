.8086

_TEXT  segment word public 'CODE'
assume  cs:_TEXT
dw 0AA55h
db 32
public entry
entry:
	jmp short install_irq

install_irq label near
	push ds
	push ax
	mov ax, 0
	mov ds, ax ; load first page
	mov ah, 0x02
	int 0x16
	and al, 0x04
	jnz SHORT SkipRomInitialization

	mov WORD PTR ds:[019h*4], offset _TEXT:init
	mov WORD PTR ds:[019h*4+2], cs

SkipRomInitialization:
	pop ax
	pop ds
	retf

assume  cs:_TEXT
extrn _start: near
init proc near

	mov bx, cs
	mov ds, bx
	mov bx, 0x1000
	mov es, bx

	cld

    mov cx, 0x2000            ; 8k WORDs in image
    mov si, 0x0000            ; Current code Address
    mov di, 0x0000            ; New code Address
    rep movsw                 ; Copy MBR
	
	push es
	mov ax, offset _TEXT:jumpram
	push ax
	retf

jumpram label near
   	mov ax,DGROUP
	mov bx, cs
	add ax, bx
    assume  ds:DGROUP
	mov	ds, ax
    assume  es:DGROUP
	mov es, ax


	sti
	cld

	call _start
abc:
	hlt
	jmp short abc
	retf
init	endp
_TEXT ends
	end
