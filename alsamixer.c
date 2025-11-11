#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <alsa/pcm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <uv.h>

#include "backend.h"

enum sound_states {
	SOUND_MUTE = 0,
	SOUND_LOW,
	SOUND_HIGH,
};

static const char *sound_glyphs[] = {
	[SOUND_MUTE] = "",
	[SOUND_LOW] = "",
	[SOUND_HIGH] = "",
};

static struct {
	snd_mixer_t *ctx;
	snd_mixer_elem_t *elem;
	bool is_muted;
	int volume_percent;
} mixer_handler;


static int get_volume_percent(snd_mixer_elem_t *elem)
{
	long min, max, v_left;
	int ret;

	ret = snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	if (ret < 0)
		return -1;

	if (max == min)
		return -1;

	ret = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT,  &v_left);
	if (ret < 0)
		return -1;

	ret = (v_left - min) * 100 / (max - min);

	return ret;
}


static bool is_front_left_muted(snd_mixer_elem_t *elem)
{
	int sw;
	int ret;

	ret = snd_mixer_selem_has_playback_switch(elem);
	if (!ret) /* no mute switch, treat as unmuted */
		return 0;

	ret = snd_mixer_selem_has_playback_channel(elem, SND_MIXER_SCHN_FRONT_LEFT);
	if (!ret) /* no FL channel, treat as unmuted */
		return 0;

	ret = snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
	if (ret < 0)
		return 0; /* the world is on fire */

	return !sw;
}


static int elem_callback(snd_mixer_elem_t *elem, unsigned int mask)
{
	if (!(mask & (SND_CTL_EVENT_MASK_VALUE |
	              SND_CTL_EVENT_MASK_ADD |
	              SND_CTL_EVENT_MASK_REMOVE)))
		return 0;

	if (elem != mixer_handler.elem)
		return 0;

	mixer_handler.is_muted = is_front_left_muted(elem) ? true : false;
	mixer_handler.volume_percent = get_volume_percent(elem);

	update_status_cb(NULL);
	return 0;
}

/* uv poll callback */
void alsa_mixer_poll_cb(uv_poll_t *handle, int status, int events)
{
	(void)handle;
	(void)events;
	int n;

	/* if this is the case, probably the world is on fire */
	if (status < 0) {
		fprintf(stderr, "uv poll error: %s\n", uv_strerror(status));
		return;
	}

	n = snd_mixer_handle_events(mixer_handler.ctx);
	if (n < 0)
		fprintf(stderr, "snd_mixer_handle_events: %s\n", snd_strerror(n));
}


char *get_volume(void)
{
	int state;

	if (mixer_handler.is_muted || mixer_handler.volume_percent == 0) {
		state = SOUND_MUTE;
	} else {
		if (mixer_handler.volume_percent < 50)
			state = SOUND_LOW;
		else
			state = SOUND_HIGH;
	}
	return smprintf("%s %d%%", sound_glyphs[state], mixer_handler.volume_percent);
}


int setup_volume_monitor(const char *card, const char *selem_name)
{
	int ret;
	int pfds_count;
	struct pollfd *pfds;
	snd_mixer_selem_id_t *control_id;

	ret = snd_mixer_open(&mixer_handler.ctx, 0);
	if (ret < 0) {
		fprintf(stderr, "snd_mixer_open: %s\n", snd_strerror(ret));
		return -1;
	}

	ret = snd_mixer_attach(mixer_handler.ctx, card);
	if (ret < 0) {
		fprintf(stderr, "snd_mixer_attach: %s\n", snd_strerror(ret));
		goto fail_setup;
	}

	ret = snd_mixer_selem_register(mixer_handler.ctx, NULL, NULL);
	if (ret < 0) {
		fprintf(stderr, "snd_mixer_selem_register: %s\n", snd_strerror(ret));
		goto fail_setup;
	}

	ret = snd_mixer_load(mixer_handler.ctx);
	if (ret < 0) {
		fprintf(stderr, "snd_mixer_load: %s\n", snd_strerror(ret));
		goto fail_setup;
	}

	ret = snd_mixer_selem_id_malloc(&control_id);
	if (ret < 0) {
		fprintf(stderr, "snd_mixer_selem_id_malloc: %s\n", snd_strerror(ret));
		goto fail_setup;
	}

	snd_mixer_selem_id_set_index(control_id, 0);
	snd_mixer_selem_id_set_name(control_id, selem_name);
	mixer_handler.elem = snd_mixer_find_selem(mixer_handler.ctx, control_id);

	snd_mixer_selem_id_free(control_id);

	if (!mixer_handler.elem) {
		fprintf(stderr, "Cannot find '%s' element\n", selem_name);
		goto fail_setup;
	}

	snd_mixer_elem_set_callback(mixer_handler.elem, elem_callback);

	mixer_handler.is_muted = is_front_left_muted(mixer_handler.elem) ? true : false;
	mixer_handler.volume_percent = get_volume_percent(mixer_handler.elem);

	pfds_count = snd_mixer_poll_descriptors_count(mixer_handler.ctx);
	if (pfds_count <= 0) {
		fprintf(stderr, "No mixer fds\n");
		goto fail_setup;
	}

	pfds = calloc((size_t)pfds_count, sizeof(*pfds));
	if (!pfds) {
		fprintf(stderr, "calloc for pollfds failed\n");
		goto fail_setup;
	}

	if ((ret = snd_mixer_poll_descriptors(mixer_handler.ctx, pfds, pfds_count)) < 0) {
		fprintf(stderr, "snd_mixer_poll_descriptors: %s\n", snd_strerror(ret));
		goto fail_poll;
	}

	/* for the sake of this project, we only care about one fd. */
	ret = pfds[0].fd;
	free(pfds);

	return ret;

fail_poll:
	free(pfds);
fail_setup:
	snd_mixer_close(mixer_handler.ctx);
	return -1;
}
