#pragma once

#include <stdbool.h>
#include <uv.h>

int setup_volume_monitor(const char *card, const char *selem_name);
void alsa_mixer_poll_cb(uv_poll_t *handle, int status, int events);
char *get_volume(void);
