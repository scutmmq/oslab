obj-m += oslab_ringbuf.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)
CC   ?= gcc
CFLAGS_USER := -Wall -Wextra -O2

.PHONY: all module user clean load unload test

all: module user

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: test_ringbuf pc_demo

test_ringbuf: test_ringbuf.c
	$(CC) $(CFLAGS_USER) -o $@ $<

pc_demo: pc_demo.c
	$(CC) $(CFLAGS_USER) -o $@ $<

load: module
	sudo insmod oslab_ringbuf.ko
	sudo chmod 666 /dev/oslab_ringbuf

unload:
	sudo rmmod oslab_ringbuf

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f test_ringbuf pc_demo

test: all
	-sudo rmmod oslab_ringbuf 2>/dev/null || true
	sudo insmod oslab_ringbuf.ko buf_size=16
	sudo chmod 666 /dev/oslab_ringbuf
	@echo "=== test_ringbuf ===" ; ./test_ringbuf
	@echo "=== pc_demo ===" ; ./pc_demo
	@echo "=== /proc/oslab_ringbuf ===" ; cat /proc/oslab_ringbuf
	sudo rmmod oslab_ringbuf
	@echo "=== ALL TESTS DONE ==="
