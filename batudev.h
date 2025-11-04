#pragma once

#include <uv.h>

int setup_battery_mon(void);
void battery_poll_cb(uv_poll_t *handle, int status, int events);
char *getbattery(void);
