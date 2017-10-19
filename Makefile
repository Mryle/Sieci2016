CC = gcc
CXX = g++
CFLAGS = -Wall -ggdb -std=c11
CXXFLAGS = -Wall --std=c++11 -lstdc++
TARGETS = siktacka-client siktacka-server
LIBS = events.h numbers.h util.h err.h

all: $(TARGETS)

.PHONY: all, clean

client.o: client.c client.h $(LIBS)

util.o: util.cc util.h err.h

err.o: err.h

server.o: server.c server.h $(LIBS)

siktacka-client: client.o util.o err.o
	g++ client.o util.o err.o -o siktacka-client

siktacka-server: server.o util.o err.o
	g++ server.o util.o err.o -o siktacka-server

clean:
	rm -f *~ DEADJOE *.o $(TARGETS) test-*
