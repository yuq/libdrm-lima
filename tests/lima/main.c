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

struct test_bo {
	lima_bo_handle bo;
	uint32_t size;
	uint32_t va;
	void *cpu;
	uint32_t submit_flags;
	void *init_data;
	uint32_t init_data_size;
};

static void create_test_bo(lima_device_handle dev, struct test_bo *bo)
{
	bo->bo = create_bo(dev, bo->size, 0);
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
	uint32_t vshader[] = {
	        0xad4ad6b5, 0x0380a2cc, 0x0007ff80, 0x000ad500,
		0xad4685c2, 0x438002b5, 0x0007ff80, 0x000ad500,
		0xad4cc980, 0x438022d9, 0x0007ff80, 0x000ad500,
		0xad48ca3b, 0x038041d3, 0x0007ff80, 0x000ad500,
		0x6c8b66b5, 0x03804193, 0x4243c080, 0x000ac508,
	};
	uint32_t fshader[] = {
		0x00021025, 0x0000014c, 0x03c007cf, 0x00000000,
	};
	uint32_t uniform[] = {
	        0x43c80000, 0xc3700000, 0x3f000000, 0x3f800000,
		0x43c80000, 0x43700000, 0x3f000000, 0x00000000,
		0x3f800000, 0x00000000, 0x00000000, 0x00000000,
	};
	uint32_t attribute[] = {
	        0x00000000, 0x00006002, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xbec00000, 0x3f200000, 0xbdcccccd, 0xbec00000,
		0xbf200000, 0xbdcccccd, 0x3ec00000, 0xbf200000,
		0xbdcccccd, 0x00000000, 0x00000000, 0x00000000,
	};
	uint32_t varying[] = {
		0x00000000, 0x00008020, 0x00000000, 0x00000000,
	};
	uint32_t vs_cmd[] = {
		0x00028000, 0x50000000,
		0x00000001, 0x50000000,

		0x00000000, /* 4 uniform va address */
		0x30000000 | (ALIGN(sizeof(uniform), 16) << 12),

		0x00000000, /* 6 shader va address */
		0x40000000 | (ALIGN(sizeof(vshader), 16) << 12),

		/* attribute prefetch */
		((3 - 1) << 20) | (((sizeof(vshader) / 16) - 1) << 10),
		0x10000040,

		/* varying count */ /* attribute count */
		((1 - 1) << 8) | ((1 - 1) << 24),
		0x10000042,

		0x00000003, 0x10000041,

		0x00000000, /* 14 attribute va address */
		0x20000000 | (1 << 17), /* attribute count */

		0x00000000, /* 16 varying va address */
		0x20000008 | ((1 + 1) << 17), /* varying count */

		(3 << 24), /* vertex count */
		0x00000000,

		0x00000000, 0x60000000,
		0x00000000, 0x50000000,
	};
	uint32_t plbu_cmd[] = {
		0x00000200, 0x1000010b, 0x10010001, 0x1000010c,
		0x31001d00, 0x10000109, 0x00000019, 0x30000000,
		0x00000000, 0x28000177, 0x00000000, 0x10000107, /* 8 */
		0x44480000, 0x10000108, 0x00000000, 0x10000105,
		0x43f00000, 0x10000106, 0x00010002, 0x60000000,
		0x00002200, 0x1000010b, 0x00000000, 0x80000000, /* 22 23 */
		0x00000000, 0x1000010a, 0x00000000, 0x1000010e,
		0x3f800000, 0x1000010f, 0x03000000, 0x00040000,
		0x00010001, 0x60000000, 0x00000000, 0x50000000,
	};
	uint32_t render_state[] = {
		0x00000000, 0x00000000, 0xfc3b1ad2, 0x00000033,
		0xffff0000, 0xff000007, 0xff000007, 0x00000000,
		0x0000f807, 0x00000000, 0x00000000, 0x00000000, /* 9 */
		0x00000000, 0x00000300, 0x00003000, 0x00000000,
	};

	enum {
		PROGRAM = 0,
		UNIFORM,
		ATTRIBUTE,
		VARYING,
		VS_CMD,
		PLBU_CMD,
		TILE_HEAP,
		PLB,
		NUM_BO,
	};
	struct test_bo bos[NUM_BO] = {
		[PROGRAM ... NUM_BO - 1].size = 0x1000,
		[TILE_HEAP].size = 0x8000,
		[PLB].size = 0x30000,

		[PROGRAM ... NUM_BO - 1].submit_flags = LIMA_SUBMIT_BO_FLAG_READ,
		[VARYING].submit_flags = LIMA_SUBMIT_BO_FLAG_WRITE,
		[TILE_HEAP].submit_flags = LIMA_SUBMIT_BO_FLAG_WRITE,

		[PROGRAM].init_data = vshader,
		[PROGRAM].init_data_size = sizeof(vshader),

		[UNIFORM].init_data = uniform,
		[UNIFORM].init_data_size = sizeof(uniform),

		[ATTRIBUTE].init_data = attribute,
		[ATTRIBUTE].init_data_size = sizeof(attribute),

		[VARYING].init_data = varying,
		[VARYING].init_data_size = sizeof(varying),

		[VS_CMD].init_data = vs_cmd,
		[VS_CMD].init_data_size = sizeof(vs_cmd),

		[PLBU_CMD].init_data = plbu_cmd,
		[PLBU_CMD].init_data_size = sizeof(plbu_cmd),

		[TILE_HEAP].init_data = NULL,
		[TILE_HEAP].init_data_size = 0,

		[PLB].init_data = render_state,
		[PLB].init_data_size = sizeof(render_state),
	};

	int i;
	lima_submit_handle submit;
	struct drm_lima_m400_gp_frame frame = {0};

	assert(!lima_submit_create(dev, 0, &submit));

	for (i = 0; i < NUM_BO; i++) {
		create_test_bo(dev, &bos[i]);
		assert(!lima_submit_add_bo(submit, bos[i].bo, bos[i].submit_flags));
	}

	/* resolve address */
	attribute[0] = bos[ATTRIBUTE].va + 0x40;
	varying[0] = bos[ATTRIBUTE].va + 0x100;

	vs_cmd[4] = bos[UNIFORM].va;
	vs_cmd[6] = bos[PROGRAM].va;
	vs_cmd[14] = bos[ATTRIBUTE].va;
	vs_cmd[16] = bos[VARYING].va;

	memcpy(bos[PROGRAM].cpu + sizeof(vshader), fshader, sizeof(fshader));
	render_state[9] = (bos[PROGRAM].va + sizeof(vshader)) | 0x05;

	plbu_cmd[8] = bos[PLB].va + sizeof(render_state); /* list of PLB address */
	plbu_cmd[22] = bos[PLB].va; /* render state */
	plbu_cmd[23] |= varying[0] >> 4;

	for (i = 0; i < 376; i++)
		((uint32_t *)(bos[PLB].cpu + sizeof(render_state)))[i] =
			bos[PLB].va + 0x1000 + 0x200 * i;

	for (i = 0; i < NUM_BO; i++) {
		if (bos[i].init_data && bos[i].init_data_size)
			memcpy(bos[i].cpu, bos[i].init_data, bos[i].init_data_size);
	}

	frame.vs_cmd_start = bos[VS_CMD].va;
	frame.vs_cmd_end = bos[VS_CMD].va + sizeof(vs_cmd);
	frame.plbu_cmd_start = bos[PLBU_CMD].va;
	frame.plbu_cmd_end = bos[PLBU_CMD].va + sizeof(plbu_cmd);
	frame.tile_heap_start = bos[TILE_HEAP].va;
	frame.tile_heap_end = bos[TILE_HEAP].va + bos[TILE_HEAP].size;

	lima_submit_set_frame(submit, &frame, sizeof(frame));
	assert(!lima_submit_start(submit));

	assert(!lima_submit_wait(submit, 1000000000, true));

	printf("gp test output %08x %08x %08x %08x %08x %08x %08x %08x\n",
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[0],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[1],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[2],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[3],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[4],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[5],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[6],
	       ((uint32_t *)(bos[VARYING].cpu + 0x100))[7]);
	printf("gp test plb %08x %08x %08x %08x %08x %08x %08x %08x\n",
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[0],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[1],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[2],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[3],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[4],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[5],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[6],
	       ((uint32_t *)(bos[PLB].cpu + 0x1000 + 0x7200))[7]);

	lima_submit_delete(submit);
	for (i = 0; i < NUM_BO; i++)
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
