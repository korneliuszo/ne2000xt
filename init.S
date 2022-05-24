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
	mov ax, 0
	jmp irq_entry
init	endp

public isr1
isr1 proc near
	push  ax
	push  ax
	pushf
	push ax
	mov ax, 1
	jmp irq_entry
isr1	endp

public _install13h
_install13h proc near
	pushf
	cli
	push ds
	mov ax, 0
	mov ds, ax
	les	ax, ds:[13h*4]
	mov WORD PTR ds:[013h*4], offset _TEXT:isr1
	mov WORD PTR ds:[013h*4+2], cs
	pop ds
	mov	WORD PTR ds:[0x1*4+irq_captured], ax
	mov	WORD PTR ds:[0x1*4+2+irq_captured], es
	popf
	ret
_install13h	endp

move_to_ram proc near

	push ax

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

	mov cx,DGROUP
	mov bx, cs
	add cx, bx
    assume  ds:DGROUP
	mov	ds, cx

	pop si

	shl si,1
	shl si,1
	mov	WORD PTR ds:[si+irq_captured], ax
	mov	WORD PTR ds:[si+2+irq_captured], es

	ret

move_to_ram endp

irq_entry proc near
	push cx
	push dx
	push bx
	push bp
	push si
	push di
	push ds
	push es

	mov bp,sp

	push ax

	mov bx,cs
	cmp bx, 0xa000
	jc skip_ram
	call move_to_ram
public skip_ram

skip_ram label near

   	mov ax,DGROUP
	mov bx, cs
	add ax, bx
    assume  ds:DGROUP
	mov	ds, ax

	pop si
	push si

	shl si,1
	shl si,1

	les	bx, ds:[si+irq_captured]

	mov WORD PTR ss:[bp+20],bx
	mov WORD PTR ss:[bp+22],es

    assume  es:DGROUP
	mov es, ax

	sti
	cld

	pop bx
	push ss
	mov ax, bp
	push ax
	push bx

	call _start

	add sp, 6

	test ax,ax
	jz handled

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
handled label near
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
	add sp, 4
	iret

irq_entry	endp
_TEXT ends

_DATA  segment word public 'DATA'
irq_captured label near
dw 0
dw 0
dw 0
dw 0

_DATA ends

	end
