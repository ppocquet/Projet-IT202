#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
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
    char *param = (char *)p;

    char *argv[2];
    argv[1] = NULL;
    argv[0] = malloc(15 * sizeof *argv[0]);
    sprintf(argv[0],"%d",input);

    if(execv(param,
	     argv)==-1){
	perror(NULL);
	exit(0);
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

    sigaction(SIGVTALRM,&act,NULL);

    struct itimerval new_value;

    new_value.it_interval.tv_sec = TIMESLICE;
    new_value.it_interval.tv_usec = 0;

    new_value.it_value = new_value.it_interval;

    char *param = strdup(argv[2]);

    long old = 0;
    while((funccall_retval == NULL || *funccall_retval != PTHREAD_CANCELED)) {
	long new;
	int lequel = RUSAGE_SELF;
	struct rusage statistiques;

	setitimer(ITIMER_VIRTUAL,
		  &new_value,
		  NULL);


	pthread_create(&current,
		       NULL,
		       funccall,
		       param);

	pthread_join(current, funccall_retval);


	getrusage (lequel, &statistiques);

	new = (statistiques.ru_stime.tv_sec * 1000000 +
	       statistiques.ru_stime.tv_usec) +
	    (statistiques.ru_utime.tv_sec * 1000000 +
	     statistiques.ru_utime.tv_usec);

	new_value.it_value.tv_sec = 0;
	new_value.it_value.tv_usec = 0;

	fprintf(output,
		"%ld ",
		new - old);

	old = new;

	printf("%d\n",input);
	input ++;

    }

    free(param);

    return EXIT_SUCCESS;
}
