
SOURCES=Src/LibpurpleAdapterExt.c
OBJECTS=$(SOURCES:.c=.o)
CFLAGS=-g `pkg-config --cflags glib-2.0 purple` -DDEVICE -IIncs -IIncs -IIncs/cjson
LDFLAGS=-Wl,-rpath=i686 -Wl,-rpath=arm -Li686 -Larm `pkg-config --libs purple` -llunaservice -lcjson
all: LibpurpleAdapterExt 

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

LibpurpleAdapterExt: $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -f LibpurpleAdapterExt Src/*.o