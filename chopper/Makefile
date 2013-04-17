CC = gcc
# Debug setup
CFLAGS = -Wall -static -g -lavcodec -lavformat -lssl -c
# Performance setup
# CFLAGS = -march=native -O2 -std=gnu99 -falign-functions=64 -fomit-frame-pointer -Wall
LDFLAGS = -g -lavcodec -lavformat -lssl
SOURCES = ../server/logging.c ../server/common.c file.c video.c video_analysis.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = chopper

all: $(SOURCES) $(EXECUTABLE)
			
$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	
.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE)	

get_sign: ../server/common.o ../server/logging.o
	$(CC) $(CFLAGS) get_sign.c -o get_sign.o
	$(CC) $(LDFLAGS) ../server/logging.o ../server/common.o get_sign.o -o get_sign
