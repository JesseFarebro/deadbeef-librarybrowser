DB_PATH ?= ../deadbeef

CFLAGS  = `pkg-config --cflags gtk+-2.0`
LIBS    = `pkg-config --libs   gtk+-2.0`
LDFLAGS = -shared

CFLAGS += -I $(DB_PATH)
CFLAGS += -Wall -std=c99

all : filebrowser.so

filebrowser.so : filebrowser.c
	$(CC) $(CFLAGS) $(LDFLAGS)  -o filebrowser.so filebrowser.c $(LIBS)

#install :

clean:
	@rm -f filebrowser.o

.PHONY: all install clean
