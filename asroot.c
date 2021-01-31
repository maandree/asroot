#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arg.h"


char *argv0;

extern char **environ;

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
	exit(125);
}


int
main(int argc, char *argv[])
{
	int keep_env = 0;
	char **new_environ = NULL;
	size_t i, j, n, len;
	struct passwd *pw;

	ARGBEGIN {
	case 'e':
		keep_env = 1;
		break;
	default:
		usage();
	} ARGEND;

	if (!argc)
		usage();

	/* TODO check password */

	if (setgid(0)) {
		fprintf(stderr, "%s: setgid 0: %s\n", argv0, strerror(errno));
		return 125;
	}
	if (setuid(0)) {
		fprintf(stderr, "%s: setuid 0: %s\n", argv0, strerror(errno));
		return 125;
	}

	if (!keep_env) {
		new_environ = calloc(sizeof(env_whitelist) / sizeof(*env_whitelist) + 5, sizeof(*env_whitelist));
		if (!new_environ) {
			fprintf(stderr, "%s: calloc %zu %zu: %s\n",
			        argv0, sizeof(env_whitelist) / sizeof(*env_whitelist) + 5, sizeof(*env_whitelist), strerror(errno));
			return errno == ENOENT ? 127 : 126;
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
			exit(125);
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
			len = strlen(pw->pw_dir);
			len += sizeof("SHELL=");
			new_environ[n] = malloc(len);
			if (!new_environ[n])
				fprintf(stderr, "%s: malloc %zu: %s\n", argv0, len, strerror(errno));
			stpcpy(stpcpy(new_environ[n++], "SHELL="), pw->pw_dir);
		}
		new_environ[n] = NULL;
	}

	execvpe(argv[0], argv, new_environ ? new_environ : environ);
	fprintf(stderr, "%s: execvpe %s: %s\n", argv0, argv[0], strerror(errno));
	return errno == ENOENT ? 127 : 126;
}
