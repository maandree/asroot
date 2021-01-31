PREFIX    = /usr
MANPREFIX = $(PREFIX)/share/man

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_GNU_SOURCE
CFLAGS   = -std=c99 -Wall -O2 $(CPPFLAGS)
LDFLAGS  = -s -lcrypt

# To use libpassphrase, add -DWITH_LIBPASSPHRASE to CPPFLAGS and -lpassphrase to LDFLAGS
