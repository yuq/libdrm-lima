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
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>

#include "lima.h"
#include "lima_drm.h"
#include "xf86drm.h"
#include "util_math.h"
#include "util/common.h"

char *gpu_name(enum lima_gpu_type type)
{
	switch (type) {
	case GPU_MALI400:
		return "MALI400";
	}

	return "unknown";
}

static lima_bo_handle create_bo(lima_device_handle dev, uint32_t size, uint32_t flags)
{
	lima_bo_handle bo;
	struct lima_bo_create_request req = {
		.size = size,
		.flags = flags,
	};

	assert(!lima_bo_create(dev, &req, &bo));
	return bo;
}

static void va_range_test(lima_device_handle dev)
{
	uint32_t va1, size1 = 4096, va2, size2 = 4096 * 2, va3, size3 = 4096 * 5;
	uint32_t rec;

	/* alloc test */
	assert(!lima_va_range_alloc(dev, size1, &va1));
	assert(va1 == 0);
	assert(!lima_va_range_alloc(dev, size2, &va2));
	assert(va2 == va1 + size1);
	assert(!lima_va_range_alloc(dev, size3, &va3));
	assert(va3 == va2 + size2);
	/* free middle test */
	rec = va2;
	assert(!lima_va_range_free(dev, size2, va2));
	assert(!lima_va_range_alloc(dev, size2, &va2));
	assert(va2 == rec);
	/* free front test */
	rec = va1;
	assert(!lima_va_range_free(dev, size1, va1));
	assert(!lima_va_range_alloc(dev, size1, &va1));
	assert(va1 == rec);
	/* free back test */
	rec = va3;
	assert(!lima_va_range_free(dev, size3, va3));
	assert(!lima_va_range_alloc(dev, size3, &va3));
	assert(va3 == rec);
	/* alloc next hole test */
	assert(!lima_va_range_free(dev, size1, va1));
	size1 = 4096 * 2;
	assert(!lima_va_range_alloc(dev, size1, &va1));
	assert(va1 == va3 + size3);
	/* free all test */
	assert(!lima_va_range_free(dev, size3, va3));
	assert(!lima_va_range_free(dev, size2, va2));
	assert(!lima_va_range_free(dev, size1, va1));

	printf("bo va range success\n");
}

static void bo_test(lima_device_handle dev)
{
	lima_bo_handle bo;
	char *cpu, cpu_partten[] = "this is a test string for mmap bo content\n";
	uint32_t va, size = 4096;

	bo = create_bo(dev, size, 0);
	printf("create bo success\n");

	assert((cpu = lima_bo_map(bo)) != NULL);
	memset(cpu, 0, size);
	strcpy(cpu, cpu_partten);
	assert(!strcmp(cpu_partten, cpu));
	assert(!lima_bo_unmap(bo));
	printf("mmap bo test success\n");

	assert(!lima_va_range_alloc(dev, size, &va));
	assert(!lima_bo_va_map(bo, va, 0));
	assert(!lima_bo_va_unmap(bo, va));
	assert(!lima_va_range_free(dev, size, va));
	printf("bo va map/unmap success\n");

	assert(!lima_bo_free(bo));
	printf("bo free success\n");
}

struct init_data {
	void *data;
	uint32_t offset;
	uint32_t size;
};

struct test_bo {
	lima_bo_handle bo;
	uint32_t size;
	uint32_t va;
	void *cpu;
	uint32_t submit_flags;
	struct init_data *init_data;
	int num_init_data;
};

#define INIT_DATA(d, o) { .data = d, .offset = o, .size = sizeof(d) }

static void create_test_bo(lima_device_handle dev, struct test_bo *bo)
{
	bo->bo = create_bo(dev, bo->size, 0);
	if (!bo->va)
		assert(!lima_va_range_alloc(dev, bo->size, &bo->va));
	assert(!lima_bo_va_map(bo->bo, bo->va, 0));
	assert((bo->cpu = lima_bo_map(bo->bo)));
	memset(bo->cpu, 0, bo->size);
}

static void free_test_bo(lima_device_handle dev, struct test_bo *bo)
{
	assert(!lima_bo_unmap(bo->bo));
	assert(!lima_bo_va_unmap(bo->bo, bo->va));
	assert(!lima_va_range_free(dev, bo->size, bo->va));
	assert(!lima_bo_free(bo->bo));
}

static void gp_submit_test(lima_device_handle dev)
{
#include "red_triangle.h"

	int i, j;
	lima_submit_handle submit;

	assert(!lima_submit_create(dev, 0, &submit));

	for (i = 0; i < ARRAY_SIZE(bos); i++) {
		create_test_bo(dev, &bos[i]);
		if (bos[i].init_data && bos[i].num_init_data) {
			for (j = 0; j < bos[i].num_init_data; j++) {
				struct init_data *init_data = bos[i].init_data + j;
				memcpy(bos[i].cpu + init_data->offset,
				       init_data->data, init_data->size);
			}
		}
		assert(!lima_submit_add_bo(submit, bos[i].bo, bos[i].submit_flags));
	}

	lima_submit_set_frame(submit, &frame, sizeof(frame));
	assert(!lima_submit_start(submit));

	assert(!lima_submit_wait(submit, 1000000000, true));

	assert(!memcmp(bos[0].cpu + 0x14400, varying, sizeof(varying)));

	lima_submit_delete(submit);
	for (i = 0; i < ARRAY_SIZE(bos); i++)
		free_test_bo(dev, &bos[i]);

	printf("gp submit test success\n");
}

int main(int argc, char **argv)
{
	int fd;
	drmVersionPtr version;
	lima_device_handle dev;
	struct lima_device_info info;

	assert(argc > 1);
	assert((fd = open(argv[1], O_RDWR)) >= 0);

	assert((version = drmGetVersion(fd)));
	printf("Version: %d.%d.%d\n", version->version_major,
	       version->version_minor, version->version_patchlevel);
	printf("  Name: %s\n", version->name);
	printf("  Date: %s\n", version->date);
	printf("  Description: %s\n", version->desc);
	drmFreeVersion(version);

	assert(!lima_device_create(fd, &dev));

	assert(!lima_device_query_info(dev, &info));
	printf("Lima gpu is %sMP%d\n", gpu_name(info.gpu_type), info.num_pp);

	va_range_test(dev);

	bo_test(dev);

	gp_submit_test(dev);

	lima_device_delete(dev);
	close(fd);
	return 0;
}
