/**************************************************************************/
/*  spu_job_queue.h                                                       */
/**************************************************************************/

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SPUJob {
	void *data;
	uint32_t size;
} SPUJob;

void spu_job_queue_enqueue(const SPUJob *p_job);
void spu_job_queue_process_all(void);
int spu_job_queue_has_jobs(void);
void spu_job_queue_flush_canvas(void);

#ifdef __cplusplus
}
#endif
