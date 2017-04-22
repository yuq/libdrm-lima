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

#ifndef _LIMA_H_
#define _LIMA_H_

#include <stdint.h>

enum lima_gpu_type {
	GPU_MALI400,
};

struct lima_device_info {
	enum lima_gpu_type gpu_type;
	uint32_t num_pp;
};

struct lima_bo_create_request {
	uint32_t size;
	uint32_t flags;
};

typedef struct lima_device *lima_device_handle;
typedef struct lima_bo *lima_bo_handle;

int lima_device_create(int fd, lima_device_handle *dev);
void lima_device_delete(lima_device_handle dev);

int lima_device_query_info(lima_device_handle dev, struct lima_device_info *info);

int lima_bo_create(lima_device_handle dev, struct lima_bo_create_request *request,
		   lima_bo_handle *bo_handle);
int lima_bo_free(lima_bo_handle bo);
void *lima_bo_map(lima_bo_handle bo);

int lima_va_range_alloc(lima_device_handle dev, uint32_t size, uint32_t *va);
int lima_va_range_free(lima_device_handle dev, uint32_t size, uint32_t va);

int lima_bo_va_map(lima_bo_handle bo, uint32_t va, uint32_t flags);
int lima_bo_va_unmap(lima_bo_handle bo, uint32_t va);

#endif /* _LIMA_H_ */
