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

int main(int argc, char **argv)
{
	int fd;
	drmVersionPtr version;
	lima_device_handle dev;
	struct lima_device_info info;
	lima_bo_handle bo;
	struct lima_bo_create_request bo_create_req;
	char *cpu, cpu_partten[] = "this is a test string for mmap bo content\n";
	uint32_t va;

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

	bo_create_req.size = 4096;
	bo_create_req.flags = 0;
	assert(!lima_bo_create(dev, &bo_create_req, &bo));
	printf("create bo success\n");

	assert((cpu = lima_bo_map(bo)) != NULL);
	memset(cpu, 0, 4096);
	strcpy(cpu, cpu_partten);
	assert(!strcmp(cpu_partten, cpu));
	printf("mmap bo test success\n");

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

	assert(!lima_va_range_alloc(dev, 4096, &va));
	assert(va == 0);
	assert(!lima_bo_va_map(bo, va, 0));
	assert(!lima_bo_va_unmap(bo, va));
	assert(!lima_va_range_free(dev, 4096, va));
	printf("bo va map success\n");

	assert(!lima_bo_free(bo));

	lima_device_delete(dev);
	close(fd);
	return 0;
}
