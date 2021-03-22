/* See LICENSE file for copyright and license details. */
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <shadow.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef WITH_LIBPASSPHRASE
# include <passphrase.h>
#endif

#include "arg.h"


#define EXIT_ERROR  125
#define EXIT_EXEC   126
#define EXIT_NOENT  127

#ifndef RETRY_SLEEP
# define RETRY_SLEEP 1
#endif

#ifndef PROMPT
# define PROMPT "%s (%s@%s) password: ", argv0, pwd->pw_name, hostname
#endif


char *argv0;

static const char *env_whitelist[] = {
	"DISPLAY=",
	"WAYLAND_DISPLAY=",
	"PATH=",
	"TERM=",
	"COLORTERM=",
	"XAUTHORITY=",
	"LANG=",
	"LANGUAGE=",
	"LOCALE=",
	"LC_CTYPE=",
	"LC_NUMERIC=",
	"LC_TIME=",
	"LC_COLLATE=",
	"LC_MONETARY=",
	"LC_MESSAGES=",
	"LC_PAPER=",
	"LC_NAME=",
	"LC_ADDRESS=",
	"LC_TELEPHONE=",
	"LC_MEASUREMENT=",
	"LC_IDENTIFICATION=",
	"LC_ALL=",
	"LOCPATH=",
	"NLSPATH=",
	"TZ=",
	"TZDIR=",
	"SDL_VIDEO_FULLSCREEN_DISPLAY=",
	"EDITOR=",
	"VISUAL=",
	"BROWSER=",
	"DESKTOP_SESSION=",
	"LS_COLORS=",
	"GTK_THEME=",
	"QT_STYLE_OVERRIDE=",
	"PWD=",
	"OLDPWD=",
	"JAVA_HOME=",
	"_JAVA_AWT_WM_NONREPARENTING=",
	"_JAVA_OPTIONS=",
	"MAIN_ALSA_MIXER=",
	"MAIN_ALSA_CARD=",
	"XDG_SEAT=",
	"XDG_SESSION_TYPE=",
	"XDG_SESSION_CLASS=",
	"XDG_VTNR=",
	"XDG_SESSION_ID=",
	"XDG_DATA_DIRS=",
	"XDG_CONFIG_DIRS=",
	"MANPATH=",
	"INFODIR=",
	"PAGER=",
	"ftp_proxy=",
	"http_proxy=",
	NULL
};


static void
usage(void)
{
	fprintf(stderr, "usage: %s [-e] command [argument] ...\n", argv0);
	exit(EXIT_ERROR);
}


static char **
set_environ(void)
{
	char **new_environ;
	size_t i, j, n, len;
	struct passwd *pw;

	new_environ = calloc(sizeof(env_whitelist) / sizeof(*env_whitelist) + 5, sizeof(*env_whitelist));
	if (!new_environ) {
		fprintf(stderr, "%s: calloc %zu %zu: %s\n",
			argv0, sizeof(env_whitelist) / sizeof(*env_whitelist) + 5, sizeof(*env_whitelist), strerror(errno));
		exit(EXIT_ERROR);
	}
	for (i = 0, n = 0; env_whitelist[i]; i++) {
		len = strlen(env_whitelist[i]);
		for (j = 0; environ[j]; j++) {
			if (!strncmp(environ[j], env_whitelist[i], len)) {
				new_environ[n++] = environ[j];
				break;
			}
		}
	}

	errno = 0;
	pw = getpwuid(0);
	if (!pw) {
		if (errno)
			fprintf(stderr, "%s: getpwuid 0: %s\n", argv0, strerror(errno));
		else
			fprintf(stderr, "%s: cannot find root user\n", argv0);
		exit(EXIT_ERROR);
	}

	if (pw->pw_dir && *pw->pw_dir) {
		len = strlen(pw->pw_dir);
		len += sizeof("HOME=");
		new_environ[n] = malloc(len);
		if (!new_environ[n])
			fprintf(stderr, "%s: malloc %zu: %s\n", argv0, len, strerror(errno));
		stpcpy(stpcpy(new_environ[n++], "HOME="), pw->pw_dir);
	}
	if (pw->pw_name && *pw->pw_name) {
		len = strlen(pw->pw_name);
		len += sizeof("LOGNAME=");
		new_environ[n] = malloc(len);
		if (!new_environ[n])
			fprintf(stderr, "%s: malloc %zu: %s\n", argv0, len, strerror(errno));
		stpcpy(stpcpy(new_environ[n++], "LOGNAME="), pw->pw_name);

		len -= sizeof("LOGNAME=");
		len += sizeof("USER=");
		new_environ[n] = malloc(len);
		if (!new_environ[n])
			fprintf(stderr, "%s: malloc %zu: %s\n", argv0, len, strerror(errno));
		stpcpy(stpcpy(new_environ[n++], "USER="), pw->pw_name);

		len -= sizeof("USER=");
		len += sizeof("MAIL=/var/spool/mail/");
		new_environ[n] = malloc(len);
		if (!new_environ[n])
			fprintf(stderr, "%s: malloc %zu: %s\n", argv0, len, strerror(errno));
		stpcpy(stpcpy(new_environ[n++], "MAIL=/var/spool/mail/"), pw->pw_name);
	}
	if (pw->pw_shell && *pw->pw_shell) {
		len = strlen(pw->pw_shell);
		len += sizeof("SHELL=");
		new_environ[n] = malloc(len);
		if (!new_environ[n])
			fprintf(stderr, "%s: malloc %zu: %s\n", argv0, len, strerror(errno));
		stpcpy(stpcpy(new_environ[n++], "SHELL="), pw->pw_shell);
	}
	new_environ[n] = NULL;

	return new_environ;
}


#ifndef WITH_LIBPASSPHRASE
static char *
read_passphrase(int fd)
{
	char *passphrase = NULL, *new;
	size_t size = 0, len = 0;
	ssize_t r;

	for (;;) {
		if (len == size) {
			new = realloc(passphrase, size += 32);
			if (!new) {
				fprintf(stderr, "\n%s: realloc %zu: %s\n", argv0, size, strerror(errno));
				if (passphrase) {
					memset(passphrase, 0, len);
					free(passphrase);
				}
				return NULL;
			}
			passphrase = new;
		}
		r = read(fd, &passphrase[len], 1);
		if (r < 0) {
			fprintf(stderr, "\n%s: read /dev/tty 1: %s\n", argv0, strerror(errno));
			memset(passphrase, 0, len);
			free(passphrase);
			return NULL;
		} else if (!r || passphrase[len] == '\n') {
			break;
		} else {
			len += 1;
		}
	}

	fprintf(stderr, "\n");
	passphrase[len] = 0;
	return passphrase;
}
#endif


static void
check_password(void)
{
	struct passwd *pwd;
	struct spwd *shdw;
	const char *expected, *got;
	char *hostname = NULL;
	char *passphrase;
	size_t size = 8;
#ifndef WITH_LIBPASSPHRASE
	struct termios stty_original;
#endif
	struct termios stty_enter;
#if RETRY_SLEEP > 0
	struct termios stty_sleep;
#endif
	int fd;

	errno = 0;
	for (;;) {
		hostname = realloc(hostname, size *= 2);
		if (!hostname) {
			fprintf(stderr, "%s: realloc %zu: %s\n", argv0, size, strerror(errno));
		}
		*hostname = 0;
		if (!gethostname(hostname, size)) {
			break;
		} else if (errno != ENAMETOOLONG) {
			fprintf(stderr, "%s: gethostname %zu: %s\n", argv0, size, strerror(errno));
			exit(EXIT_ERROR);
		}
	}

	errno = 0;
	pwd = getpwuid(getuid());
	if (!pwd || !pwd->pw_name || !*pwd->pw_name) {
		if (errno)
			fprintf(stderr, "%s: getpwuid: %s\n", argv0, strerror(errno));
		else
			fprintf(stderr, "%s: your user does not exist\n", argv0);
		exit(EXIT_ERROR);
	}

	shdw = getspnam(pwd->pw_name);
	if (!shdw || !shdw->sp_pwdp) {
		if (errno)
			fprintf(stderr, "%s: getspnam: %s\n", argv0, strerror(errno));
		else
			fprintf(stderr, "%s: your user does not have a shadow entry\n", argv0);
		exit(EXIT_ERROR);
	}

	expected = shdw->sp_pwdp;

	if (!*expected) {
		fprintf(stderr, "%s: your user does not have a password\n", argv0);
		exit(EXIT_ERROR);
	} else if (expected[expected[0] == '!'] == '*' || expected[expected[0] == '!'] == 'x') {
		fprintf(stderr, "%s: login to your account is disabled\n", argv0);
		exit(EXIT_ERROR);
	} else if (expected[0] == '!') {
		fprintf(stderr, "%s: your account is currently locked\n", argv0);
		exit(EXIT_ERROR);
	}

	fd = open("/dev/tty", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: open /dev/tty O_RDWR: %s\n", argv0, strerror(errno));
		exit(EXIT_ERROR);
	}

#ifdef WITH_LIBPASSPHRASE
	passphrase_disable_echo1(fd);

	memset(&stty_enter, 0, sizeof(stty_enter));
	if (tcgetattr(fd, &stty_enter)) {
		fprintf(stderr, "%s: tcgetattr /dev/tty: %s\n", argv0, strerror(errno));
		passphrase_reenable_echo1(fd);
		close(fd);
		exit(EXIT_ERROR);
	}
#else
	memset(&stty_original, 0, sizeof(stty_original));
	if (tcgetattr(fd, &stty_original)) {
		fprintf(stderr, "%s: tcgetattr /dev/tty: %s\n", argv0, strerror(errno));
		close(fd);
		exit(EXIT_ERROR);
	}
	memcpy(&stty_enter, &stty_original, sizeof(stty_original));
	stty_enter.c_lflag &= (tcflag_t)~ECHO;
	tcsetattr(fd, TCSAFLUSH, &stty_enter);
#endif

#if RETRY_SLEEP > 0
	memcpy(&stty_sleep, &stty_enter, sizeof(stty_enter));
	stty_sleep.c_lflag = 0;
#endif

again:
	fprintf(stderr, PROMPT);
	fflush(stderr);

#ifdef WITH_LIBPASSPHRASE
	passphrase = passphrase_read2(fd, PASSPHRASE_READ_EXISTING);
	if (!passphrase) {
		fprintf(stderr, "%s: passphrase_read2 /dev/tty PASSPHRASE_READ_EXISTING: %s\n", argv0, strerror(errno));
		passphrase_reenable_echo1(fd);
		close(fd);
		exit(EXIT_ERROR);
	}
#else
	passphrase = read_passphrase(fd);
	if (!passphrase) {
		tcsetattr(fd, TCSAFLUSH, &stty_original);
		close(fd);
		exit(EXIT_ERROR);
	}
#endif

	got = crypt(passphrase, expected);
#ifdef WITH_LIBPASSPHRASE
	passphrase_wipe1(passphrase);
#else
	memset(passphrase, 0, strlen(passphrase));
#endif
	free(passphrase);

	if (strcmp(got, expected)) {
		fprintf(stderr, "%s: incorrect password, please try again\n", argv0);
#if RETRY_SLEEP > 0
		tcsetattr(fd, TCSAFLUSH, &stty_sleep);
		sleep(RETRY_SLEEP);
		tcsetattr(fd, TCSAFLUSH, &stty_enter);
#endif
		goto again;
	}

#ifdef WITH_LIBPASSPHRASE
	passphrase_reenable_echo1(fd);
#else
	tcsetattr(fd, TCSAFLUSH, &stty_original);
#endif
	close(fd);
}


int
main(int argc, char *argv[])
{
	int keep_env = 0;
	char **new_environ = NULL;

	ARGBEGIN {
	case 'e':
		keep_env = 1;
		break;
	default:
		usage();
	} ARGEND;

	if (!argc)
		usage();

	check_password();

	if (!keep_env)
		new_environ = set_environ();

	if (setgid(0)) {
		fprintf(stderr, "%s: setgid 0: %s\n", argv0, strerror(errno));
		exit(EXIT_ERROR);
	}
	if (setuid(0)) {
		fprintf(stderr, "%s: setuid 0: %s\n", argv0, strerror(errno));
		exit(EXIT_ERROR);
	}

	execvpe(argv[0], argv, new_environ ? new_environ : environ);
	fprintf(stderr, "%s: execvpe %s: %s\n", argv0, argv[0], strerror(errno));
	return errno == ENOENT ? EXIT_NOENT : EXIT_EXEC;
}
