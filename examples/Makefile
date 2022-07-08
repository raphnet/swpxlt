NASM=nasm
DOSBOX=dosbox

all: showvga.com

vga256.bin: ../images/vga256.png ../png2vga
	../png2vga $< $@ -p

showvga.com: showvga.asm vga256.bin
	$(NASM) $< -fbin -o $@

run_vga: showvga.com
	$(DOSBOX) -noautoexec -conf vga.dosbox.conf -c 'mount c .' -c 'c:' -c '$<'
