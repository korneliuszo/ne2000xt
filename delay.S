.8086

_DATA  segment word public 'DATA'
old_int1c label word
	dw 0
old_int1c_1 label word
	dw 0
_DATA ends

_TEXT  segment word public 'CODE'
assume  cs:_TEXT

public _delay_init
_delay_init proc near
	push es
	push ax
	mov ax, 0
	mov es, ax
	cli
	mov WORD PTR ax,es:[01ch*4]
	mov WORD PTR old_int1c, ax
	mov WORD PTR ax,es:[01ch*4+2]
	mov WORD PTR old_int1c_1, ax
	mov WORD PTR es:[01ch*4], offset _TEXT:int1c
	mov WORD PTR es:[01ch*4+2], cs
	sti
	pop ax
	pop es
	ret
_delay_init endp

extrn _delay_c_isr : proc
int1c proc
	push ax
	push bx
	push cx
	push dx
	push ds
	push es
	cld
	mov ax,DGROUP
	mov bx, cs
	add ax, bx
    assume  ds:DGROUP
	mov	ds, ax

	call _delay_c_isr

	pushf
	callf dword ptr ds:old_int1c

	pop es
	pop ds
	pop dx
	pop cx
	pop bx
	pop ax
	iret
int1c endp

_TEXT ends
	end
