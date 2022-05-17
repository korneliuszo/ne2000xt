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


public inswf_
zero:
ret
inswf_ proc near

jcxz zero
shr cx,1
jc endfuncf
mov bx, cx
add cx, 15
shr cx,1
shr cx,1
shr cx,1
shr cx,1
neg bx
and bx, 0xf
mov ax, offset _TEXT:read_loopf
add ax,bx
shl bx,1
shl bx,1
add ax,bx
jmp ax
read_loopf:
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
in	al,dx
mov ah,al
in	al,dx
stosw
loop read_loopf
endfuncf:
ret
inswf_ endp

_TEXT ends
	end

