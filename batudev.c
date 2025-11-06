#include <libudev.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <uv.h>

#include "backend.h"

#define POWER_SUPPLY_SYSFS "/sys/class/power_supply/BAT0"

static struct {
	struct udev *ctx;
	struct udev_device *dev;
	struct udev_monitor *mon;
} bat_udev;


static struct {
	long full;
	long now;
	char *status;
	size_t status_len;
} bat_info;


void update_battery_info(struct udev_device *devp)
{
	if (!devp)
		return;

	const char *new_status = udev_device_get_sysattr_value(devp, "status");
	const char *new_full = udev_device_get_sysattr_value(devp, "energy_full");
	const char *new_now = udev_device_get_sysattr_value(devp, "energy_now");

	/* fallback to charge_* if energy_* is not available */
	if (!new_full || !new_now) {
		new_full = udev_device_get_sysattr_value(devp, "charge_full");
		new_now  = udev_device_get_sysattr_value(devp, "charge_now");
	}

	if (new_status && new_full && new_now) {
		snprintf(bat_info.status, bat_info.status_len, "%s", new_status);
		/* don't expect me to handle strtol()'s errors; this comes from sysfs,
		 * so go yell at the Linux mailing list
		 */
		bat_info.full = strtol(new_full, NULL, 10);
		bat_info.now = strtol(new_now, NULL, 10);
	}
}


int setup_battery_mon(void)
{
	int ret;

	bat_udev.ctx = udev_new();
	if (!bat_udev.ctx) {
		perror("udev_new");
		return -1;
	}

	bat_udev.dev = udev_device_new_from_syspath(bat_udev.ctx, POWER_SUPPLY_SYSFS);
	if (!bat_udev.dev) {
		perror("udev_device_new_from_syspath");
		goto fail_dev;
	}

	bat_udev.mon = udev_monitor_new_from_netlink(bat_udev.ctx, "udev");
	if (!bat_udev.mon) {
		perror("udev_monitor_new_from_netlink");
		goto fail_mon;
	}

	udev_monitor_filter_add_match_subsystem_devtype(bat_udev.mon, "power_supply", NULL);
	udev_monitor_filter_update(bat_udev.mon);

	ret = udev_monitor_enable_receiving(bat_udev.mon);
	if (ret < 0) {
		perror("udev_monitor_enable_receiving");
		goto fail_mon;
	}

	ret = udev_monitor_get_fd(bat_udev.mon);
	if (ret == -1) {
		perror("udev_monitor_get_fd");
		goto fail_mon;
	}

	/* According to Documentation/filesystems/sysfs.txt,
 	 * sysfs allocates a buffer of size (PAGE_SIZE) and
 	 * passes it to the show() method. getpagesize() is
 	 * more than enough for this buffer.
 	 */
 	bat_info.status_len = getpagesize();
	bat_info.status = calloc(bat_info.status_len, sizeof(char)); 
	if (!bat_info.status) {
		perror("failed to allocate memory for bat_info.status");
		goto  fail_mon;
	}
	update_battery_info(bat_udev.dev);
	return ret;

fail_mon:
	udev_monitor_unref(bat_udev.mon);
	udev_device_unref(bat_udev.dev);
fail_dev:
	udev_unref(bat_udev.ctx);
	bat_udev.ctx = NULL;
	bat_udev.mon = NULL;
	bat_udev.dev = NULL;

	return -1;
}


void battery_poll_cb(uv_poll_t *handle, int status, int events)
{
	(void)handle;
	struct udev_device *altered_devp;

	/* if this is the case, probably the world is on fire */
	if (status < 0) {
		fprintf(stderr, "poll_cb: %s\n", uv_strerror(status));
		return;
	}

	if (!(events & UV_READABLE)) {
		return;
	}

	/* drain all pending devices */
	while ((altered_devp = udev_monitor_receive_device(bat_udev.mon))) {
		const char *altered_syspath = udev_device_get_syspath(altered_devp);
		const char *bat_syspath = udev_device_get_syspath(bat_udev.dev);

		if (!altered_syspath || !bat_syspath ||
		    strcmp(altered_syspath, bat_syspath) != 0) {
			udev_device_unref(altered_devp);
			continue;
		}

		udev_device_unref(bat_udev.dev);
		bat_udev.dev = altered_devp;
		update_status_cb(NULL);
	}
}


char *getbattery(void)
{
	char status;

	update_battery_info(bat_udev.dev);

	if (!bat_info.status)
		return NULL;

	if (!strcmp(bat_info.status, "Discharging")) {
		status = '-';
	} else if (!strcmp(bat_info.status, "Charging")) {
		status = '+';
	} else {
		status = '\0';
	}

	if (bat_info.full <= 0 || bat_info.now < 0)
		return smprintf("invalid");

	return smprintf("%.0f%%%c", ((float)bat_info.now / bat_info.full) * 100.0f, status);
}
