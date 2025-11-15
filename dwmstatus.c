#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <X11/Xlib.h>

#include "batudev.h"
#include "backend.h"
#include "alsamixer.h"

#define TICK 30000

static Display *dpy;


static inline void set_status(const char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}


void update_status_cb(uv_timer_t *handle)
{
	(void)handle;
	char *tm;
	char *t0;
	char *bat;
	char *batfmt;
	char *status;
	char *volume;

	bat = getbattery();
	batfmt = bat ? smprintf("%s | ", bat) : strdup("");
	tm = mktimes(" %a %d %b %Y |  %H:%M ");
	t0 = gettemperature("/sys/devices/virtual/thermal/thermal_zone0", "temp");
	volume = get_volume();

	status = smprintf(" %s %s | %s | %s", batfmt, t0, volume, tm);
	set_status(status);

	free(t0);
	free(bat);
	free(tm);
	free(volume);
	free(status);
	free(batfmt);
}


int main(void)
{
	int err;
	int netlink_fd, alsa_mixer_fd;
	uv_poll_t bat_poll_handler = { 0 };
	uv_poll_t alsa_mixer_poll_handler = { 0 };
	uv_timer_t timer_req = { 0 };
	uv_loop_t *loop;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return EXIT_FAILURE;
	}

	alsa_mixer_fd = setup_volume_monitor("default", "Master");
	if (alsa_mixer_fd < 0)
		return EXIT_FAILURE;

	if ((loop = uv_default_loop()) == NULL) {
		return EXIT_FAILURE;
	}

	if ((err = uv_timer_init(loop, &timer_req)) != 0) {
		fprintf(stderr, "uv_timer_init: %s\n", uv_strerror(err));
		return EXIT_FAILURE;
	}

	if ((err = uv_timer_start(&timer_req, update_status_cb, 0, TICK)) != 0) {
		fprintf(stderr, "uv_timer_start: %s\n", uv_strerror(err));
		return EXIT_FAILURE;
	}

	/* battery info is optional, no need to exit the program */
	netlink_fd = setup_battery_mon();
	if (netlink_fd >= 0) {
		err = uv_poll_init(loop, &bat_poll_handler, netlink_fd);
		if (err != 0) {
			fprintf(stderr, "uv_poll_init: udev_netlink: %s\n", uv_strerror(err));
			return EXIT_FAILURE;
		}
		uv_poll_start(&bat_poll_handler, UV_READABLE, battery_poll_cb);
	}


	if ((err = uv_poll_init(loop, &alsa_mixer_poll_handler, alsa_mixer_fd)) != 0) {
		fprintf(stderr, "uv_poll_init: alsa_mixer: %s\n", uv_strerror(err));
		return EXIT_FAILURE;
	}

	uv_poll_start(&alsa_mixer_poll_handler, UV_READABLE, alsa_mixer_poll_cb);
	uv_run(loop, UV_RUN_DEFAULT);
}
