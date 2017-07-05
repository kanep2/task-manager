CFLAGS = -Wall -g 
CC = gcc


all : taskmanager

% : %.c
	$(CC) $(CFLAGS) -o $@ $< `pkg-config --cflags --libs gtk+-3.0`
