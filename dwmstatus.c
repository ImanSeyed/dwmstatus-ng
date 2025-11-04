#include <stdlib.h>
#include <X11/Xlib.h>

#include "batudev.h"
#include "backend.h"

#define TICK 30000

extern Display *dpy;

int main(void)
{
	int err;
	int netlink_fd;
	uv_poll_t uv_poll_handler = { 0 };
	uv_timer_t timer_req = { 0 };
	uv_loop_t *loop;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return EXIT_FAILURE;
	}

	netlink_fd = setup_battery_mon();
	if (netlink_fd < 0)
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

	if ((err = uv_poll_init(loop, &uv_poll_handler, netlink_fd)) != 0) {
		fprintf(stderr, "uv_poll_init: %s\n", uv_strerror(err));
		return EXIT_FAILURE;
	}

	uv_poll_start(&uv_poll_handler, UV_READABLE, battery_poll_cb);

	uv_run(loop, UV_RUN_DEFAULT);
}
