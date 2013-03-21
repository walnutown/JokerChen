/*
Since the kernel is multi-threaded, we need some way to ensure that certain critical paths are not executed simultaneously by multiple threads. Once mechanism you will need is the mutex. Feel free to implement semaphores, condition variables, or read-write locks as well if you so desire, though they are in no way necessary to have a functioning kernel.
Functions you will need to write in proc/kmutex.c:
void kmutex_init(kmutex_t *mtx);
void kmutex_lock(kmutex_t *mtx);
void kmutex_lock_cancellable(kmutex_t *mtx); void kmutex_unlock(kmutex_t *mtx);
*/

#include "globals.h"
#include "errno.h"

#include "util/debug.h"

#include "proc/kthread.h"
#include "proc/kmutex.h"

/*
 * IMPORTANT: Mutexes can _NEVER_ be locked or unlocked from an
 * interrupt context. Mutexes are _ONLY_ lock or unlocked from a
 * thread context.
 */

void
kmutex_init(kmutex_t *mtx)
{
	/* ---------------------heguang-------------------- */
	sched_queue_init(&mtx->km_waitq);
	mtx->km_holder=NULL;
	/* ---------------------heguang-------------------- */
    NOT_YET_IMPLEMENTED("PROCS: kmutex_init");
}

/*
 * This should block the current thread (by sleeping on the mutex's
 * wait queue) if the mutex is already taken.
 *
 * No thread should ever try to lock a mutex it already has locked.
 */
void
kmutex_lock(kmutex_t *mtx)
{
	/* ---------------------heguang-------------------- */
	if(mtx->km_holder!=NULL)
	{
		sched_sleep_on(&mtx->km_waitq);
	}
	mtx->km_holder=curthr;
	/* ---------------------heguang-------------------- */
	NOT_YET_IMPLEMENTED("PROCS: kmutex_lock");
}

/*
 * This should do the same as kmutex_lock, but use a cancellable sleep
 * instead.
 */
int
kmutex_lock_cancellable(kmutex_t *mtx)
{
    /* ---------------------heguang-------------------- */
    if(mtx->km_holder!=NULL)
    {
    	int val=sched_cancellable_sleep_on(&mtx->km_waitq);
    	if(val!=-EINTR)/*thread is not cancelled*/
    	{
    		mtx->km_holder=curthr;
    	}
    	return val;
    }
    else
    {
    	mtx->km_holder=curthr;
    	return 0;
    }
    /* ---------------------heguang-------------------- */
        NOT_YET_IMPLEMENTED("PROCS: kmutex_lock_cancellable");
}

/*
 * If there are any threads waiting to take a lock on the mutex, one
 * should be woken up and given the lock.
 *
 * Note: This should _NOT_ be a blocking operation!
 *
 * Note: Don't forget to add the new owner of the mutex back to the
 * run queue.
 *
 * Note: Make sure that the thread on the head of the mutex's wait
 * queue becomes the new owner of the mutex.
 *
 * @param mtx the mutex to unlock
 */
void
kmutex_unlock(kmutex_t *mtx)
{
    /* ---------------------heguang-------------------- */
    if(mtx->km_waitq.tq_size==0)
    {
    	mtx->km_holder=NULL;
    }
    else
    {
    	mtx->km_holder=sched_wakeup_on(&mtx->km_waitq);
    }
    /* ---------------------heguang-------------------- */
    NOT_YET_IMPLEMENTED("PROCS: kmutex_unlock");

}
