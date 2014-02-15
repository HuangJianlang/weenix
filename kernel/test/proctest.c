#include "proc/proc.h"
#include "util/list.h"
#include "proc/sched.h"
#include "util/debug.h"
#include "errno.h"

#define NUM_PROCS 3

typedef enum {ANY, SPECIFIC} waitpid_type_t;

static void * simple_function(int arg1, void *arg2);
    
static int in_child_list(proc_t *myproc){
    list_link_t *link;
    list_t *child_list = &myproc->p_pproc->p_children;

    for (link = child_list->l_next; link != child_list; link = link->l_next){
        proc_t *p = list_item(link, proc_t, p_child_link);
        if (p == myproc){
            return 1;
        }
    }

    return 0;
}

/*
 * Should be called from the init proc 
 */
static void test_proc_create(){
    dbg_print("testing proc_create\n");

    proc_t *myproc = proc_create("myproc");

    KASSERT(list_empty(&myproc->p_threads));
    KASSERT(list_empty(&myproc->p_children));

    KASSERT(sched_queue_empty(&myproc->p_wait));

    KASSERT(myproc->p_pproc->p_pid == 1 && "created proc's parent isn't the init proc\n");
    KASSERT(myproc->p_state == PROC_RUNNING);

    /* make sure it's in the proc list */
    KASSERT(proc_lookup(myproc->p_pid) == myproc && "created proc not in proc list\n");

    /* make sure it's in it's parent's child list */
    KASSERT(in_child_list(myproc));

    /* clean everything up */
    kthread_t *mythread = kthread_create(myproc, simple_function, NULL, NULL);
    sched_make_runnable(mythread);

    int status;
    do_waitpid(myproc->p_pid, 0, &status);
}

/* 
 * A simple function
 */
static void * simple_function(int arg1, void *arg2){
    dbg_print("Running a simple method from test thread %d\n", arg1);
    dbg_print("Exiting a simple method from test thread %d\n", arg1);

    return NULL;
}

static void test_do_waitpid(waitpid_type_t type){
    proc_t *test_procs[NUM_PROCS];
    kthread_t *test_threads[NUM_PROCS];

    int i;
    for (i = 0; i < NUM_PROCS; i++){
        test_procs[i] = proc_create("test proc");
        test_threads[i] = kthread_create(test_procs[i], simple_function, i, NULL);
        sched_make_runnable(test_threads[i]);
    }

    int j;

    for (j = 0; j < NUM_PROCS; j++){

        if (type == ANY){
            int status;
            do_waitpid(-1, 0, &status);
        } else {
            int status;
            pid_t proc_pid = test_procs[j]->p_pid;

            pid_t waitpid_pid = do_waitpid(proc_pid, 0, &status);

            KASSERT(waitpid_pid == proc_pid);
        }
    }

    int k;
    for (k = 0; k < NUM_PROCS; k++){
        proc_t *p = test_procs[k];

        KASSERT(proc_lookup(p->p_pid) == NULL);

        /* make sure all children have been reparented */
        KASSERT(list_empty(&p->p_children));

        /* make sure that it is no longer in it's parent's
         * child list
         */
        KASSERT(!in_child_list(p));

        /* make sure it exited with the correct status */
        KASSERT(p->p_status == 0);

        KASSERT(p->p_state == PROC_DEAD);

        KASSERT(sched_queue_empty(&p->p_wait));
    }
}

static void test_do_waitpid_no_child(){

    pid_t pid;

    /* find a PID that definitely isn't a child of curproc */
    for (pid = 0; proc_lookup(pid) != NULL; pid++){}
    
    int status;

    pid_t returned_pid = do_waitpid(pid, 0, &status);

    KASSERT(returned_pid = -ECHILD);
} 

/*
 * Should be called from a new process
 */
static void * test_do_exit_and_do_waitpid(int arg1, void *arg2){
    int status;

    dbg_print("testing do_waitpid on an invalid PID\n");
    KASSERT(do_waitpid(-1, 0, &status) == -ECHILD);

    dbg_print("testing do_waitpid on an empty child list\n");
    KASSERT(do_waitpid(5, 0, &status) == -ECHILD);

    dbg_print("testing do_waitpid with pid == -1\n");
    test_do_waitpid(ANY);

    dbg_print("testing do waitpid with specific pids\n");
    test_do_waitpid(SPECIFIC);

    dbg_print("testing do_waitpid with non-child pid\n");
    test_do_waitpid_no_child();

    return NULL;
}

void run_proc_tests(){

    test_proc_create();

    proc_t *waitpid_test_proc = proc_create("waitpid_test_proc");
    kthread_t *waitpid_test_thread = kthread_create(waitpid_test_proc,
            test_do_exit_and_do_waitpid, NULL, NULL);

    sched_make_runnable(waitpid_test_thread);

    int status;
    do_waitpid(waitpid_test_proc->p_pid, 0, &status);

    dbg(DBG_TESTPASS, "all tests passed!\n");
}


/* TODO:
   - mutexes
   - test cancelling
   - proc kill
   - proc kill all
*/
