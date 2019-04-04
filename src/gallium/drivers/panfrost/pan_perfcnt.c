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

static const struct panfrost_counters t86x_counters = {
	PANFROST_BLK_COUNTERS(JM,
			      PANFROST_COUNTER(4, "MESSAGES_SENT"),
			      PANFROST_COUNTER(5, "MESSAGES_RECEIVED"),
			      PANFROST_COUNTER(6, "GPU_ACTIVE"),
			      PANFROST_COUNTER(7, "IRQ_ACTIVE"),
			      PANFROST_COUNTER(8, "JS0_JOBS"),
			      PANFROST_COUNTER(9, "JS0_TASKS"),
			      PANFROST_COUNTER(10, "JS0_ACTIVE"),
			      PANFROST_COUNTER(12, "JS0_WAIT_READ"),
			      PANFROST_COUNTER(13, "JS0_WAIT_ISSUE"),
			      PANFROST_COUNTER(14, "JS0_WAIT_DEPEND"),
			      PANFROST_COUNTER(15, "JS0_WAIT_FINISH"),
			      PANFROST_COUNTER(16, "JS1_JOBS"),
			      PANFROST_COUNTER(17, "JS1_TASKS"),
			      PANFROST_COUNTER(18, "JS1_ACTIVE"),
			      PANFROST_COUNTER(20, "JS1_WAIT_READ"),
			      PANFROST_COUNTER(21, "JS1_WAIT_ISSUE"),
			      PANFROST_COUNTER(22, "JS1_WAIT_DEPEND"),
			      PANFROST_COUNTER(23, "JS1_WAIT_FINISH"),
			      PANFROST_COUNTER(24, "JS2_JOBS"),
			      PANFROST_COUNTER(25, "JS2_TASKS"),
			      PANFROST_COUNTER(26, "JS2_ACTIVE"),
			      PANFROST_COUNTER(28, "JS2_WAIT_READ"),
			      PANFROST_COUNTER(29, "JS2_WAIT_ISSUE"),
			      PANFROST_COUNTER(30, "JS2_WAIT_DEPEND"),
			      PANFROST_COUNTER(31, "JS2_WAIT_FINISH")),
	PANFROST_BLK_COUNTERS(TILER,
			      PANFROST_COUNTER(3, "TI_JOBS_PROCESSED"),
			      PANFROST_COUNTER(4, "TI_TRIANGLES"),
			      PANFROST_COUNTER(5, "TI_QUADS"),
			      PANFROST_COUNTER(6, "TI_POLYGONS"),
			      PANFROST_COUNTER(7, "TI_POINTS"),
			      PANFROST_COUNTER(8, "TI_LINES"),
			      PANFROST_COUNTER(9, "TI_VCACHE_HIT"),
			      PANFROST_COUNTER(10, "TI_VCACHE_MISS"),
			      PANFROST_COUNTER(11, "TI_FRONT_FACING"),
			      PANFROST_COUNTER(12, "TI_BACK_FACING"),
			      PANFROST_COUNTER(13, "TI_PRIM_VISIBLE"),
			      PANFROST_COUNTER(14, "TI_PRIM_CULLED"),
			      PANFROST_COUNTER(15, "TI_PRIM_CLIPPED"),
			      PANFROST_COUNTER(16, "TI_LEVEL0"),
			      PANFROST_COUNTER(17, "TI_LEVEL1"),
			      PANFROST_COUNTER(18, "TI_LEVEL2"),
			      PANFROST_COUNTER(19, "TI_LEVEL3"),
			      PANFROST_COUNTER(20, "TI_LEVEL4"),
			      PANFROST_COUNTER(21, "TI_LEVEL5"),
			      PANFROST_COUNTER(22, "TI_LEVEL6"),
			      PANFROST_COUNTER(23, "TI_LEVEL7"),
			      PANFROST_COUNTER(24, "TI_COMMAND_1"),
			      PANFROST_COUNTER(25, "TI_COMMAND_2"),
			      PANFROST_COUNTER(26, "TI_COMMAND_3"),
			      PANFROST_COUNTER(27, "TI_COMMAND_4"),
			      PANFROST_COUNTER(28, "TI_COMMAND_5_7"),
			      PANFROST_COUNTER(29, "TI_COMMAND_8_15"),
			      PANFROST_COUNTER(30, "TI_COMMAND_16_63"),
			      PANFROST_COUNTER(31, "TI_COMMAND_64"),
			      PANFROST_COUNTER(32, "TI_COMPRESS_IN"),
			      PANFROST_COUNTER(33, "TI_COMPRESS_OUT"),
			      PANFROST_COUNTER(34, "TI_COMPRESS_FLUSH"),
			      PANFROST_COUNTER(35, "TI_TIMESTAMPS"),
			      PANFROST_COUNTER(36, "TI_PCACHE_HIT"),
			      PANFROST_COUNTER(37, "TI_PCACHE_MISS"),
			      PANFROST_COUNTER(38, "TI_PCACHE_LINE"),
			      PANFROST_COUNTER(39, "TI_PCACHE_STALL"),
			      PANFROST_COUNTER(40, "TI_WRBUF_HIT"),
			      PANFROST_COUNTER(41, "TI_WRBUF_MISS"),
			      PANFROST_COUNTER(42, "TI_WRBUF_LINE"),
			      PANFROST_COUNTER(43, "TI_WRBUF_PARTIAL"),
			      PANFROST_COUNTER(44, "TI_WRBUF_STALL"),
			      PANFROST_COUNTER(45, "TI_ACTIVE"),
			      PANFROST_COUNTER(46, "TI_LOADING_DESC"),
			      PANFROST_COUNTER(47, "TI_INDEX_WAIT"),
			      PANFROST_COUNTER(48, "TI_INDEX_RANGE_WAIT"),
			      PANFROST_COUNTER(49, "TI_VERTEX_WAIT"),
			      PANFROST_COUNTER(50, "TI_PCACHE_WAIT"),
			      PANFROST_COUNTER(51, "TI_WRBUF_WAIT"),
			      PANFROST_COUNTER(52, "TI_BUS_READ"),
			      PANFROST_COUNTER(53, "TI_BUS_WRITE"),
			      PANFROST_COUNTER(59, "TI_UTLB_HIT"),
			      PANFROST_COUNTER(60, "TI_UTLB_NEW_MISS"),
			      PANFROST_COUNTER(61, "TI_UTLB_REPLAY_FULL"),
			      PANFROST_COUNTER(62, "TI_UTLB_REPLAY_MISS"),
			      PANFROST_COUNTER(63, "TI_UTLB_STALL")),
	PANFROST_BLK_COUNTERS(SHADER,
			      PANFROST_COUNTER(4, "FRAG_ACTIVE"),
			      PANFROST_COUNTER(5, "FRAG_PRIMITIVES"),
			      PANFROST_COUNTER(6, "FRAG_PRIMITIVES_DROPPED"),
			      PANFROST_COUNTER(7, "FRAG_CYCLES_DESC"),
			      PANFROST_COUNTER(8, "FRAG_CYCLES_FPKQ_ACTIVE"),
			      PANFROST_COUNTER(9, "FRAG_CYCLES_VERT"),
			      PANFROST_COUNTER(10, "FRAG_CYCLES_TRISETUP"),
			      PANFROST_COUNTER(11, "FRAG_CYCLES_EZS_ACTIVE"),
			      PANFROST_COUNTER(12, "FRAG_THREADS"),
			      PANFROST_COUNTER(13, "FRAG_DUMMY_THREADS"),
			      PANFROST_COUNTER(14, "FRAG_QUADS_RAST"),
			      PANFROST_COUNTER(15, "FRAG_QUADS_EZS_TEST"),
			      PANFROST_COUNTER(16, "FRAG_QUADS_EZS_KILLED"),
			      PANFROST_COUNTER(17, "FRAG_THREADS_LZS_TEST"),
			      PANFROST_COUNTER(18, "FRAG_THREADS_LZS_KILLED"),
			      PANFROST_COUNTER(19, "FRAG_CYCLES_NO_TILE"),
			      PANFROST_COUNTER(20, "FRAG_NUM_TILES"),
			      PANFROST_COUNTER(21, "FRAG_TRANS_ELIM"),
			      PANFROST_COUNTER(22, "COMPUTE_ACTIVE"),
			      PANFROST_COUNTER(23, "COMPUTE_TASKS"),
			      PANFROST_COUNTER(24, "COMPUTE_THREADS"),
			      PANFROST_COUNTER(25, "COMPUTE_CYCLES_DESC"),
			      PANFROST_COUNTER(26, "TRIPIPE_ACTIVE"),
			      PANFROST_COUNTER(27, "ARITH_WORDS"),
			      PANFROST_COUNTER(28, "ARITH_CYCLES_REG"),
			      PANFROST_COUNTER(29, "ARITH_CYCLES_L0"),
			      PANFROST_COUNTER(30, "ARITH_FRAG_DEPEND"),
			      PANFROST_COUNTER(31, "LS_WORDS"),
			      PANFROST_COUNTER(32, "LS_ISSUES"),
			      PANFROST_COUNTER(33, "LS_REISSUE_ATTR"),
			      PANFROST_COUNTER(34, "LS_REISSUES_VARY"),
			      PANFROST_COUNTER(35, "LS_VARY_RV_MISS"),
			      PANFROST_COUNTER(36, "LS_VARY_RV_HIT"),
			      PANFROST_COUNTER(37, "LS_NO_UNPARK"),
			      PANFROST_COUNTER(38, "TEX_WORDS"),
			      PANFROST_COUNTER(39, "TEX_BUBBLES"),
			      PANFROST_COUNTER(40, "TEX_WORDS_L0"),
			      PANFROST_COUNTER(41, "TEX_WORDS_DESC"),
			      PANFROST_COUNTER(42, "TEX_ISSUES"),
			      PANFROST_COUNTER(43, "TEX_RECIRC_FMISS"),
			      PANFROST_COUNTER(44, "TEX_RECIRC_DESC"),
			      PANFROST_COUNTER(45, "TEX_RECIRC_MULTI"),
			      PANFROST_COUNTER(46, "TEX_RECIRC_PMISS"),
			      PANFROST_COUNTER(47, "TEX_RECIRC_CONF"),
			      PANFROST_COUNTER(48, "LSC_READ_HITS"),
			      PANFROST_COUNTER(49, "LSC_READ_OP"),
			      PANFROST_COUNTER(50, "LSC_WRITE_HITS"),
			      PANFROST_COUNTER(51, "LSC_WRITE_OP"),
			      PANFROST_COUNTER(52, "LSC_ATOMIC_HITS"),
			      PANFROST_COUNTER(53, "LSC_ATOMIC_OP"),
			      PANFROST_COUNTER(54, "LSC_LINE_FETCHES"),
			      PANFROST_COUNTER(55, "LSC_DIRTY_LINE"),
			      PANFROST_COUNTER(56, "LSC_SNOOPS"),
			      PANFROST_COUNTER(57, "AXI_TLB_STALL"),
			      PANFROST_COUNTER(58, "AXI_TLB_MISS"),
			      PANFROST_COUNTER(59, "AXI_TLB_TRANSACTION"),
			      PANFROST_COUNTER(60, "LS_TLB_MISS"),
			      PANFROST_COUNTER(61, "LS_TLB_HIT"),
			      PANFROST_COUNTER(62, "AXI_BEATS_READ"),
			      PANFROST_COUNTER(63, "AXI_BEATS_WRITTEN")),
	PANFROST_BLK_COUNTERS(MMU_L2,
			      PANFROST_COUNTER(4, "MMU_HIT"),
			      PANFROST_COUNTER(5, "MMU_NEW_MISS"),
			      PANFROST_COUNTER(6, "MMU_REPLAY_FULL"),
			      PANFROST_COUNTER(7, "MMU_REPLAY_MISS"),
			      PANFROST_COUNTER(8, "MMU_TABLE_WALK"),
			      PANFROST_COUNTER(9, "MMU_REQUESTS"),
			      PANFROST_COUNTER(12, "UTLB_HIT"),
			      PANFROST_COUNTER(13, "UTLB_NEW_MISS"),
			      PANFROST_COUNTER(14, "UTLB_REPLAY_FULL"),
			      PANFROST_COUNTER(15, "UTLB_REPLAY_MISS"),
			      PANFROST_COUNTER(16, "UTLB_STALL"),
			      PANFROST_COUNTER(30, "L2_EXT_WRITE_BEATS"),
			      PANFROST_COUNTER(31, "L2_EXT_READ_BEATS"),
			      PANFROST_COUNTER(32, "L2_ANY_LOOKUP"),
			      PANFROST_COUNTER(33, "L2_READ_LOOKUP"),
			      PANFROST_COUNTER(34, "L2_SREAD_LOOKUP"),
			      PANFROST_COUNTER(35, "L2_READ_REPLAY"),
			      PANFROST_COUNTER(36, "L2_READ_SNOOP"),
			      PANFROST_COUNTER(37, "L2_READ_HIT"),
			      PANFROST_COUNTER(38, "L2_CLEAN_MISS"),
			      PANFROST_COUNTER(39, "L2_WRITE_LOOKUP"),
			      PANFROST_COUNTER(40, "L2_SWRITE_LOOKUP"),
			      PANFROST_COUNTER(41, "L2_WRITE_REPLAY"),
			      PANFROST_COUNTER(42, "L2_WRITE_SNOOP"),
			      PANFROST_COUNTER(43, "L2_WRITE_HIT"),
			      PANFROST_COUNTER(44, "L2_EXT_READ_FULL"),
			      PANFROST_COUNTER(46, "L2_EXT_WRITE_FULL"),
			      PANFROST_COUNTER(47, "L2_EXT_R_W_HAZARD"),
			      PANFROST_COUNTER(48, "L2_EXT_READ"),
			      PANFROST_COUNTER(49, "L2_EXT_READ_LINE"),
			      PANFROST_COUNTER(50, "L2_EXT_WRITE"),
			      PANFROST_COUNTER(51, "L2_EXT_WRITE_LINE"),
			      PANFROST_COUNTER(52, "L2_EXT_WRITE_SMALL"),
			      PANFROST_COUNTER(53, "L2_EXT_BARRIER"),
			      PANFROST_COUNTER(54, "L2_EXT_AR_STALL"),
			      PANFROST_COUNTER(55, "L2_EXT_R_BUF_FULL"),
			      PANFROST_COUNTER(56, "L2_EXT_RD_BUF_FULL"),
			      PANFROST_COUNTER(57, "L2_EXT_R_RAW"),
			      PANFROST_COUNTER(58, "L2_EXT_W_STALL"),
			      PANFROST_COUNTER(59, "L2_EXT_W_BUF_FULL"),
			      PANFROST_COUNTER(60, "L2_EXT_R_BUF_FULL"),
			      PANFROST_COUNTER(61, "L2_TAG_HAZARD"),
			      PANFROST_COUNTER(62, "L2_SNOOP_FULL"),
			      PANFROST_COUNTER(63, "L2_REPLAY_FULL")),
};

static const struct panfrost_gpu_counters gpus[] = {
	{
		.gpu_id = 0x860,
		.counters = &t86x_counters,
	},
};

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
