/*
 * shared_memory.c
 *
 *  Created on: Sep 10, 2012
 *      Author: alexey
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <semaphore.h>
#include <pthread.h>

#include "data.h"
#include "dbgovernor_string_functions.h"
#include "shared_memory.h"

#define MAX_ITEMS_IN_TABLE 128000
#define SHARED_MEMORY_NAME "governor_bad_users_list"
#define SHARED_MEMORY_SEM "governor_bad_users_list_sem"

typedef struct __shm_structure {
	long numbers;
	char items[MAX_ITEMS_IN_TABLE][USERNAMEMAXLEN];
} shm_structure;


shm_structure *bad_list = NULL;
int shm_fd = 0;
sem_t *sem = NULL;

int init_bad_users_list_utility() {

	if ((shm_fd = shm_open(SHARED_MEMORY_NAME, (O_RDWR), 0755))
			< 0) {
		return -1;
	}

	if ((bad_list = (shm_structure *) mmap(0, sizeof(shm_structure), (PROT_READ
									| PROT_WRITE), MAP_SHARED, shm_fd, 0)) == MAP_FAILED) {
		close(shm_fd);
		return -1;
	}

	sem = sem_open(SHARED_MEMORY_SEM, O_CREAT, 0777, 1);

	if (sem == SEM_FAILED) {
		munmap((void *) bad_list, sizeof(shm_structure));
		close(shm_fd);
		return -1;
	}
	if (sem_wait(sem) == 0) {
		clear_bad_users_list();
		sem_post(sem);
	}

	return 0;
}

int remove_bad_users_list_utility() {
	if (sem != SEM_FAILED) sem_close(sem);
	if (bad_list) munmap((void *) bad_list, sizeof(shm_structure));
	close(shm_fd);
	return 0;
}

int init_bad_users_list() {
	//shm_unlink(SHARED_MEMORY_NAME);
	sem_unlink(SHARED_MEMORY_SEM);
	mode_t old_umask = umask(0);

	int first = 0;
	if ((shm_fd = shm_open(SHARED_MEMORY_NAME, (O_CREAT | O_EXCL | O_RDWR),
							0755)) > 0) {
		first = 1;
	} else if ((shm_fd = shm_open(SHARED_MEMORY_NAME, (O_CREAT | O_RDWR), 0755))
			< 0) {
		umask(old_umask);
		return -1;
	}

	if (first) {
		ftruncate(shm_fd, sizeof(shm_structure));
	}

	if ((bad_list = (shm_structure *) mmap(0, sizeof(shm_structure), (PROT_READ
									| PROT_WRITE), MAP_SHARED, shm_fd, 0)) == MAP_FAILED) {
		close(shm_fd);
		umask(old_umask);
		return -1;
	}

	sem = sem_open(SHARED_MEMORY_SEM, O_CREAT, 0777, 1);
	umask(old_umask);

	if (sem == SEM_FAILED) {
		munmap((void *) bad_list, sizeof(shm_structure));
		close(shm_fd);
		return -1;
	}
	if (sem_wait(sem) == 0) {
		clear_bad_users_list();
		sem_post(sem);
	}

	return 0;
}

void clear_bad_users_list() {
	if (!bad_list)
	return;
	memset((void *) bad_list, 0, sizeof(shm_structure));
}

int remove_bad_users_list() {
	if (sem != SEM_FAILED) sem_close(sem);
	sem_unlink(SHARED_MEMORY_SEM);
	if (bad_list) munmap((void *) bad_list, sizeof(shm_structure));
	close(shm_fd);
	return 0;
}

int is_user_in_list(char *username) {
	if (!bad_list)
	return -1;
	long index;
	for (index = 0; index < bad_list->numbers; index++) {
		if (!strncmp(bad_list->items[index], username, USERNAMEMAXLEN))
		return 1;
	}
	return 0;
}

int add_user_to_list(char *username) {
	if (!bad_list)
	return -1;
	if (!is_user_in_list(username)) {
		if ((bad_list->numbers + 1) == MAX_ITEMS_IN_TABLE)
		return -2;
		if (sem_wait(sem) == 0) {
			strlcpy(bad_list->items[bad_list->numbers++], username,
					USERNAMEMAXLEN);
			sem_post(sem);
		}
	}
	return 0;
}

int delete_user_from_list(char *username) {
	if (!bad_list)
	return -1;
	long index;
	for (index = 0; index < bad_list->numbers; index++) {
		if (!strncmp(bad_list->items[index], username, USERNAMEMAXLEN)) {
			if (sem_wait(sem) == 0) {
				if (index == (bad_list->numbers - 1)) {
					bad_list->numbers--;
					sem_post(sem);
					return 0;
				} else {
					memmove((char *) bad_list->items + USERNAMEMAXLEN
							* sizeof(char) * index, (char *) bad_list->items
							+ USERNAMEMAXLEN * sizeof(char) * (index + 1),
							(bad_list->numbers - index - 1) * USERNAMEMAXLEN
							* sizeof(char));
					bad_list->numbers--;
					sem_post(sem);
					return 0;
				}
				sem_post(sem);
			}
		}
	}
	return -2;
}

long get_users_list_size() {
	if (!bad_list)
	return 0;
	return bad_list->numbers;
}

void printf_bad_users_list() {
	if (!bad_list)
	return;
	long index;
	for (index = 0; index < bad_list->numbers; index++) {
		printf("%ld) user - %s\n", index, bad_list->items[index]);
	}
	return;
}


int is_user_in_bad_list_cleint(char *username) {
	int shm_fd_clents = 0;
	int fnd = 0;
	shm_structure *bad_list_clents;
	if ((shm_fd_clents = shm_open(SHARED_MEMORY_NAME, O_RDONLY, 0755)) < 0) {
		return 0;
	}
	if ((bad_list_clents = (shm_structure *) mmap(0, sizeof(shm_structure),
			PROT_READ, MAP_SHARED, shm_fd_clents, 0)) == MAP_FAILED) {
		close(shm_fd_clents);
		return 0;
	}

	sem_t *sem_client = sem_open(SHARED_MEMORY_SEM, 0, 0777, 1);
	int trys = 1, sem_reopen = 0;

	if (sem_client != SEM_FAILED) {
		while (trys) {
			if (sem_trywait(sem_client) == 0) {
				if (bad_list_clents) {
					long index;
					for (index = 0; index < bad_list_clents->numbers; index++) {
						if (!strncmp(bad_list_clents->items[index], username,
								USERNAMEMAXLEN)) {
							fnd = 1;
							break;
						}
					}
				}
				trys = 0;
			} else {
				if (errno == EAGAIN) {
					trys++;
					if (trys == 100) {
						trys = 1;
						sem_close(sem_client);
						sem_client = sem_open(SHARED_MEMORY_SEM, 0, 0777, 1);
						sem_reopen++;
						if (sem_reopen==4) break;
					}
				} else {
					trys = 0;
				}

			}
		}
		sem_post(sem_client);
		sem_close(sem_client);
	}

	munmap((void *) bad_list_clents, sizeof(shm_structure));
	close(shm_fd_clents);
	return fnd;
}

int user_in_bad_list_cleint_show() {
	int shm_fd_clents = 0;
	int fnd = 0;
	mode_t old_umask = umask(0);
	shm_structure *bad_list_clents;
	if ((shm_fd_clents = shm_open(SHARED_MEMORY_NAME, O_RDONLY, 0755)) < 0) {
		umask(old_umask);
		return 0;
	}
	if ((bad_list_clents = (shm_structure *) mmap(0, sizeof(shm_structure),
			PROT_READ, MAP_SHARED, shm_fd_clents, 0)) == MAP_FAILED) {
		close(shm_fd_clents);
		umask(old_umask);
		return 0;
	}

	sem_t *sem_client = sem_open(SHARED_MEMORY_SEM, 0, 0777, 1);
	umask(old_umask);
	int trys = 1;

	if (sem_client != SEM_FAILED) {
		while (trys) {
			if (sem_trywait(sem_client) == 0) {
				if (bad_list_clents) {
					long index;
					for (index = 0; index < bad_list_clents->numbers; index++) {
						printf("%s\n", bad_list_clents->items[index]);
					}
				}
				trys = 0;
			} else {
				if (errno == EAGAIN) {
					trys++;
					if (trys == 100) {
						trys = 1;
						sem_post(sem_client);
						sem_client = sem_open(SHARED_MEMORY_SEM, 0, 0777, 1);
					}
				} else {
					trys = 0;
				}

			}
		}
		sem_post(sem_client);
		sem_close(sem_client);
	}

	munmap((void *) bad_list_clents, sizeof(shm_structure));
	close(shm_fd_clents);
	return fnd;
}

int shm_fd_clents_global = 0;
shm_structure *bad_list_clents_global = NULL;
pthread_mutex_t mtx_shared = PTHREAD_MUTEX_INITIALIZER;

int init_bad_users_list_client() {
	pthread_mutex_lock(&mtx_shared);
	if ((shm_fd_clents_global = shm_open(SHARED_MEMORY_NAME, O_RDONLY, 0755))
			< 0) {
		pthread_mutex_unlock(&mtx_shared);
		return -1;
	}
	if ((bad_list_clents_global = (shm_structure *) mmap(0,
			sizeof(shm_structure), PROT_READ, MAP_SHARED, shm_fd_clents_global,
			0)) == MAP_FAILED) {
		close(shm_fd_clents_global);
		pthread_mutex_unlock(&mtx_shared);
		return -2;
	}
	pthread_mutex_unlock(&mtx_shared);

	return 0;
}

int remove_bad_users_list_client() {
	pthread_mutex_lock(&mtx_shared);
	if (bad_list_clents_global)
		munmap((void *) bad_list_clents_global, sizeof(shm_structure));
	close(shm_fd_clents_global);
	pthread_mutex_unlock(&mtx_shared);
	return 0;
}

int is_user_in_bad_list_cleint_persistent(char *username) {
	sem_t *sem_client = sem_open(SHARED_MEMORY_SEM, 0, 0777, 1);
	int trys = 1, sem_reopen = 0;
	int fnd = 0;

	if (sem_client != SEM_FAILED) {
		while (trys) {
			if (sem_trywait(sem_client) == 0) {
				if (bad_list_clents_global) {
					long index = 0;
					//pthread_mutex_lock(&mtx_shared);
					for (index = 0; index < bad_list_clents_global->numbers; index++) {
						if (!strncmp(bad_list_clents_global->items[index],
								username, USERNAMEMAXLEN)) {
							fnd = 1;
							break;
						}
					}
					//pthread_mutex_unlock(&mtx_shared);
				}
				trys = 0;
			} else {
				if (errno == EAGAIN) {
					trys++;
					if (trys == 100) {
						trys = 1;
						sem_close(sem_client);
						sem_client = sem_open(SHARED_MEMORY_SEM, 0, 0777, 1);
						sem_reopen++;
						if (sem_reopen==4) break;
					}
				} else {
					trys = 0;
				}

			}
		}
		sem_post(sem_client);
		sem_close(sem_client);
	}

	return fnd;
}
