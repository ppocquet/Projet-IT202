#include<stdlib.h>
#include<stdio.h>

#include"../thread.h"

thread_t thr1;
thread_t thr2;

int run1(int val){
  int i;

  for(i=1;i<=5;i++){
    printf("1, %d\n", i);
    thread_yield();
  }
  
  printf("attaque seau de colle!!\n");
  thread_kill(thr2, SIG_STOP);

  for(i=6;i<=10;i++){
    printf("1, %d\n", i);
    thread_yield();
  }

  thread_kill(thr2, SIG_WAKE);

  thread_exit(val);
}


int run2(int val){
  int i;
  for(i=1;i<=10;i++){
    printf("2, %d\n", i);
    thread_yield();
  }
  thread_exit(val);
}


int main(int argc, char **argv){
    void * retval1;
    void * retval2;
    int err;

    err = thread_create(&thr1, (void * (*) (void *))run1, (void*) 1);
    err = thread_create(&thr2, (void * (*) (void *))run2, (void*) 2);

    err = thread_join(thr1, &retval1);
    err = thread_join(thr2, &retval2);

    return 0;
}
