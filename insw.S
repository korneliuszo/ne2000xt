.8086

_TEXT  segment word public 'CODE'
assume  cs:_TEXT

public insw_
insw_ proc near

jcxz endfunc
mov bx, cx
add cx, 15
shr cx,1
shr cx,1
shr cx,1
shr cx,1
neg bx
and bx, 0xf
shl bx,1
mov ax, offset _TEXT:read_loop
add ax,bx
jmp ax
read_loop:
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
in	al,dx
stosb
loop read_loop
endfunc:
ret
insw_ endp

_TEXT ends
	end

