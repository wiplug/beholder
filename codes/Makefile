
# Makefile for POSIX-based OSes

TARGET	= libga module server

.PHONY: $(TARGET)

all: $(TARGET)

libga:
	make -C libga

module: libga
	make -C module

server: libga
	make -C server/rpi

install:
	mkdir -p ../bin
	make -C libga install
	make -C module install
	make -C server/rpi install
	cp -fr config ../bin/

clean:
	make -C libga clean
	make -C module clean
	make -C server/rpi clean

