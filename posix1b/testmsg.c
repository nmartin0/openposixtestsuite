#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "mqueue.h"

mqd_t mqdes;

void msg_handler()
{
    char buf[256];
    unsigned int priority;
    int len = mq_receive(mqdes, buf, sizeof buf, &priority);
    if (len < 0) { 
	perror("mq_receive");
    }
    printf("\nReceive message with priority %d: %s\n", priority, buf); 
}

#define QUEUE_NAME "/tmp/testmsg"

int main()
{
    char buf[256];
    int choice, len;
    unsigned int priority;
    struct sigevent notification;
    static struct sigaction sa;

    mqdes = mq_open(QUEUE_NAME, O_CREAT, 0777, NULL);
    if (mqdes == (mqd_t)-1) {
	perror("mq_open");
	return 1;
    }
    
    notification.sigev_notify = SIGEV_SIGNAL;
    notification.sigev_signo = SIGUSR1;

    sa.sa_handler = msg_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    
    while (1) { 
	printf("\t1. Send\n"
	       "\t2. Asynchronouse receive\n"	       
	       "\t3. Synchronouse receive\n"
	       "\t4. Close queue and exit\n"
	       "\t5. Remove queue and exit\n"
	       "> ");
	if (!fgets(buf, sizeof buf, stdin)) { 
	    return 1;
	}
	if (sscanf(buf, "%d", &choice) != 1) { 
	    printf("Please select 1..5\n");
	    continue;
	}
	switch (choice) { 
	  case 1:
	    do { 
		printf("Message priority: ");
		fgets(buf, sizeof buf, stdin);
	    } while (sscanf(buf, "%d", &priority) != 1);
	    printf("Message to send: ");
	    fgets(buf, sizeof buf, stdin);
	    if (mq_send(mqdes, buf, strlen(buf)+1, priority) != 0) { 
		perror("mq_send");
		return 1;
	    }
	    break;
	  case 2:
	    mq_notify(mqdes, &notification);
	    printf("Waiting for notifications...\n");
	    break;
	  case 3:
	    len = mq_receive(mqdes, buf, sizeof buf, &priority);
	    if (len < 0) { 
		perror("mq_receive");
	    }
	    printf("Receie message with priority %d: %s\n", priority, buf); 
	    break;
	  case 4:
	    mq_close(mqdes);
	    return 0;
	  case 5:
	    mq_close(mqdes);
	    mq_unlink(QUEUE_NAME);
	    return 0;;
	  default:
	    printf("Please select 1..5\n");
	}
    }
}






