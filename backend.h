#pragma once

#include <uv.h>

char *smprintf(const char *fmt, ...);
void update_status_cb(uv_timer_t *handle);

