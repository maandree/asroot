PREFIX    = /usr
MANPREFIX = $(PREFIX)/share/man

CC = cc

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_GNU_SOURCE
CFLAGS   = -std=c99 -Wall -O2
LDFLAGS  = -s -lcrypt


# To use libpassphrase, add -DWITH_LIBPASSPHRASE to CPPFLAGS and -lpassphrase to LDFLAGS

# To customise the sleep time between authentication attempts, add -DRETRY_SLEEP=###
# to CPPFLAGS, where ### is the number of seconds (integers only) to sleep, the
# default ### is 1 (= 1 second)

# To customise the password prompt, add -DPROMPT=### to CPPFLAGS, where ### is the
# format string and arguments for fprintf(3), do not forget quotes to escape spaces,
# the default ### is (with the surrounding quotes)
# '"%s (%s@%s) password: ", argv0, pwd->pw_name, hostname'
# argv0 is the name of the process,
# pwd->pw_name is the user name, and
# hostname is the machine name
