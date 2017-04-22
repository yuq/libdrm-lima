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

#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "lima_priv.h"
#include "lima.h"
#include "util_math.h"

drm_private int
lima_vamgr_init(struct lima_va_mgr *mgr)
{
	struct lima_va_hole *hole;

	list_inithead(&mgr->va_holes);
	pthread_mutex_init(&mgr->lock, NULL);

	hole = malloc(sizeof(*hole));
	if (!hole)
		return -ENOMEM;

	hole->offset = 0;
	hole->size = 0x100000000;
	list_add(&hole->list, &mgr->va_holes);
	return 0;
}

drm_private void
lima_vamgr_fini(struct lima_va_mgr *mgr)
{
	struct lima_va_hole *hole, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(hole, tmp, &mgr->va_holes, list) {
		list_del(&hole->list);
		free(hole);
	}
	pthread_mutex_destroy(&mgr->lock);
}

int lima_va_range_alloc(lima_device_handle dev, uint32_t size, uint32_t *va)
{
	struct lima_va_mgr *mgr = &dev->vamgr;
	struct lima_va_hole *hole;
	int err = -ENOENT;

	size = ALIGN(size, LIMA_PAGE_SIZE);

	pthread_mutex_lock(&mgr->lock);

	LIST_FOR_EACH_ENTRY(hole, &mgr->va_holes, list) {
		if (hole->size == size) {
			*va = hole->offset;
			err = 0;

			list_del(&hole->list);
			free(hole);
			break;
		}
		else if (hole->size > size) {
			*va = hole->offset;
			err = 0;

			hole->offset += size;
			hole->size -= size;
			break;
		}
	}

	pthread_mutex_unlock(&mgr->lock);
	return err;
}

static int add_va_hole(struct list_head *head, uint32_t size, uint32_t va)
{
	struct lima_va_hole *hole;

	hole = malloc(sizeof(*hole));
	if (!hole)
		return -ENOMEM;

	hole->offset = va;
	hole->size = size;

	list_addtail(&hole->list, head);
	return 0;
}

int lima_va_range_free(lima_device_handle dev, uint32_t size, uint32_t va)
{
	struct lima_va_mgr *mgr = &dev->vamgr;
	struct lima_va_hole *hole;
	int err = 0;

	va &= ~(LIMA_PAGE_SIZE - 1);
	size = ALIGN(size, LIMA_PAGE_SIZE);

	pthread_mutex_lock(&mgr->lock);

	if (LIST_IS_EMPTY(&mgr->va_holes))
		err = add_va_hole(&mgr->va_holes, size, va);
	else {
		LIST_FOR_EACH_ENTRY(hole, &mgr->va_holes, list) {
			if (va + size == hole->offset) {
				hole->offset = va;
				hole->size += size;
				break;
			}
			else if (va + size < hole->offset) {
				err = add_va_hole(&hole->list, size, va);
				break;
			}
			else if (va == hole->offset + hole->size) {
				if (hole->list.next != &mgr->va_holes) {
					struct lima_va_hole *next;
					next = LIST_FIRST_ENTRY(
						&hole->list, struct lima_va_hole, list);
					if (next->offset == va + size) {
						size += next->size;
						list_del(&next->list);
						free(next);
					}
				}
				hole->size += size;
				break;
			}
			else if (va > hole->offset + hole->size) {
				if (hole->list.next == &mgr->va_holes) {
					err = add_va_hole(&mgr->va_holes, size, va);
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&mgr->lock);
	return err;
}
