#pragma once

#include <uv.h>

char *smprintf(const char *fmt, ...);
void update_status_cb(uv_timer_t *handle);
char *mktimes(const char *fmt);
char *gettemperature(const char *base, const char *sensor);
char *execscript(const char *cmd);

