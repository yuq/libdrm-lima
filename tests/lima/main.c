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
#include "xf86drm.h"

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
	printf("mmap bo test success\n");

	assert(!lima_va_range_alloc(dev, size, &va));
	assert(va == 0);
	assert(!lima_bo_va_map(bo, va, 0));
	assert(!lima_bo_va_unmap(bo, va));
	assert(!lima_va_range_free(dev, size, va));
	printf("bo va map/unmap success\n");

	assert(!lima_bo_free(bo));
	printf("bo free success\n");
}

#define SUBMIT_TEST_NUM_BOS 3

static void submit_test(lima_device_handle dev)
{
	int i;
	lima_bo_handle bos[SUBMIT_TEST_NUM_BOS];
	uint32_t bos_size[SUBMIT_TEST_NUM_BOS] = {4096, 4096 * 3, 4096 * 10};
	uint32_t vas[SUBMIT_TEST_NUM_BOS];
	lima_submit_handle submit;
	char frame[256];

	assert(!lima_submit_create(dev, 0, &submit));

	for (i = 0; i < SUBMIT_TEST_NUM_BOS; i++) {
		bos[i] = create_bo(dev, bos_size[i], 0);
		assert(!lima_va_range_alloc(dev, bos_size[i], vas + i));
		assert(!lima_bo_va_map(bos[i], vas[i], 0));
		assert(!lima_submit_add_bo(submit, bos[i], LIMA_SUBMIT_BO_FLAG_READ));
	}

	lima_submit_remove_bo(submit, bos[1]);
	assert(!lima_submit_add_bo(submit, bos[1], LIMA_SUBMIT_BO_FLAG_WRITE));

	lima_submit_set_frame(submit, frame, sizeof(frame));
	assert(!lima_submit_start(submit));

	lima_submit_delete(submit);
	for (i = 0; i < SUBMIT_TEST_NUM_BOS; i++) {
		assert(!lima_bo_va_unmap(bos[i], vas[i]));
		assert(!lima_va_range_free(dev, bos_size[i], vas[i]));
		assert(!lima_bo_free(bos[i]));
	}

	printf("submit test success\n");
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

	submit_test(dev);

	lima_device_delete(dev);
	close(fd);
	return 0;
}
