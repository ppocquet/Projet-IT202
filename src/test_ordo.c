#include "thread.h"
#include <stdio.h>
#include <assert.h>


static void * threadfunc(void * arg)
{
    
  char *name = arg;
  int i;
  for(i=1; i<11; i++) {
      fprintf(stderr, "je suis le thread %p, lancé avec l'argument %s | %d fois\n",
	      thread_self(), name, i);
      thread_yield();
  }
  thread_exit(arg);
}

int main(int argc, char *argv[])
{
    thread_t thread1, thread2, thread3;
    void *retval1, *retval2, *retval3;
    int err;
    
    printf("le main lance 3 threads...\n");
    err = thread_create_with_prio(&thread1, threadfunc, "thread1", -5);
    assert(!err);
    err = thread_create_with_prio(&thread2, threadfunc, "thread2", 0);
    assert(!err);
    err = thread_create_with_prio(&thread3, threadfunc, "thread3", 5);
    assert(!err);
    printf("le main a lancé les threads %p, %p et %p\n",
	   thread1, thread2, thread3);
    
    printf("le main attend les threads\n");
    err = thread_join(thread3, &retval3);
    assert(!err);
    err = thread_join(thread2, &retval2);
    assert(!err);
    err = thread_join(thread1, &retval1);
    assert(!err);
    printf("les threads ont terminé en renvoyant '%s', '%s' et '%s'\n",
	   (char *) retval1, (char *) retval2, (char *) retval3);

  return 0;
}
