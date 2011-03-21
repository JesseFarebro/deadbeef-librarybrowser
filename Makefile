DATE     = `date +"%Y-%m-%d"`

CFLAGS  += `pkg-config --cflags gtk+-2.0`
LIBS    += `pkg-config --libs   gtk+-2.0`
LDFLAGS += -shared

CFLAGS  += -Wall -std=c99

INSTDIR  = $(HOME)/.local/deadbeef/lib/
RELEASE  = deadbeef-fb_$(DATE).tar.gz
SOURCES  = filebrowser.c support.c utils.c

all : filebrowser.so

filebrowser.so : $(SOURCES)
	$(CC) $(CFLAGS) $(LDFLAGS)  -o filebrowser.so $(SOURCES) $(LIBS)

#install :

release :
	@tar -czvf $(RELEASE) *.c *.h filebrowser.so Makefile COPYING README
	@mv $(RELEASE) ../$(RELEASE)

clean:
	@rm -f filebrowser.o support.o utils.o filebrowser.so

.PHONY: all install release clean
