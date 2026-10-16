/**************************************************************************/
/*  spu_job_queue.cpp                                                     */
/**************************************************************************/

#include "spu_job_queue.h"

#include <cstring>

#define SPU_JOB_QUEUE_MAX 32

static SPUJob g_pending[SPU_JOB_QUEUE_MAX];
static int g_pending_count = 0;

void spu_job_queue_enqueue(const SPUJob *p_job) {
	if (!p_job || g_pending_count >= SPU_JOB_QUEUE_MAX) {
		return;
	}
	g_pending[g_pending_count++] = *p_job;
}

void spu_job_queue_process_all(void) {
	for (int i = 0; i < g_pending_count; i++) {
		const SPUJob *job = &g_pending[i];
		if (job->data && job->size > 0) {
			// M9: PPU fallback until SPU ELF jobs are linked.
			(void)job;
		}
	}
	g_pending_count = 0;
}

int spu_job_queue_has_jobs(void) {
	return g_pending_count > 0;
}

void spu_job_queue_flush_canvas(void) {
	spu_job_queue_process_all();
}
