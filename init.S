.8086

_TEXT  segment word public 'CODE'
assume  cs:_TEXT
dw 0AA55h
db 32
public entry
entry:
	jmp short install_irq

install_irq label near
	pushf
	push ds
	push es
	push ax
	mov ax, 0
	mov ds, ax ; load first page
	mov ah, 0x02
	int 0x16
	and al, 0x04
	jnz SHORT SkipRomInitialization

	les	ax, ds:[19h*4]

	mov WORD PTR ds:[019h*4], offset _TEXT:init
	mov WORD PTR ds:[019h*4+2], cs

	mov	WORD PTR ds:[032h*4], ax
	mov	WORD PTR ds:[032h*4+2], es


SkipRomInitialization:
	pop ax
	pop es
	pop ds
	popf
	retf

assume  cs:_TEXT
extrn _start: near
init proc near
	push  ax
	push  ax
	pushf
	push ax
	push cx
	push dx
	push bx
	push bp
	push si
	push di
	push ds
	push es

	mov bp,sp

	mov bx, cs
	mov ds, bx
	mov bx, 0
	mov es, bx

	mov ax, WORD PTR  es:[0413h]
	sub ax, 16
	mov WORD PTR  es:[0413h], ax
	mov cl, 6
	shl ax,cl
	mov es,ax
	cld

    mov cx, 0x2000            ; 8k WORDs in image
    mov si, 0x0000            ; Current code Address
    mov di, 0x0000            ; New code Address
    rep movsw                 ; Copy MBR
	
	push es
	mov ax, offset _TEXT:jumpram
	push ax
	retf

public jumpram
jumpram label near

	mov ax, 0
	mov ds, ax ; load first page

	les	ax, ds:[32h*4]

	mov	WORD PTR ds:[019h*4], ax
	mov	WORD PTR ds:[019h*4+2], es

	mov WORD PTR ss:[bp+20],ax
	mov WORD PTR ss:[bp+22],es

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

	pop es
	pop ds
	pop di
	pop si
	pop bp
	pop bx
	pop dx
	pop cx
	pop ax
	popf

	retf
init	endp
_TEXT ends
	end
