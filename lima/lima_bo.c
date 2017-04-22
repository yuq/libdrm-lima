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
#include <errno.h>

#include "libdrm_macros.h"
#include "xf86drm.h"
#include "lima_priv.h"
#include "lima.h"
#include "lima_drm.h"


int lima_bo_create(lima_device_handle dev, struct lima_bo_create_request *request,
		   lima_bo_handle *bo_handle)
{
	int err;
	struct lima_bo *bo;
	struct drm_lima_gem_create drm_request = {
		.size = request->size,
		.flags = request->flags,
	};

	bo = calloc(1, sizeof(*bo));
	if (!bo)
		return -ENOMEM;

	err = drmIoctl(dev->fd, DRM_IOCTL_LIMA_GEM_CREATE, &drm_request);
	if (err) {
		free(bo);
		return err;
	}

	bo->dev = dev;
	bo->size = drm_request.size;
	bo->handle = drm_request.handle;
	atomic_set(&bo->refcnt, 1);

	*bo_handle = bo;
	return 0;
}

int lima_bo_free(lima_bo_handle bo)
{
	int err;
	struct drm_gem_close req = {
		.handle = bo->handle,
	};

	if (!atomic_dec_and_test(&bo->refcnt))
		return 0;

	err = drmIoctl(bo->dev->fd, DRM_IOCTL_GEM_CLOSE, &req);
	free(bo);
	return err;
}

void *lima_bo_map(lima_bo_handle bo)
{
	if (!bo->map) {
		if (!bo->offset) {
			struct drm_lima_gem_info req = {
				.handle = bo->handle,
			};

			if (drmIoctl(bo->dev->fd, DRM_IOCTL_LIMA_GEM_INFO, &req))
				return NULL;
			else
				bo->offset = req.offset;
		}

		bo->map = drm_mmap(0, bo->size, PROT_READ | PROT_WRITE,
				   MAP_SHARED, bo->dev->fd, bo->offset);
		if (bo->map == MAP_FAILED)
			bo->map = NULL;
	}

	return bo->map;
}

int lima_bo_va_map(lima_bo_handle bo, uint32_t va, uint32_t flags)
{
	struct drm_lima_gem_va req = {
		.handle = bo->handle,
		.op = LIMA_VA_OP_MAP,
		.flags = flags,
		.va = va,
	};

	return drmIoctl(bo->dev->fd, DRM_IOCTL_LIMA_GEM_VA, &req);
}

int lima_bo_va_unmap(lima_bo_handle bo, uint32_t va)
{
	struct drm_lima_gem_va req = {
		.handle = bo->handle,
		.op = LIMA_VA_OP_UNMAP,
		.flags = 0,
		.va = va,
	};

	return drmIoctl(bo->dev->fd, DRM_IOCTL_LIMA_GEM_VA, &req);
}
