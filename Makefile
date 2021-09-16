.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)


all: asroot
asroot.o: asroot.c arg.h

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

asroot: asroot.o
	$(CC) -o $@ asroot.o $(LDFLAGS)

install: asroot
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin/"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man8/"
	cp -- asroot "$(DESTDIR)$(PREFIX)/bin/asroot"
	cp -- asroot.8 "$(DESTDIR)$(MANPREFIX)/man8/asroot.8"

post-install:
	chown -- '0:wheel' "$(DESTDIR)$(PREFIX)/bin/asroot"
	chmod -- 4750 "$(DESTDIR)$(PREFIX)/bin/asroot"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/bin/asroot"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man8/asroot.8"

clean:
	-rm -f -- asroot *.o *.su

.SUFFIXES:
.SUFFIXES: .o .c

.PHONY: all install post-install uninstall clean
