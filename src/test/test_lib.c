#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>



#define TIMESLICE 30

static pthread_t current;
static int input = 1;

void
sigvtalarm_treatment(int i) {
    (void) i;
    pthread_cancel(current);
}

void *
funccall(void *p) {

    pid_t id = fork();
    char *param = (char *)p;

    char *argv[3];

    switch (id) {
    case 0:
	argv[2] = NULL;
	argv[1] = malloc(15 * sizeof *argv[0]);
	sprintf(argv[1],"%d",input);
	argv[0] = strdup(param);

	if(execv(param,
		 argv)==-1){
	    perror(NULL);
	    exit(0);
	}
	break;
    case -1:
	exit(1);
    default:
	waitpid(id,NULL,0);
    }

    return NULL;
}

int
main(int argc, char **argv) {
    if (argc != 3){
	printf("usage :\n%s filename progam\n",argv[0]);
	return EXIT_FAILURE;
    }

    FILE *output = fopen(argv[1],"w+");

    void **funccall_retval=NULL;

    struct sigaction act;

    act.sa_handler = sigvtalarm_treatment;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;

    sigaction(SIGVTALRM,&act,NULL);

    struct itimerval new_value;

    new_value.it_interval.tv_sec = TIMESLICE;
    new_value.it_interval.tv_usec = 0;

    new_value.it_value = new_value.it_interval;

    char *param = strdup(argv[2]);

    while((funccall_retval == NULL || *funccall_retval != PTHREAD_CANCELED)) {
	long t;
	
	setitimer(ITIMER_VIRTUAL,
		  &new_value,
		  NULL);

	pthread_attr_t attr;
	pthread_attr_init(&attr);

	unsigned long s;
	unsigned long us;
    
	struct timeval tv1;
	struct timeval tv2;
	gettimeofday(&tv1, NULL);
	
	
    
		printf("%lus %luus\n ",s, us);

	pthread_create(&current,
		       &attr,
		       funccall,
		       param);

	pthread_join(current, funccall_retval);
	gettimeofday(&tv2, NULL);

	pthread_attr_destroy(&attr);

	s=tv2.tv_sec - tv1.tv_sec;
	us=tv2.tv_usec - tv1.tv_usec;

	t = s*1000000 + us;
	
	new_value.it_value.tv_sec = 0;
	new_value.it_value.tv_usec = 0;
	

	fprintf(output,
		"%d\t%ld\n",
		input, t);
	fflush(output);
	
	input ++;

    }

    free(param);
    fclose(output);
    return EXIT_SUCCESS;
}
