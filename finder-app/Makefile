
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -g
TARGET ?= writer

all: $(TARGET)

$(TARGET): writer.c
	$(CC) $(CFLAGS) writer.c -o $(TARGET)

clean :
	rm -f $(TARGET)