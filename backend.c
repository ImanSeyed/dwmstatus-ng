#include <unistd.h>
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

#include "batudev.h"
#include "alsamixer.h"

Display *dpy;

static inline void setstatus(const char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *smprintf(const char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

static char *mktimes(const char *fmt)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf) - 1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return strdup(buf);
}


static char *readfile(const char *base, const char *file)
{
	char *path, line[513];
	FILE *fd;

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line) - 1, fd) == NULL) {
		fclose(fd);
		return NULL;
	}
	fclose(fd);

	return strdup(line);
}


char *gettemperature(const char *base, const char *sensor)
{
	char *co;
	int temp;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");

	temp = strtol(co, NULL, 10);
	free(co);

	return smprintf("%02.0f°C", temp / 1000.0f);
}

__attribute__((unused)) static char *execscript(const char *cmd)
{
	FILE *fp;
	char retval[1025], *rv;
	size_t len;

	fp = popen(cmd, "r");
	if (fp == NULL)
		return smprintf("");

	rv = fgets(retval, sizeof(retval), fp);
	pclose(fp);
	if (rv == NULL)
		return smprintf("");

	len = strlen(retval);
	if (len && retval[len - 1] == '\n')
		retval[len - 1] = '\0';

	return strdup(retval);
}


void update_status_cb(uv_timer_t *handle)
{
	(void)handle;
	char *status;
	char *volume;
	char *tm;
	char *bat;
	char *t0;

	tm = mktimes(" %a %d %b %Y |  %H:%M ");
	t0 = gettemperature("/sys/devices/virtual/thermal/thermal_zone0", "temp");
	volume = get_volume();


	/* TODO: right now, when CONFIG_BATTERY_INFO is not defined,
	 * getbattery() returns NULL, so free(bat) is fine but
	 * it'd be a good idea to have a safer wrapper over free()
	 * to set the pointer to NULL when free() is done.
	 */
	bat = getbattery();
	if (bat)
		status = smprintf(" %s|  %s | %s | %s",
			          bat,t0, volume, tm);
	else
		status = smprintf("  %s | %s | %s", t0, volume, tm);
	setstatus(status);

	free(t0);
	free(bat);
	free(tm);
	free(volume);
	free(status);
}
