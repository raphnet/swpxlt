NASM=nasm
DOSBOX=dosbox

all: showvga.com showcga.com

vga256.bin: ../images/vga256.png ../png2vga
	../png2vga $< $@ -p

cga.bin: ../images/flower_cga.png ../png2cga
	../png2cga $< $@ -p

showvga.com: showvga.asm vga256.bin
	$(NASM) $< -fbin -o $@

showcga.com: showcga.asm cga.bin
	$(NASM) $< -fbin -o $@

run_vga: showvga.com
	$(DOSBOX) -noautoexec -conf vga.dosbox.conf -c 'mount c .' -c 'c:' -c '$<'

run_cga: showcga.com
	$(DOSBOX) -noautoexec -conf vga.dosbox.conf -c 'mount c .' -c 'c:' -c '$<'
