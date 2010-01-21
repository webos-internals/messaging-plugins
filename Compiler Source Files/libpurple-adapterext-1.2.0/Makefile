SOURCES=Src/LibpurpleAdapterExt.c
OBJECTS=$(SOURCES:.c=.o)
CFLAGS=-g `pkg-config --cflags glib-2.0 purple` -DDEVICE -IIncs -IIncs -IIncs/cjson
LDFLAGS=-Wl,-rpath=i686 -Li686 -Wl,-rpath=arm -Liarm `pkg-config --libs purple` -llunaservice -lcjson -Wl,-rpath=/var/usr/palm/applications/org.webosinternals.messagingsysfiles/usr/lib -Wl,-rpath=/media/cryptofs/apps/usr/palm/applications/org.webosinternals.messagingsysfiles/usr/lib
all: LibpurpleAdapterExt 

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

LibpurpleAdapterExt: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -f LibpurpleAdapterExt Src/*.o