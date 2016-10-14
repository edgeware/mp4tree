CC=gcc
CFLAGS=-g -Wall
TARGET=mp4tree

SRCS := mp4tree.c

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

all: $(TARGET)

clean:
	$(RM) $(TARGET)
