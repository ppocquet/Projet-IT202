#include "thread.h"
#include <glib.h>
#include <stdlib.h>
#include <ucontext.h>
#include <valgrind/valgrind.h>
#include<stdio.h>

GList * ready_list = NULL;
GList * ready_list_end = NULL;
GList * zombie_list = NULL;

#define DEFAULT_PRIO 0
#define MIN_PRIO -20
#define MAX_PRIO 19

struct thread {
    ucontext_t uc;
    GList * sleeping_list;
    void *retval;
    int basic_prio; /* MIN_PRIO à MAX_PRIO */
    int current_prio;

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



thread_t thread_self(void) {
    return (thread_t)g_list_nth_data(ready_list, 0);
}

int thread_create_with_prio(thread_t *newthread, void *(*func)(void *), void *funcarg, int prio) {
    if (ready_list == NULL){
    	thread_t main_thread;
    	main_thread=malloc(sizeof(struct thread));
	main_thread->sleeping_list=NULL;
    	//getcontext(&main_thread->uc);
	ready_list = g_list_append(ready_list, main_thread);
	ready_list_end = ready_list; 
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
    //ready_list = g_list_append(ready_list, *newthread);
    prio_update_sorted_insert_by_end(*newthread);

    return 0;
}

int thread_create(thread_t *newthread, void *(*func)(void *), void *funcarg) {
    return thread_create_with_prio(newthread, func, funcarg, DEFAULT_PRIO);
}

int thread_yield(void) {
    thread_t next, current = g_list_nth_data(ready_list, 0);
    //reinitialisation de la prio
    current->current_prio = current->basic_prio;
    
    prio_update_sorted_insert_by_end(current);
    ready_list = g_list_remove(ready_list, current);
    
    next = g_list_nth_data(ready_list, 0);
    return swapcontext(&current->uc, &next->uc);
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

	if(swapcontext(&current->uc, &next->uc) == -1)
	    return -1;

	*retval = current->retval;

	if (g_list_index(zombie_list, thread) != -1){
	    zombie_list = g_list_remove(zombie_list,thread);
	    free(thread->uc.uc_stack.ss_sp);
	    /* juste avant de libérer la pile */
	    VALGRIND_STACK_DEREGISTER(thread->stackid);

	    free(thread);

	}

	thread_t cur_t =  g_list_nth_data(ready_list, 0);
	if(g_list_length(ready_list)==1 && g_list_length(cur_t->sleeping_list)==0){
	    g_list_free(cur_t->sleeping_list);

	    free(cur_t);

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
	prio_update_sorted_insert_by_end(data);
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
    setcontext(&(((thread_t)g_list_nth_data(ready_list, 0))->uc));

    exit(0);
}
