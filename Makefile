CFLAGS  += `pkg-config --cflags gtk+-2.0`
LIBS    += `pkg-config --libs   gtk+-2.0`
LDFLAGS += -shared

CFLAGS += -Wall -std=c99

all : filebrowser.so

filebrowser.so : filebrowser.c support.c utils.c
	$(CC) $(CFLAGS) $(LDFLAGS)  -o filebrowser.so filebrowser.c support.c utils.c $(LIBS)

#install :

clean:
	@rm -f filebrowser.o support.o utils.o filebrowser.so

.PHONY: all install clean
