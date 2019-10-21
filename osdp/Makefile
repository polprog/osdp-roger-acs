CC=gcc
CFLAGS=-g
#enable RS232 instead of 485
#CFLAGS+=-D_USE_RS232
CFLAGS+=-DTTY=\"/dev/ttyS2\" 

CFLAGS_OSDP=-lgpiod

SOURCES_LIB=util.c osdp.c crctable.c
OBJS_LIB=$(SOURCES_LIB:%.c=%.o)

SOURCES_OSDP=main.c gpio.c
OBJS_OSDP=$(SOURCES_OSDP:%.c=%.o)

SOURCES_ADRES=adres.c
OBJS_ADRES=$(SOURCES_ADRES:%.c=%.o)



idesco.a: $(OBJS_LIB)
	ar ru $@ $^
	ranlib $@

osdp: $(OBJS_LIB) $(OBJS_OSDP)
	$(CC) ${CFLAGS} $(CFLAGS_OSDP) $(OBJS_LIB) $(OBJS_OSDP) -o osdp.elf

adres: $(OBJS_LIB) $(OBJS_ADRES)
	$(CC) ${CFLAGS} $(OBJS_LIB) $(OBJS_ADRES) -o adres.elf

%.o: %.c
	@#printf "  CC      $(*).c\n"
	$(CC) $(CFLAGS) -o $(*).o -c $(*).c

clean:
	rm -rf *.o *.a osdp.elf adres.elf

