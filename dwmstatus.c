#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

const char *tzone = "America/Toronto";
static Display *dpy;

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

static inline void settz(const char *tzname)
{
	setenv("TZ", tzname, 1);
	tzset();
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

static inline void setstatus(const char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
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

static char *getbattery(const char *base)
{
	char *co, status;
	int descap, remcap;

	descap = -1;
	remcap = -1;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "energy_full");
	if (co == NULL)
		return smprintf("");
	descap = strtol(co, NULL, 10);
	free(co);

	co = readfile(base, "energy_now");
	if (co == NULL)
		return smprintf("");
	remcap = strtol(co, NULL, 10);
	free(co);

	co = readfile(base, "status");
	if (co == NULL)
		return smprintf("no status");

	if (!strncmp(co, "Discharging", 11)) {
		status = '-';
	} else if (!strncmp(co, "Charging", 8)) {
		status = '+';
	} else {
		status = '\0'; /* Full */
	}
	free(co);

	if (descap == 0 || remcap < 0 || descap < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%c", ((float)remcap / (float)descap) * 100.0f,
			status);
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

static char *execscript(const char *cmd)
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

int main(void)
{
	char *status;
	char *volume;
	char *tm;
	char *bat;
	char *t0;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	settz(tzone);

	for (;; sleep(30)) {
		// TODO: charging/discharging events can be captured via netlink
		bat = getbattery("/sys/class/power_supply/BAT0");

		tm = mktimes("Time: %H:%M | Date: %a %d %b %Y");
		t0 = gettemperature("/sys/devices/virtual/thermal/thermal_zone0", "temp");

		// FIXME: this should updat instantly
		volume = execscript("amixer sget Master | awk -F'[][]' '/Left:/ { print $2 }'");

		status = smprintf("Temp: %s | Battery: %s | Volume: %s | %s",
				   t0, bat, volume, tm);
		setstatus(status);

		free(t0);
		free(bat);
		free(tm);
		free(volume);
		free(status);
	}
}
