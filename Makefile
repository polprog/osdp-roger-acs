CC = gcc
CFLAGS = -Wall -Ilibs -I.
LDFLAGS = -s -Wl,-warn-common -lpthread -lm -lc -lsqlite3 -lgpiod

#CFLAGS += -DDEBUG -DGPIO_DEBUG -DEPSO_DEBUG -DOSDP_VERBOSE_LEVEL=3

all:
	@echo "make what?"
	@echo "use \`make kd-idesco\` to build app for OSDP Idesco readers (RS485 on /dev/ttyS2)"
	@echo "use \`make kd-roger\` to build app for Roger EPSO protocol readers (eg. PRT12EM)"
	@echo "use \`make osdp-reader-test\` to build minimal OSDP lib reader test tool"
	@echo "use \`make osdp-set-address\` to build OSDP reader set address tool"

%: build/%.elf
	@echo "$< build successful"

%-lib: build/%.a
	@echo "$< build successful"

# KD apps
build/kd-idesco.elf: build/door_controller/main.o build/door_controller/door_controller.o build/door_controller/user_db.o build/door_controller/eventSend.o build/door_controller/remoteControl.o \
  build/libs/md5.o build/gpios/gpio-orangepizero.o \
  build/readers/reader-idesco.o build/osdp.a

build/kd-roger.elf: build/door_controller/main.o build/door_controller/door_controller.o build/door_controller/user_db.o build/door_controller/eventSend.o build/door_controller/remoteControl.o \
  build/libs/md5.o build/gpios/gpio-orangepizero.o \
  build/readers/reader-roger.o build/epso.a

# tools
build/osdp-reader-test.elf: build/tools/osdp-reader-test.o build/osdp.a

build/osdp-set-address.elf: build/tools/osdp-set-address.o build/osdp.a

# Roger EPSO lib
build/epso.a: $(addprefix build/readers/epso/, epso.o)

# OSDPv2 lib
build/osdp.a: $(addprefix build/readers/osdp/, osdp.o crctable.o)


# include dependencies
include $(shell find build/ -name '*.d' 2>/dev/null)

# general compile, linking, etc rules
build/%.o: %.c build/%.d
	$(CC) $(CFLAGS) -o $@ -c $<

build/%.a:
	ar r $@ $^
	ranlib $@

build/%.elf:
	@echo Link... $@
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# write dependencies rule
.PRECIOUS: build/%.d
build/%.d: %.c
	mkdir -p $(@D)
	@ echo "$@ $(@D)`$(CC) $(CFLAGS) -MM $<`" > $@

.PHONY: clean
clean:
	rm -fr build
