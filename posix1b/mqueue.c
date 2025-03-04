/*-< MQUEUE.C >-----------------------------------------------------*--------*/
/* POSIX.1b                   Version 1.0        (c) 1998  GARRET   *     ?  */
/* (POSIX.1b implementation for Linux)                              *   /\|  */
/*                                                                  *  /  \  */
/*                          Created:     25-Aug-98    K.A. Knizhnik * / [] \ */
/*                          Last update: 27-Aug-98    K.A. Knizhnik * GARRET */
/*------------------------------------------------------------------*--------*/
/* Message queue implementation                                     *        */
/*------------------------------------------------------------------*--------*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <alloca.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "mqueue.h"

#define IMPLEMENT_MQ_NOTIFY 1 // support asynchronouse message receiving

#ifdef DEBUG
#define dprint(x) printf(x)
#else
#define dprint(x)
#endif

struct msgbuf {
    long mtype;     /* message type, must be > 0 */
    char mtext[1];  /* message data */
};

mqd_t mq_open(const char *name, int oflag, ...)
{
    key_t key = IPC_PRIVATE;
    va_list ap;
    struct mq_attr* attr = NULL;
    int msgid, fd = -1;
    unsigned long mode = 0, msgflg = 0;
    mqd_t q;

    va_start(ap, oflag);
    mode = va_arg(ap, mode_t); /* | IPC_CREAT; */
    attr = (attr = va_arg(ap, struct mq_attr *)) ? attr : NULL ;
    va_end(ap);

    if (oflag & O_CREAT) {
        dprint("mq_open: O_CREAT get arguments\n");

	if (!(oflag & O_EXCL)) {
            dprint("mq_open: non-exclusive open\n");
            fd = open(name, oflag & ~O_CREAT);
        }

        if (attr != NULL) {
            dprint("mq_open: test attr\n");
            if (attr->mq_maxmsg <= 0) {
                dprint("mq_open: mq_maxmsg <= 0");
                errno = EINVAL;
                goto err;
            }
            if (attr->mq_msgsize <= 0) {
                dprint("mq_open: mq_msgsize <= 0");
                errno = EINVAL;
                goto err;
            }
            if (attr->mq_maxmsg > MQ_MAXMSG) {
                dprint("mq_open: mq_maxmsg > MQ_MAXMSG");
                errno = EINVAL;
                goto err;
            }
            if (attr->mq_msgsize > MQ_MSGSIZE) {
                dprint("mq_open: mq_msgsize > MQ_MSGSIZE");
                errno = EINVAL;
                goto err;
            }
        }

        if (fd < 0) {
            if ((fd = open(name, oflag, mode)) < 0) {
                goto err;
            }
            dprint("mq_open: open using oflag\n");
        }
    } else {
        if ((fd = open(name, oflag)) < 0) {
            goto err;
        }
        dprint("mq_open: open no-CREAT\n");
    }

    if ((key = ftok(name, 'a')) < 0) {
        goto err;
    }
    dprint("mq_open: ftok passed\n");

    msgflg = (0x1FF & mode);
    if (oflag & (O_CREAT | O_EXCL))
        msgflg |= IPC_CREAT;

    if ((msgid = msgget(key, msgflg)) < 0) {
        goto err;
    }
    dprint("mq_open: msgget passed\n");

    if ((q = (mqd_t)malloc(sizeof(struct mq_descriptor))) == 0) {
        errno = ENOSPC;
        goto err;
    }

    q->msgid = msgid;
    q->fd = fd;
    q->flags = (attr ? attr->mq_flags : 0);
    dprint("mq_open: mqd_t has been initialized\n");

#if IMPLEMENT_MQ_NOTIFY
    ftruncate(fd, sizeof(struct mq_notification_request));
    dprint("mq_open: mmap notification request\n");
    q->req = (struct mq_notification_request*)
	mmap(NULL, sizeof(struct mq_notification_request),
	     PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    if (q->req == (struct mq_notification_request*)-1) { 
	q->req = NULL;
    } else { 
	q->req->pid = 0;
    } 
#else 
    close(fd);
    q->fd = -1;
    q->req = NULL;
#endif		
    if (attr != NULL) { 
	mq_setattr(q, attr, NULL);
    }
    dprint("mq_open: success\n");
    return q;

err:
    perror("mq_open");
    if (fd >= 0) {
        if (unlink(name) < 0)
            perror("mq_open: unlink error");
        if (close(fd) < 0)
            perror("mq_open: close error");
    }
    return (mqd_t)-1;
}  
    
int mq_close(mqd_t mqdes)
{
    if (mqdes->req != NULL) { 
	munmap((void*)mqdes->req, sizeof(struct mq_notification_request));
    }
    if (mqdes->fd >= 0) { 
	close(mqdes->fd);
    }
    free(mqdes);
    return 0;
}

int mq_unlink(const char *name)
{
    int key, msgid;
    key = ftok(name, 0);
    if (key < 0) {
	return -1;
    }
    msgid = msgget(key, 0);
    if (msgid < 0) { 
	return -1;
    }
    unlink(name);
    return msgctl(msgid, IPC_RMID, 0);    
}

int mq_notify(mqd_t mqdes, const struct sigevent *notification)
{
    if (mqdes->req == NULL) { 
	errno = ENOENT;
	return -1;
    } 
    if (notification == NULL || notification->sigev_notify == SIGEV_NONE) { 
	mqdes->req->pid = 0;
    } else if (notification->sigev_notify == SIGEV_SIGNAL) { 
	if (mqdes->req->pid != 0) { 
	    errno = EBUSY;
	    return -1;
	}
	mqdes->req->pid = getpid();
	mqdes->req->signo = notification->sigev_signo;
    } else { 
	errno = EINVAL;
	return -1;
    }
    return 0;
}



int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len, 
	    unsigned int msg_prio)
{
    struct msgbuf* buf;
    int rc;
    if (msg_prio > MQ_PRIO_MAX) { 
	errno = EINVAL;
	return -1;
    }
    if (msg_len > MQ_MSGSIZE) {
        errno = E2BIG;
        return -1;
    }

    buf = (struct msgbuf*)alloca(msg_len + sizeof(long));
    buf->mtype = MQ_PRIO_MAX - msg_prio;
    memcpy(buf->mtext, msg_ptr, msg_len);
    rc = msgsnd(mqdes->msgid, buf, msg_len, mqdes->flags);
    if (rc == 0 && mqdes->req != NULL && mqdes->req->pid != 0) { 
	kill(mqdes->req->pid, mqdes->req->signo);
	mqdes->req->pid = 0;
    }
    return 0;
}


ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len, 
		   unsigned int *msg_prio)
{
    struct msgbuf* buf = (struct msgbuf*)alloca(msg_len + sizeof(long));
    ssize_t rc = msgrcv(mqdes->msgid, buf, msg_len,-MQ_PRIO_MAX, mqdes->flags);
    if (msg_prio != NULL && rc >= 0) { 
	*msg_prio = MQ_PRIO_MAX - buf->mtype;
    }
    if (rc > 0) { 
	memcpy(msg_ptr, buf->mtext, rc);
    }
    return rc;
}


int mq_getattr(mqd_t mqdes, struct mq_attr *mqstat)
{
    struct msqid_ds buf;
    if (mqstat == NULL) { 
	errno = EINVAL;
	return -1;
    }
    if (msgctl(mqdes->msgid, IPC_STAT, &buf) < 0) { 
	return -1;
    }
    mqstat->mq_maxmsg = MQ_MAX;	    /* max # of messages allowed in MQ */
    mqstat->mq_msgsize = MQ_MSGSIZE;     /* max size of any one message */
    mqstat->mq_flags = (mqdes->flags & IPC_NOWAIT) ? O_NONBLOCK : 0; 
    mqstat->mq_curmsgs = buf.msg_qnum; /* # of messages currently on mq */
    return 0;
}


int mq_setattr(mqd_t mqdes, const struct mq_attr *mqstat,
	       struct mq_attr *omqstat)
{
    long oflags = mqdes->flags;
    if (mqstat != NULL) { 
        dprint("mq_setattr: set flags\n");
	mqdes->flags = (mqstat->mq_flags & O_NONBLOCK) ? IPC_NOWAIT : 0;
    } 
    if (omqstat != NULL) { 
	struct msqid_ds buf;
	if (msgctl(mqdes->msgid, IPC_STAT, &buf) < 0) { 
            dprint("mq_setattr: msgctl failed\n");
	    return -1;
	}
        dprint("mq_setattr: set omqstat fields\n");
	omqstat->mq_maxmsg = MQ_MAX;  
	omqstat->mq_msgsize = MQ_MSGSIZE; 
	omqstat->mq_flags = (oflags & IPC_NOWAIT) ? O_NONBLOCK : 0; 
	omqstat->mq_curmsgs = buf.msg_qnum; 
    }
    return 0;
}
