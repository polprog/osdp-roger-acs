ALLAPP = kontrola_dostepu

CC=gcc
CFLAGS = -Wall -Os -fomit-frame-pointer -DLINUX -D_GNU_SOURCE
LDFLAGS = -s -Wl,-warn-common -lc -lsqlite3 idesco.a

.c.o:
	@echo Compile... $<
	$(CC) $(CFLAGS) -I. -c -o $@ $<

#all: $(ALLAPP)
#
#	@rm -f *.o *.gdb

all:
	@echo "make what?"
roger: CFLAGS+= -DROGER
idesco: CFLAGS+= -DIDESCO -DTTY="\"/dev/ttyS2\"" -Iidesco/
idesco roger: idesco.a kontrola_dostepu

.PHONY: roger idesco kontrola_dostepu
	
	
idesco.a:
	cd idesco; make idesco.a
	mv idesco/idesco.a .
kontrola_dostepu.o: idesco_logic.c roger_logic.c
kontrola_dostepu: kontrola_dostepu.o md5.o 
	@echo Link... $@
	$(CC) $(CFLAGS) -pthread -lm -o $@ $^ -lpthread $(LDFLAGS) 

clean:
	@rm -fr *.o *.gdb $(ALLAPP)
	cd idesco; make clean
