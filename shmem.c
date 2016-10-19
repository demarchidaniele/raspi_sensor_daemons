#include<stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int shmem_server_init(key_t key, size_t size, void** ptr)
{
	int shmid;
	/*
	* Create the segment.
	*/
	if ((shmid = shmget(key, size, IPC_CREAT | 0666)) < 0) {
	perror("shmget");
	exit(1);
	}

	/*
	* Now we attach the segment to our data space.
	*/
	if ((*ptr = shmat(shmid, NULL, 0)) == (char *) -1) {
	perror("shmat");
	exit(1);
	}

	return shmid;
}

int shmem_detach(void** ptr, int id)
{
		//Detach and release shared memory of deameon
		shmdt(*ptr);
		shmctl(id, IPC_RMID, NULL);
}

void shmem_client_init(key_t key, size_t size, void** ptr)
{
	int shmid;
	char *shm, *s;

	/*
	* Locate the segment.
	*/
	if ((shmid = shmget(key, size, 0666)) < 0) {
	perror("shmget");
	exit(1);
	}

	/*
	* Now we attach the segment to our data space.
	*/
	if ((*ptr = shmat(shmid, NULL, 0)) == (char *) -1) {
	perror("shmat");
	exit(1);
	}
}
