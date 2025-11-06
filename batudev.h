#pragma once

#include <uv.h>

#ifdef CONFIG_BATTERY_INFO

int setup_battery_mon(void);
void battery_poll_cb(uv_poll_t *handle, int status, int events);
char *getbattery(void);

#else /* nop */

static inline int setup_battery_mon(void)
{
	return -1;
}

static inline void battery_poll_cb(uv_poll_t *handle, int status, int events)
{
	return;
}

static inline char *getbattery(void)
{
	return NULL;
}

#endif
