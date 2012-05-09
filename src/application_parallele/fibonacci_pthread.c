#include "../thread.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>

int i = 0;

int fibonacci(int n){
    if (n == 1 || n == 2){
	pthread_exit(1);
	return 1;
    }

    else{
	pthread_t thread1, thread2;
	void * retval1, *retval2;
	int err;
	int a = n-1;
	int b = n-2;
	err = pthread_create(&thread1, NULL, (void * (*) (void *))fibonacci, (void *) a);
	assert(!err);
	i++;
	err = pthread_create(&thread2, NULL, (void * (*) (void *))fibonacci, (void *) b);
	assert(!err);
	err = pthread_join(thread2, &retval2);
	assert(!err);
	err = pthread_join(thread1, &retval1);
	assert(!err);
	int c = (int) retval1;
	int d = (int) retval2;
	pthread_exit(c+d);
	return c+d;
    }
}

int main(int argc, char **argv){
    pthread_t threadfibo;
    void * retval;
    int err;
    int n = atoi(argv[1]);
    err = pthread_create(&threadfibo, NULL, (void * (*) (void *))fibonacci,(void*) n);
    assert(!err);
    err = pthread_join(threadfibo, &retval);
    printf("fibonacci(%d)=%d\n",n,(int)retval);
    return 0;
}
