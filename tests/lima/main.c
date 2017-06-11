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

#ifdef LIMA_TEST_HAVE_LIBPNG
#include <png.h>
#endif

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
	uint32_t va, size = 4096, handle;
	struct lima_bo_import_result result;

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

	assert(!lima_bo_export(bo, lima_bo_handle_type_gem_flink_name, &handle));
	assert(handle);
	assert(!lima_bo_import(dev, lima_bo_handle_type_gem_flink_name, handle, &result));
	assert(result.bo == bo);
	assert(!lima_bo_free(bo));
	printf("bo flink name export/import success\n");

	assert(!lima_bo_export(bo, lima_bo_handle_type_kms, &handle));
	assert(handle);
	assert(!lima_bo_import(dev, lima_bo_handle_type_kms, handle, &result));
	assert(result.bo == bo);
	assert(!lima_bo_free(bo));
	printf("bo kms export/import success\n");

	assert(!lima_bo_wait(bo, LIMA_BO_WAIT_FLAG_READ, 0, false));
	assert(!lima_bo_wait(bo, LIMA_BO_WAIT_FLAG_WRITE, 0, false));
	printf("bo wait success\n");

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
	uint32_t submit_flags[2];
	struct init_data *init_data;
	int num_init_data;
};

struct lima_dumped_mem_content {
	unsigned int offset;
	unsigned int size;
	unsigned int memory[];
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

#ifdef LIMA_TEST_HAVE_LIBPNG
void write_image(char *filename, int width, int height, void *buffer, char *title)
{
	int code = 0, i;
	FILE *fp = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;

	fp = fopen(filename, "wb");
	if (fp == NULL) {
		printf("Could not open file %s for writing\n", filename);
		return;
	}

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		printf("Could not allocate write struct\n");
		goto out0;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		printf("Could not allocate info struct\n");
		goto out1;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		printf("Error during png creation\n");
		goto out2;
	}

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, width, height,
		     8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	if (title != NULL) {
		png_text title_text;
		title_text.compression = PNG_TEXT_COMPRESSION_NONE;
		title_text.key = "Title";
		title_text.text = title;
		png_set_text(png_ptr, info_ptr, &title_text, 1);
	}

	png_write_info(png_ptr, info_ptr);

	for (i = 0; i < height; i++)
		png_write_row(png_ptr, (png_bytep)buffer + i * width * 4);

	png_write_end(png_ptr, NULL);

out2:
	png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
out1:
	png_destroy_write_struct(&png_ptr, NULL);
out0:
	fclose(fp);
}
#endif

static void submit_test(lima_device_handle dev)
{
#include "red_triangle.h"

	int i, j;
	lima_submit_handle submit;

	/* create and init bos */
	for (i = 0; i < ARRAY_SIZE(bos); i++) {
		create_test_bo(dev, &bos[i]);
		if (bos[i].init_data && bos[i].num_init_data) {
			for (j = 0; j < bos[i].num_init_data; j++) {
				struct init_data *init_data = bos[i].init_data + j;
				memcpy(bos[i].cpu + init_data->offset,
				       init_data->data, init_data->size);
			}
		}
	}

	/* test gp */
	assert(!lima_submit_create(dev, 0, &submit));
	for (i = 0; i < ARRAY_SIZE(bos) - 1; i++)
		assert(!lima_submit_add_bo(submit, bos[i].bo, bos[i].submit_flags[0]));

	lima_submit_set_frame(submit, &gp_frame, sizeof(gp_frame));
	assert(!lima_submit_start(submit));

	assert(!lima_submit_wait(submit, 1000000000, true));

	assert(!memcmp(bos[0].cpu + 0x14400, varying, sizeof(varying)));

	for (i = 0; i < ARRAY_SIZE(plbs); i++)
		assert(!memcmp(bos[1].cpu + plbs[i]->offset, plbs[i]->memory, plbs[i]->size));

	lima_submit_delete(submit);
	printf("gp submit test success\n");

	/* test pp */
	assert(!lima_submit_create(dev, 1, &submit));
	for (i = 0; i < ARRAY_SIZE(bos); i++)
		assert(!lima_submit_add_bo(submit, bos[i].bo, bos[i].submit_flags[1]));

	lima_submit_set_frame(submit, &pp_frame, sizeof(pp_frame));
	assert(!lima_submit_start(submit));

	assert(!lima_submit_wait(submit, 1000000000, true));

#ifdef LIMA_TEST_HAVE_LIBPNG
	write_image("output.png", 800, 480, bos[2].cpu, "mali");
#endif

	lima_submit_delete(submit);
	printf("pp submit test success\n");

	for (i = 0; i < ARRAY_SIZE(bos); i++)
		free_test_bo(dev, &bos[i]);
	printf("submit test success\n");
}

int main(int argc, char **argv)
{
	int fd;
	drmVersionPtr version;
	lima_device_handle dev;
	struct lima_device_info info;
	char *dri_dev = "/dev/dri/card0";

	if (argc > 1)
		dri_dev = argv[1];
	assert((fd = open(dri_dev, O_RDWR)) >= 0);

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
