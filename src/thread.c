#include "thread.h"
#include <glib.h>
#include <stdlib.h>
#include <ucontext.h>
#include <valgrind/valgrind.h>
#include <sys/time.h>
#include <signal.h>
#include <stdio.h>


GList * ready_list = NULL;
GList * ready_list_end = NULL;
GList * zombie_list = NULL;

#define DEFAULT_PRIO 0
#define MIN_PRIO -20
#define MAX_PRIO 19

#define TIMESLICE 10

/**
 * Par defaut on a un ordonnancement FIFO
 */
#define ADD_THREAD append

#if ORDO_PRIO==1
    #define ADD_THREAD prio_update_sorted_insert_by_end
#endif

typedef void(*treat_func)(int);

//initial treatment signal function
void basic_sig_treatment(int sig){
    if(sig<0 || sig>NB_SIG)
	return;

    switch(sig){
    case SIG_KILL :
	thread_exit(NULL);
	break;
    case SIG_YIELD :
	thread_yield();
	break;
    default:
	printf("signal %d recu\n", sig);
    }
}


/*list for stopped thread*/
GList * stopped_list = NULL;

struct thread {
    ucontext_t uc;
    GList * sleeping_list;
    void *retval;
    int basic_prio; /* MIN_PRIO à MAX_PRIO */
    int current_prio;

    treat_func treat_tab[NB_SIG];
    GList* sig_list;

    /*valgrind stackid*/
    int stackid;
};

/**
 * une insertion triée à partir de la fin de la liste ready sur current_prio  et
 * lors de l'insertion on augmente(--)le current_prio des threads qu'on a dépassé.
 * /!\ l'insertion de doit pas dépasser le 1er élement qui est le thread courant.
 */
int prio_update_sorted_insert_by_end(thread_t thread) {
    if(thread == NULL)
	return -1;

    if(g_list_length(ready_list) == 0) {
	ready_list = g_list_append(ready_list, thread);
	ready_list_end = ready_list;
	return 0;
    }

    GList *current = ready_list_end;
    while(current != ready_list) {
	thread_t current_thread = current->data;
	if(thread->current_prio < current_thread->current_prio) {
	    current_thread->current_prio--;
	    current = g_list_previous(current);
	}
	else {
	    // ajout à la fin de la liste
	    if(current == ready_list_end) {
		GList *end_prev = g_list_previous(ready_list_end);
		ready_list_end = g_list_append(ready_list_end, thread);

		end_prev->next = ready_list_end;
		ready_list_end = ready_list_end->next;
	    }
	    else {
		GList *next = g_list_next(current);
		ready_list = g_list_insert_before(ready_list, next, thread);
	    }
	    break;
	}
    }

    if(current == ready_list) {
	//le thread courant est le seul dans la liste
	if(ready_list->next == NULL) {
	    ready_list = g_list_append(ready_list, thread);
	    ready_list_end = g_list_next(ready_list);
	}
	else
	    ready_list = g_list_insert_before(ready_list, g_list_next(ready_list), thread);
    }

    return 0;
}

/**
 * Cette ajoute un thread_t à la fin de la liste ready_list
 * sans parcourir toute la liste.
 * Uililisé pour la politique d'ordonnancement FIFO
 */
void append(thread_t thread) {
    if(ready_list == ready_list_end || ready_list == NULL ) {
	ready_list = g_list_append(ready_list, thread);
	ready_list_end = g_list_next(ready_list);
	//ready_list etait NULL -> maintenant un seul elt dans la liste
	if(ready_list_end == NULL)
	    ready_list_end = ready_list;
    }
    else {
	GList *end_prev = g_list_previous(ready_list_end);
	ready_list_end = g_list_append(ready_list_end, thread);

	end_prev->next = ready_list_end;
	ready_list_end = ready_list_end->next;
    }
}

thread_t thread_self(void) {
    return (thread_t)g_list_nth_data(ready_list, 0);
}


void sigvtalarm_treatment(int i){
    sig_block();
    (void)i;
    thread_t current = g_list_nth_data(ready_list, 0);
    
        struct itimerval new_value;

    getitimer(ITIMER_VIRTUAL,
	      &new_value);
    fprintf(stderr,"%d %d\n",new_value.it_interval.tv_usec, new_value.it_value.tv_usec);

    fprintf(stderr,"current %p  1%p  e%p\n", current, ready_list->data, ready_list_end->data);////
    thread_kill(current,SIG_YIELD);
    fprintf(stderr,"2current %p  1%p  e%p\n", current, ready_list->data, ready_list_end->data);////
    sig_unblock();
    thread_sigTreat(current);
    fprintf(stderr,"3current %p  1%p  e%p\n", current, ready_list->data, ready_list_end->data);////
    //thread_yield();

    
}


int thread_create_with_prio(thread_t *newthread, void *(*func)(void *), void *funcarg, int prio) {

    sig_block();
    if (ready_list == NULL){
    	thread_t main_thread;
    	main_thread=malloc(sizeof(struct thread));
	main_thread->sleeping_list=NULL;
    	//getcontext(&main_thread->uc);
	ready_list = g_list_append(ready_list, main_thread);
	ready_list_end = ready_list;

	struct sigaction act;

	act.sa_handler = sigvtalarm_treatment;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;

	sigaction(SIGVTALRM,&act,NULL);

	struct itimerval new_value;

	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_usec = TIMESLICE;
	fprintf(stderr,"%d\n",new_value.it_interval.tv_usec);
	new_value.it_value = new_value.it_interval;

	setitimer(ITIMER_VIRTUAL,
		  &new_value,
		  NULL);

	thread_initSigTab(main_thread);

    }

    *newthread = malloc(sizeof(struct thread));
    (*newthread)->sleeping_list = NULL;

    if(getcontext(&((*newthread)->uc)) == -1)
	return -1;
    (*newthread)->uc.uc_stack.ss_size = 64*1024;
    (*newthread)->uc.uc_stack.ss_sp = malloc((*newthread)->uc.uc_stack.ss_size);

    /* initialisation de la priorité*/
    int pr = 0;
    if(prio > MAX_PRIO)
	pr = MAX_PRIO;
    else if(prio < MIN_PRIO)
	pr = MIN_PRIO;
    else
	pr = prio;
    (*newthread)->basic_prio = pr;
    (*newthread)->current_prio = pr;

    /* juste après l'allocation de la pile */
    (*newthread)->stackid =
	VALGRIND_STACK_REGISTER((*newthread)->uc.uc_stack.ss_sp,
				(*newthread)->uc.uc_stack.ss_sp +
				(*newthread)->uc.uc_stack.ss_size);

    (*newthread)->uc.uc_link = NULL;

    makecontext(&(*newthread)->uc, (void (*)(void)) func, 1, funcarg);
    //return -1;

    ADD_THREAD(*newthread);
    thread_initSigTab(*newthread);
    sig_unblock();
    fprintf(stderr,"dsfd\n");
    return 0;
}

int thread_create(thread_t *newthread, void *(*func)(void *), void *funcarg) {
    return thread_create_with_prio(newthread, func, funcarg, DEFAULT_PRIO);
}

int thread_yield(void) {
    thread_t next, current = g_list_nth_data(ready_list, 0);
    int ok;

    //reinitialisation de la prio
    current->current_prio = current->basic_prio;

    ready_list = g_list_remove(ready_list, current);

    ADD_THREAD(current);

    next = g_list_nth_data(ready_list, 0);
    fprintf(stderr,"next%p\n",next);
    struct itimerval new_value;

    getitimer(ITIMER_VIRTUAL,
	      &new_value);
    
    if (new_value.it_value.tv_usec) {
	new_value.it_value = new_value.it_interval;
	
	setitimer(ITIMER_VIRTUAL,
		  &new_value,
		  NULL);
    }
    
    ok=swapcontext(&current->uc, &next->uc);

    if(!ok)
      thread_sigTreat(current);

    return ok;
}

int thread_join(thread_t thread, void **retval) {

    int found = 0;
    unsigned int i;
    for(i = 0; i < g_list_length(ready_list); i++) {
	thread_t t = g_list_nth_data(ready_list, i);
	if(thread == t) {
	    found = 1;
	    break;
	}
	else {
	    if(g_list_find(t->sleeping_list, thread) != NULL) {
		found = 1;
		break;
	    }
	}
    }

    if (found){

	thread_t next, current = g_list_nth_data(ready_list, 0);

	ready_list = g_list_remove(ready_list, current);

	thread->sleeping_list = g_list_append(thread->sleeping_list, current);

	next = g_list_nth_data(ready_list, 0);

	struct itimerval new_value;

	getitimer(ITIMER_VIRTUAL,
	      &new_value);

	if (new_value.it_value.tv_usec) {
	    new_value.it_value = new_value.it_interval;

	    setitimer(ITIMER_VIRTUAL,
		      &new_value,
		      NULL);
	}


	if(swapcontext(&current->uc, &next->uc) == -1)
	    return -1;

	*retval = current->retval;

	thread_sigTreat(current);

	if (g_list_index(zombie_list, thread) != -1){
	    zombie_list = g_list_remove(zombie_list,thread);
	    /* juste avant de libérer la pile */
	    VALGRIND_STACK_DEREGISTER(thread->stackid);
	    free(thread->uc.uc_stack.ss_sp);


	    free(thread);

	}

	thread_t cur_t =  g_list_nth_data(ready_list, 0);
	if(g_list_length(ready_list)==1 && g_list_length(cur_t->sleeping_list)==0){
	    g_list_free(cur_t->sleeping_list);

	    free(cur_t);


	    new_value.it_interval.tv_sec = 0;
	    new_value.it_interval.tv_usec = 0;

	    setitimer(ITIMER_VIRTUAL,
		      &new_value,
		      NULL);


	    g_list_free(ready_list);
	    ready_list=NULL;
	}
    }
    else if (g_list_index(zombie_list,thread)!=-1){

	thread_t waiter = g_list_nth_data(zombie_list,(g_list_index(zombie_list,
								    thread)));
	*retval = waiter->retval;
	zombie_list = g_list_remove(zombie_list,thread);
	free(thread->uc.uc_stack.ss_sp);
	/* juste avant de libérer la pile */
	VALGRIND_STACK_DEREGISTER(thread->stackid);
	/* free(thread->retval); */
	free(thread);
    }
    else {
	*retval = NULL;
	fprintf(stderr, "le thread %p n'existe pas\n", thread);
	return -1;
    }
    return 0;
}


static void wakeup_func(thread_t data, gpointer user_data) {
    if(data != NULL) {
	data->retval = user_data;
	ADD_THREAD(data);
    }
}


void thread_exit(void *retval) {
    
    thread_t head = g_list_nth_data(ready_list, 0);

    g_list_foreach(head->sleeping_list,
    		   (GFunc)wakeup_func,
    		   retval);

    g_list_free(head->sleeping_list);

    head->retval=retval;

    ready_list = g_list_remove(ready_list, head);
    zombie_list = g_list_append(zombie_list, head);

    struct itimerval new_value;
    new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_usec = TIMESLICE;
	fprintf(stderr,"%d\n",new_value.it_interval.tv_usec);
	new_value.it_value = new_value.it_interval;
	
	//getitimer(ITIMER_VIRTUAL,
	//    &new_value);
	
    if (new_value.it_value.tv_usec) {
	new_value.it_value = new_value.it_interval;
	
	setitimer(ITIMER_VIRTUAL,
		  &new_value,
		  NULL);
    }
        
    setcontext(&(((thread_t)g_list_nth_data(ready_list, 0))->uc));

    exit(0);
}

void thread_kill(thread_t thr, int sig){
    int* new_sig;
    if(thr==NULL)
	return;
    
    if(sig>=0){
      new_sig=malloc(sizeof(int));
      *new_sig=sig;
      thr->sig_list=g_list_append(thr->sig_list, new_sig);  
    }
    else{
      switch(sig){
      case SIG_STOP:
	if(g_list_index(ready_list, thr)!=-1){
	  stopped_list = g_list_append(stopped_list, thr);
	  ready_list = g_list_remove(ready_list, thr);
	}
	break;
      case SIG_WAKE:
	if(g_list_index(stopped_list, thr)!=-1){
	  ADD_THREAD(thr);
	  stopped_list = g_list_remove(stopped_list, thr);
	}
	break;
      default:
	printf("signal ordonnanceur %d recu\n", sig);
      }
    }
    
}

void thread_signal(thread_t thr, int sig, void (*sig_func)(int)){
    if(thr==NULL)
	return;

    if(sig<0 || sig >= NB_SIG)
	return;

    thr->treat_tab[sig]=sig_func;
}

void thread_initSigTab(thread_t thr){
    int i;

    for(i=0; i<NB_SIG; i++)
	thr->treat_tab[i]=basic_sig_treatment;

    thr->sig_list = NULL;
}

void thread_sigTreat(thread_t thr){
    if(thr==NULL)
	return;
    
    while(g_list_length(thr->sig_list)>0){
	int* sig = g_list_nth_data(thr->sig_list, 0);
	
	if(*sig>=0 && *sig<NB_SIG){
	    (*(thr->treat_tab[*sig])) (*sig);
	}
	
	thr->sig_list=g_list_remove(thr->sig_list, sig);
	free(sig);
    }
}

void sig_block() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &set, NULL);
}
void sig_unblock() {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    sigprocmask(SIG_UNBLOCK, &set, NULL);
}

