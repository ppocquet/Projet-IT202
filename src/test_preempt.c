#include "thread.h"
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>

static void *threadfunc(void * arg)
{
    int i;
    for(i=0;i<100000;i++){
    }
    thread_exit(arg);
}

void print_date(int i){
    (void) i;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    printf("%d sec %d ms\n", tv.tv_sec, tv.tv_usec);
}

int main(int argc, char *argv[])
{
    thread_t thread1, thread2;
    void *retval1, *retval2;
    int err;

    printf("le main lance 2 threads...\n");
    err = thread_create(&thread1, (void * (*) (void *)threadfunc, "thread1");
    thread_signal(thread1,SIG_STOP,print_date);
    assert(!err);
    err = thread_create(&thread2, (void * (*) (void *)threadfunc, "thread2");
    thread_signal(thread2,SIG_STOP,print_date);
    assert(!err);
    printf("le main a lancé les threads %p et %p\n",
	   thread1, thread2);

    printf("le main attend les threads\n");
    err = thread_join(thread2, &retval2);
    assert(!err);
    err = thread_join(thread1, &retval1);
    assert(!err);
    printf("les threads ont terminé en renvoyant '%s' et '%s'\n",
	   (char *) retval1, (char *) retval2);

    return 0;
}
