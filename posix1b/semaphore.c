/*-< SEMAPHORE.C >--------------------------------------------------*--------*/
/* POSIX.1b                   Version 1.0        (c) 1998  GARRET   *     ?  */
/* (POSIX.1b implementation for Linux)                              *   /\|  */
/*                                                                  *  /  \  */
/*                          Created:     25-Aug-98    K.A. Knizhnik * / [] \ */
/*                          Last update: 27-Aug-98    K.A. Knizhnik * GARRET */
/*------------------------------------------------------------------*--------*/
/* Semaphore implementation                                         *        */
/*------------------------------------------------------------------*--------*/

#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "semaphore.h"

#if (defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)) || defined(__FreeBSD__) 
/* union semun is defined by including <sys/sem.h> */  
#else
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};
#endif

#define HASH_TABLE_SIZE 253

typedef struct hash_item { 
    struct hash_item* next;
    long              semkey;
    int               semid;
} hash_item;

static hash_item* hash_table[HASH_TABLE_SIZE];

#ifdef _REENTRANT
#include <pthread.h>
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define lock()   pthread_mutex_lock(&mutex)
#define unlock() pthread_mutex_unlock(&mutex)
#else
#define lock()
#define unlock()
#endif

static void insert_semid(long key, int semid)
{
    hash_item* ip = (hash_item*)malloc(sizeof(hash_item));
    ip->semkey = key;
    ip->semid = semid;
    key %= HASH_TABLE_SIZE;
    ip->next = hash_table[key];
    hash_table[key] = ip;
}

static int get_semid(sem_t* sem) 
{
    hash_item* ip;
    long key;
    int semid;
    if (sem->initialized) { 
	return sem->semid;
    }
    key = sem->semkey;
    lock();
    for (ip=hash_table[key % HASH_TABLE_SIZE]; ip != NULL; ip=ip->next) {
	if (ip->semkey == key) { 
	    unlock();
	    return ip->semid;
	}
    }
    semid = semget(key, 1, 0);
    if (semid < 0) { 
	return -1;
    }
    insert_semid(key, semid);
    unlock();
    return -1;
}

static void remove_semid(long key)
{
    hash_item** ip;
    lock();
    for (ip=&hash_table[key % HASH_TABLE_SIZE]; *ip != NULL; ip=&(*ip)->next){
	if ((*ip)->semkey == key) { 
	    *ip = (*ip)->next;
	    free(*ip);
	    break;
	}
    }
    unlock();
}

sem_t *sem_open(const char *name, int oflag, ...)
{
    key_t key = IPC_PRIVATE;
    int rc, n_sops, semid, mode = 0i, init_value = 0;
    struct sembuf sops[4];
    sem_t* s;

    if (name != NULL) { 
        int fd = open(name, O_WRONLY|O_CREAT, 0777);
	if (fd < 0) {
	    return (sem_t*)-1;
	}
	close(fd);
	key = ftok(name, 0);
	if (key < 0) {
	    return (sem_t*)-1;
	}
    }
    n_sops = 3;
    if (oflag & O_CREAT) { 
	va_list ap;
	va_start(ap, oflag);
	mode = va_arg(ap, mode_t) | IPC_CREAT;
	init_value = va_arg(ap, unsigned int); 
	if (init_value < 0) { 
	    errno = EINVAL;
	    return (sem_t*)-1;
	}
	va_end(ap);
    }
    if (oflag & O_EXCL) {
	mode |= IPC_EXCL;
    } 
    semid = semget(key, 3, mode);
    if (semid < 0) {
        return (sem_t*)-1;
    }
    do { 
	if (n_sops == 3 && (oflag & O_CREAT)) { 
	    sops[0].sem_num = 1;
	    sops[0].sem_op  = 0; /* check if not yet initialized */
	    sops[0].sem_flg = IPC_NOWAIT;
	    sops[1].sem_num = 1;
	    sops[1].sem_op  = 1; /* mark sempahore as initialized */
	    sops[1].sem_flg = 0;
	    sops[2].sem_num = 0;
	    sops[2].sem_op  = init_value;
	    sops[2].sem_flg = 0;
	    sops[3].sem_num = 2; /* access counter */
	    sops[3].sem_op  = 1;
	    sops[3].sem_flg = SEM_UNDO;
	    n_sops = 4;
        } else { 
	    sops[0].sem_num = 1;
	    sops[0].sem_op  = -1; /* wait until semaphore is initialized */
	    sops[0].sem_flg = (oflag & O_CREAT) ? IPC_NOWAIT : 0;
	    sops[1].sem_num = 1;
	    sops[1].sem_op  = 1; /* restore initialized flag */
	    sops[1].sem_flg = 0;
	    sops[2].sem_num = 2; /* access counter */
	    sops[2].sem_op  = 1;
	    sops[2].sem_flg = SEM_UNDO;
	    n_sops = 3;
	}
    } while ((rc = semop(semid, sops, n_sops)) != 0 && errno == EAGAIN);

    if (rc != 0) { 
	return (sem_t*)-1;
    }
    s = (sem_t*)malloc(sizeof(sem_t));
    s->semid = semid;
    s->initialized = 1;
    s->semkey = key;
    return s;
}
    
int sem_init(sem_t *sem, int pshared, unsigned int value)
{
    int semid;
    if (value < 0) { 
	errno = EINVAL;
	return -1;
    }
    sem->semkey = pshared ? (long)sem : IPC_PRIVATE;
    sem->initialized = 0;
    semid = semget(sem->semkey, 1, IPC_CREAT|0777);
    if (semid < 0) { 
	return -1;
    }
    if (value != 0) { 
	struct sembuf sops[1];
	sops[0].sem_num = 0;
	sops[0].sem_op  = value;
	sops[0].sem_flg = 0;
	if (semop(semid, sops, 1) != 0) { 
	    return -1;
	}
    }
    lock();
    insert_semid(sem->semkey, semid); 
    unlock();
    return 0;
}


int sem_post(sem_t *sem)
{
    static struct sembuf sops[] = {{0, 1, 0}};
    return semop(get_semid(sem), sops, 1);
}

int sem_getvalue(sem_t *sem, int *sval)
{
    int result;
    if (sval == NULL) { 
	errno = EINVAL;
	return -1;
    }
    result = semctl(get_semid(sem), 0, GETVAL, (union semun)0);
    if (result == -1) { 
	return -1;
    }
    *sval = result;
    return 0;
}

int sem_wait(sem_t *sem)
{
    static struct sembuf sops[] = {{0, -1, 0}};
    return semop(get_semid(sem), sops, 1);
}

int sem_trywait(sem_t *sem)
{
    static struct sembuf sops[] = {{0, -1, IPC_NOWAIT}};
    return semop(get_semid(sem), sops, 1);
}


int sem_unlink(const char *name)
{
    struct sembuf sops[1];
    int key, semid;
    key = ftok(name, 0);
    if (key < 0) {
	return -1;
    }
    semid = semget(key, 1, 0);
    if (semid < 0) { 
	return -1;
    }
    unlink(name);
    sops[0].sem_num = 1;
    sops[0].sem_op  = -1; /* mark as deleted */
    sops[0].sem_flg = 0;    
    semop(semid, sops, 1);

    sops[0].sem_num = 2;
    sops[0].sem_op  = 0; /* check id access counter is 0 */
    sops[0].sem_flg = IPC_NOWAIT;    
    if (semop(semid, sops, 1) == 0) {
	semctl(semid, 0, IPC_RMID, (union semun)0);
    }
    return 0;
}

int sem_close(sem_t *sem)
{
    struct sembuf sops[2];
    int semid = get_semid(sem);

    sops[0].sem_num = 2;
    sops[0].sem_op  = -1; /* decrement access counter */
    sops[0].sem_flg = SEM_UNDO;
    semop(semid, sops, 1);

    sops[0].sem_num = 2;
    sops[0].sem_op  = 0; /* check if access counter is 0 */
    sops[0].sem_flg = IPC_NOWAIT;
    sops[1].sem_num = 1;
    sops[1].sem_op  = 0; /* check if semaphore was destroyed */
    sops[1].sem_flg = IPC_NOWAIT;
    if (semop(semid, sops, 2) == 0) {
	 semctl(semid, 0, IPC_RMID, (union semun)0);
    }
    free(sem);
    return 0;
}

int sem_destroy(sem_t* sem) 
{
    int semid = get_semid(sem);
    remove_semid(sem->semkey);
    return semctl(semid, 0, IPC_RMID, (union semun)0);
}
