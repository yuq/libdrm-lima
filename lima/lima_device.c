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

#include "xf86drm.h"
#include "lima_drm.h"
#include "lima_priv.h"
#include "lima.h"

int lima_device_create(int fd, lima_device_handle *dev)
{
	int err;
	struct lima_device *ldev;

	ldev = calloc(1, sizeof(*ldev));
	if (!ldev)
		return -ENOMEM;

	ldev->fd = fd;
	err = lima_vamgr_init(&ldev->vamgr);
	if (err)
		goto err_out0;

	ldev->bo_handles = drmHashCreate();
	if (!ldev->bo_handles) {
		err = -ENOMEM;
		goto err_out1;
	}

	ldev->bo_flink_names = drmHashCreate();
	if (!ldev->bo_flink_names) {
		err = -ENOMEM;
		goto err_out2;
	}

	pthread_mutex_init(&ldev->bo_table_mutex, NULL);
	*dev = ldev;
	return 0;

err_out2:
	drmHashDestroy(ldev->bo_handles);
err_out1:
	lima_vamgr_fini(&ldev->vamgr);
err_out0:
	free(ldev);
	return err;
}

void lima_device_delete(lima_device_handle dev)
{
	pthread_mutex_destroy(&dev->bo_table_mutex);
	drmHashDestroy(dev->bo_handles);
	drmHashDestroy(dev->bo_flink_names);
	lima_vamgr_fini(&dev->vamgr);
	free(dev);
}

int lima_device_query_info(lima_device_handle dev, struct lima_device_info *info)
{
	int err;
	struct drm_lima_info drm_info;

	err = drmIoctl(dev->fd, DRM_IOCTL_LIMA_INFO, &drm_info);
	if (err)
		return err;

	switch (drm_info.gpu_id) {
	case LIMA_INFO_GPU_MALI400:
		info->gpu_type = GPU_MALI400;
		break;
	default:
		return -ENODEV;
	}

	info->num_pp = drm_info.num_pp;
	return 0;
}
