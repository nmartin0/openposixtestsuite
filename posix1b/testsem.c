#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include "semaphore.h"

#define SHARED_SECTION "/tmp/shared"
#define SEM_NAME       "/tmp/semaphore"

typedef struct shared { 
    sem_t sem;
    int   count[4];
} shared;

int main()
{
    int fd;
    sem_t *msem, *sem;
    shared* s;
    int pid, i;

    sem = sem_open(SEM_NAME, O_CREAT, 0777, 0);
    if (sem == (sem_t*)-1) { 
	perror("sem_open");
	return 1;
    }
    pid = fork();
    fd = open(SHARED_SECTION, O_CREAT|O_RDWR, 0777);
    ftruncate(fd, sizeof(shared));
    s = (shared*)mmap(NULL, sizeof(shared), PROT_READ|PROT_WRITE, MAP_SHARED, 
		      fd, 0);
    msem = &s->sem;
    if (pid != 0) { 
	if (sem_init(msem, 1, 1) != 0) { 
	    perror("sem_init");
	}
	sem_post(sem);
    } else { 
	sem_wait(sem);
	sem_post(sem);
    }
	
    for (i = 0; i < 10000; i++) { 
	int count;
	sem_wait(sem);
	count = s->count[0];
	if (sem_trywait(msem) == 0) { 
	    s->count[1] += 1;
	    sem_post(msem);
	}
	s->count[0] = count + 1;
	sem_post(sem);

	sem_wait(msem);
	count = s->count[2];
	if (sem_trywait(sem) == 0) { 
	    s->count[3] += 1;
	    sem_post(sem);
	}
	s->count[2] = count + 1;
	sem_post(msem);
    }
    if (pid != 0) { 
	int status;
	wait(&status);
	printf("count[0]=%d, count[1]=%d, count[2]=%d, count[3]=%d\n", 
	       s->count[0], s->count[1], s->count[2], s->count[3]);
	sem_close(sem);
	sem_unlink(SEM_NAME);
	sem_destroy(msem);
	munmap((void*)s, sizeof(shared));
	unlink(SHARED_SECTION);
    }  
    return 0;
}


