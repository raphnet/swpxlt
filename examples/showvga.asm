org 100h
bits 16
cpu 8086

[map all showvga.map]

section .text
	jmp main


section .bss

old_mode: resb 1

section .data

imagedata: incbin 'vga256.bin'

section .text

main:
	call setvidemode

	; Point ES:DI to VRAM
	mov ax, 0xA000
	mov es, ax
	xor di,di

	; Point DS:SI to image
	mov si, imagedata

	; Image size
	mov cx, 320*200

	; Load image
	cld
	rep movsb

	; Load palette (SI points to the count word following the image data)
	lodsw
	mov cx, ax ; This will be the count for the copy loop below
	; SI now points to the first palette entry

	; Set PEL address
	mov dx, 0x3c8
	xor al, al
	out dx, al

	; Data register
	mov dx, 0x3c9

.next_color:
	lodsb
	out dx, al
	lodsb
	out dx, al
	lodsb
	out dx, al
	loop .next_color


	call waitPressSpace

	call restorevidmode
	jmp exit


setvidemode:
	push ax

	mov ah, 0fh ; Get Video State
	int 10h
	mov byte [old_mode], al

	mov ah, 00h ; Set video mode
	mov al, 13h ; VGA 256 color 320x200
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


