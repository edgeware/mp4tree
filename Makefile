CC ?= gcc
CFLAGS = -g -Wall
TARGET = mp4tree

SRCS := mp4tree.c
SRCS += atom-desc.c
SRCS += common.c
SRCS += nal.c
SRCS += sei.c

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

all: $(TARGET)

test: CFLAGS += -S -fsyntax-only -Werror
test: $(SRCS)
	$(CC) $(CFLAGS) $^

clean:
	$(RM) $(TARGET)
	$(RM) -r $(TARGET).dSYM
