/**************************************************************************
 *
 * Copyright 2019 Collabora Ltd
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "drm-uapi/panfrost_drm.h"

#include "util/u_memory.h"

#include "pan_context.h"
#include "pan_perfcnt.h"
#include "pan_screen.h"


#define PANFROST_COUNTER(_id, _name)	\
	{				\
		.id = _id,		\
		.name = _name,		\
	}

#define PANFROST_BLK_COUNTERS(_blk, ...)						\
	.block[PANFROST_##_blk##_BLOCK] = {						\
		.counters = (struct panfrost_counter[]) { __VA_ARGS__ },		\
		.ncounters = sizeof((struct panfrost_counter[]) { __VA_ARGS__ }) /	\
			     sizeof(struct panfrost_counter),				\
	}

struct panfrost_gpu_counters {
	unsigned int gpu_id;
	const struct panfrost_counters *counters;
};

static const struct panfrost_gpu_counters gpus[0];

static const char *block_names[] = {
	[PANFROST_SHADER_BLOCK] = "SHADER",
	[PANFROST_TILER_BLOCK] = "TILER",
	[PANFROST_MMU_L2_BLOCK] = "MMU_L2",
	[PANFROST_JM_BLOCK] = "JM",
};

static void panfrost_perfcnt_init_queries(struct panfrost_screen *pscreen)
{
	struct panfrost_perfcnt_query_info *qinfo;
	const struct panfrost_counters *counters;
	unsigned int i, j, k;

	counters = pscreen->perfcnt_info.counters;
	for (i = 0; i < PANFROST_NUM_BLOCKS; i++) {
		for (j = 0; j < 64; j++) {
			if (!((1ULL << j) & pscreen->perfcnt_info.instances[i]))
				continue;

			pscreen->perfcnt_info.nqueries += counters->block[i].ncounters;
		}
	}

	if (!pscreen->perfcnt_info.nqueries)
		return;

	pscreen->perfcnt_info.queries = CALLOC(pscreen->perfcnt_info.nqueries,
					       sizeof(*pscreen->perfcnt_info.queries));
	assert(pscreen->perfcnt_info.queries);

	qinfo = pscreen->perfcnt_info.queries;

	for (i = 0; i < PANFROST_NUM_BLOCKS; i++) {
		for (j = 0; j < 64; j++) {

			if (!((1ULL << j) & pscreen->perfcnt_info.instances[i]))
				continue;

			for (k = 0; k < counters->block[i].ncounters; k++) {
				char *name;

				asprintf(&name, "%s.%s-%d",
					 counters->block[i].counters[k].name,
					 block_names[i], j);
				assert(name);
				qinfo->name = name;
				qinfo->block = i;
				qinfo->instance = j;
				qinfo->counter = k;
				qinfo++;
			}
		}
	}
}

static void panfrost_perfcnt_cleanup_queries(struct panfrost_screen *pscreen)
{
	unsigned int i;

	if (!pscreen->perfcnt_info.nqueries)
		return;

	for (i = 0; i < pscreen->perfcnt_info.nqueries; i++)
		FREE((void *)pscreen->perfcnt_info.queries[i].name);

	FREE(pscreen->perfcnt_info.queries);
}

static int panfrost_get_query_group_info(struct pipe_screen *screen,
					 unsigned index,
					 struct pipe_driver_query_group_info *info)
{
	struct panfrost_screen *pscreen = pan_screen(screen);

	if (!info)
		return 1;

	if (index)
		return 0;

	info->name = "Panfrost GPU counters";
	info->num_queries = pscreen->perfcnt_info.nqueries;
	info->max_active_queries = info->num_queries;
	return 1;
}

static int panfrost_get_query_info(struct pipe_screen *screen, unsigned index,
				   struct pipe_driver_query_info *info)
{
	struct panfrost_screen *pscreen = pan_screen(screen);
	struct panfrost_perfcnt_query_info *qinfo;

	if (!info)
		return pscreen->perfcnt_info.nqueries;

	if (index >= pscreen->perfcnt_info.nqueries)
		return 0;

	qinfo = &pscreen->perfcnt_info.queries[index];
	info->group_id = 0;
	info->flags = PIPE_DRIVER_QUERY_FLAG_BATCH;
	info->type = PIPE_DRIVER_QUERY_TYPE_UINT64;
	info->result_type = PIPE_DRIVER_QUERY_RESULT_TYPE_CUMULATIVE;
	info->query_type = PIPE_QUERY_DRIVER_SPECIFIC + index;
	info->name = qinfo->name;
	return 1;
}

void panfrost_perfcnt_init(struct panfrost_screen *pscreen)
{
	unsigned gpu_id, i;

	gpu_id = pscreen->driver->query_gpu_version(pscreen);
	for (i = 0; i < ARRAY_SIZE(gpus); i++) {
		if (gpus[i].gpu_id == gpu_id)
			break;
	}

	if (i == ARRAY_SIZE(gpus))
		return;

	pscreen->perfcnt_info.counters = gpus[i].counters;

	if (pscreen->driver->init_perfcnt)
		pscreen->driver->init_perfcnt(pscreen);

	panfrost_perfcnt_init_queries(pscreen);

	pscreen->base.get_driver_query_group_info = panfrost_get_query_group_info;
	pscreen->base.get_driver_query_info = panfrost_get_query_info;
}

void panfrost_perfcnt_cleanup(struct panfrost_screen *pscreen)
{
	panfrost_perfcnt_cleanup_queries(pscreen);
}
