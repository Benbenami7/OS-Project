CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
LDFLAGS = -lraylib -lm

SRCS = main.c graph.c gui.c
OBJS = $(SRCS:.c=.o)
TARGET = os_project

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
