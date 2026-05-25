OBJECTS = file_handling.o socket_handling.o aesdsocket.o

CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -g
TARGET ?= aesdsocket
LDFLAGS ?=

all: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $(TARGET)

aesdsocket.o: aesdsocket.c file_handling.o socket_handling.o
	$(CC) $(CFLAGS) -c aesdsocket.c

file_handling.o: file_handling.c file_handling.h
	$(CC) $(CFLAGS) -c file_handling.c

socket_handling.o: socket_handling.c socket_handling.h server_cfg.h
	$(CC) $(CFLAGS) -c socket_handling.c

clean :
	rm -f $(OBJECTS) $(TARGET)