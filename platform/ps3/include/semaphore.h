#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int value;
} sem_t;

static inline int sem_init(sem_t *sem, int pshared, unsigned int value) {
	if (!sem) {
		return -1;
	}
	(void)pshared;
	sem->value = (int)value;
	return 0;
}

static inline int sem_destroy(sem_t *sem) {
	(void)sem;
	return 0;
}

static inline int sem_post(sem_t *sem) {
	if (!sem) {
		return -1;
	}
	sem->value++;
	return 0;
}

static inline int sem_wait(sem_t *sem) {
	if (!sem) {
		return -1;
	}
	while (sem->value <= 0) {
	}
	sem->value--;
	return 0;
}

#ifdef __cplusplus
}
#endif
