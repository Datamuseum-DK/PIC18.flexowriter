
# port sdcc-2.5.4.2005.11.15 is not new enough

SDCC	=	/usr/local/bin/sdcc

#OPTS	=
#OPTS	= --optimize-df --fomit-frame-pointer 
#OPTS	+= --calltree

PIC	=	pic18f4550
PIC_F	=	pic18f4550

PIC_INCL_DIR =	-I/usr/local/share/sdcc/non-free/include/pic16 \
		-I/usr/local/share/sdcc/include/pic16

PROG	=	phk_flexowriter

# --pstack-model=large 
# --optimize-cmp 
# --obanksel=1 \
# --denable-peeps \

all:	*.c
	sdcc -V --verbose \
		--calltree \
		-mpic16 -p${PIC} \
		--optimize-df \
		--use-non-free \
		--fomit-frame-pointer \
		${PROG}.c \
		-Wl-s18f4550.lkr -Wl-m
	cp *.hex /tmp
	tail -15 *.lst
	ls -l *.hex

aoll:	${PROG}.hex
	scp *.hex root@hex:.

${PROG}.hex:	${PROG}.c usb.h usb.c serial.c usb_desc.c
	${SDCC} -Wl-m \
		-I${.CURDIR} ${PIC_INCL_DIR} \
		--use-non-free \
		${OPTS} \
		-mpic16 -p${PIC} ${PROG}.c -llibc18f.lib -llibsdcc.lib
	tail -8 ${PROG}.lst | cut -c40-1000 | head -5

h55:	${PROG}.hex
	scp ${PROG}.hex root@h55:/tmp

clean:
	rm -f _* *.hex *.map *.o *.calltree *.cod *.lst *.asm

flint:	
	flint9 \
		-I${.CURDIR} \
		${PIC_INCL_DIR} \
		-D${PIC_F} \
		-DFLEXELINT \
		${.CURDIR}/flint.lnt \
		${PROG}.c

.include <bsd.obj.mk>

