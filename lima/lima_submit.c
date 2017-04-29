/*
 * Copyright (C) 2017 Lima Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xf86drm.h"
#include "lima_priv.h"
#include "lima.h"
#include "lima_drm.h"


int lima_submit_create(lima_device_handle dev, uint32_t pipe, lima_submit_handle *submit)
{
	struct lima_submit *s;

	s = calloc(sizeof(*s), 1);
	if (!s)
		return -ENOMEM;

	s->dev = dev;
	s->pipe = pipe;
	*submit = s;
	return 0;
}

void lima_submit_delete(lima_submit_handle submit)
{
	if (submit->bos)
		free(submit->bos);
	free(submit);
}

int lima_submit_add_bo(lima_submit_handle submit, lima_bo_handle bo, uint32_t flags)
{
	uint32_t new_bos = 8;

	if (submit->bos && submit->max_bos == submit->nr_bos)
		new_bos = submit->max_bos * 2;

	if (new_bos > submit->max_bos) {
		void *bos = realloc(submit->bos, sizeof(*submit->bos) * new_bos);
		if (!bos)
			return -ENOMEM;
		submit->max_bos = new_bos;
		submit->bos = bos;
	}

	submit->bos[submit->nr_bos].handle = bo->handle;
	submit->bos[submit->nr_bos].flags = flags;
	submit->nr_bos++;
	return 0;
}

void lima_submit_remove_bo(lima_submit_handle submit, lima_bo_handle bo)
{
	uint32_t i;

	for (i = 0; i < submit->nr_bos; i++) {
		if (submit->bos[i].handle == bo->handle) {
			submit->nr_bos--;
			memmove(submit->bos + i, submit->bos + i + 1,
				sizeof(*submit->bos) * (submit->nr_bos - i));
			return;
		}
	}
}

void lima_submit_set_frame(lima_submit_handle submit, void *frame, uint32_t size)
{
	submit->frame = frame;
	submit->frame_size = size;
}

int lima_submit_start(lima_submit_handle submit)
{
	int err;
	struct drm_lima_gem_submit req = {
		.fence = 0,
		.pipe = submit->pipe,
		.nr_bos = submit->nr_bos,
		.bos = VOID2U64(submit->bos),
		.frame = VOID2U64(submit->frame),
		.frame_size = submit->frame_size,
	};

	err = drmIoctl(submit->dev->fd, DRM_IOCTL_LIMA_GEM_SUBMIT, &req);
	if (err)
		return err;

	submit->fence = req.fence;
	return 0;
}
