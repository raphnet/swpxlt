org 100h
bits 16
cpu 8086

[map all showvga.map]

section .text
	jmp main


section .bss

old_mode: resb 1

section .data

imagedata: incbin 'cga.bin'

section .text

main:
	call setvidemode

	; Point ES:DI to VRAM
	mov ax, 0xB800
	mov es, ax

	; Point DS:SI to image
	mov si, imagedata

	cld
	mov di, 0
	mov cx, 320*100/4
	rep movsb

	mov di, 0x2000
	mov cx, 320*100/4
	rep movsb


	; Load image


	call waitPressSpace

	call restorevidmode
	jmp exit


setvidemode:
	push ax

	mov ah, 0fh ; Get Video State
	int 10h
	mov byte [old_mode], al

	mov ah, 00h ; Set video mode
	mov al, 04h ; CGA 4-color
	int 10h

	mov ah, 0bh	; Set palette
	mov bh, 1 ; 4-color palette select
	mov bl, 1 ; black/cyan/magenta/white palette
	int 10h


	pop ax
	ret

restorevidmode:
	push ax
	mov ah, 00
	mov al, [old_mode]
	int 10h
	pop ax
	ret


	; Block until space is pressed
waitPressSpace:
	push ax

.poll:
	call .getKey
	cmp al, ' '
	jne .poll
	pop ax
	ret

.getKey:
	mov ah, 01
	int 16h
	jz .getKey
	; Remove the key from the queue
	mov ah, 0
	int 16h
	ret



exit:
	mov ah,0x4c
	mov al,0x00
	int 21h
	ret ; not reached


