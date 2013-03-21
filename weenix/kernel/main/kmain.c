/*
 The goal of bootstrap()is to set up the first kernel thread and process which should start executing the idleproc_run()function. 
 This should be a piece of cake once you've implemented threads and the scheduler. 

 idleproc_run()performs further initialization 
 and starts the init process using initproc_create(), which starts executing in initproc_run(). All of your test code should be 
 in initproc_run(). When your operating system is nearly complete, it will execute a binary program in user mode, but for now, 
 be content to put together your own testing system for threads and processes. Once you have implemented terminal drivers, you 
 can test here by setting up the kernel shell from test/kshell/.
*/

#include "types.h"
#include "globals.h"
#include "kernel.h"

#include "util/gdb.h"
#include "util/init.h"
#include "util/debug.h"
#include "util/string.h"
#include "util/printf.h"

#include "mm/mm.h"
#include "mm/page.h"
#include "mm/pagetable.h"
#include "mm/pframe.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "main/acpi.h"
#include "main/apic.h"
#include "main/interrupt.h"
#include "main/cpuid.h"
#include "main/gdt.h"

#include "proc/sched.h"
#include "proc/proc.h"
#include "proc/kthread.h"

#include "drivers/dev.h"
#include "drivers/blockdev.h"
#include "drivers/tty/virtterm.h"

#include "api/exec.h"
#include "api/syscall.h"

#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/fcntl.h"
#include "fs/stat.h"

#include "test/kshell/kshell.h"
#define TEST_NUMS 5

GDB_DEFINE_HOOK(boot)
GDB_DEFINE_HOOK(initialized)
GDB_DEFINE_HOOK(shutdown)

static void      *bootstrap(int arg1, void *arg2);
static void      *idleproc_run(int arg1, void *arg2);
static kthread_t *initproc_create(void);
static void      *initproc_run(int arg1, void *arg2);
static void       hard_shutdown(void);
static void      *test(int arg1, void *arg2);


static context_t bootstrap_context;

/**
 * This is the first real C function ever called. It performs a lot off
 * hardware-specific initialization, then creates a pseudo-context to
 * execute the bootstrap function in.
 */
void
kmain()
{
        GDB_CALL_HOOK(boot);

        dbg_init();
        dbgq(DBG_CORE, "Kernel binary:\n");
        dbgq(DBG_CORE, "  text: 0x%p-0x%p\n", &kernel_start_text, &kernel_end_text);
        dbgq(DBG_CORE, "  data: 0x%p-0x%p\n", &kernel_start_data, &kernel_end_data);
        dbgq(DBG_CORE, "  bss:  0x%p-0x%p\n", &kernel_start_bss, &kernel_end_bss);

        page_init();

        pt_init();
        slab_init();
        pframe_init();

        acpi_init();
        apic_init();
        intr_init();

        gdt_init();

        /* initialize slab allocators */
#ifdef __VM__
        anon_init();
        shadow_init();
#endif
        vmmap_init();
        proc_init();
        kthread_init();

#ifdef __DRIVERS__
        bytedev_init();
        blockdev_init();
#endif

        void *bstack = page_alloc();
        pagedir_t *bpdir = pt_get();
        KASSERT(NULL != bstack && "Ran out of memory while booting.");
        context_setup(&bootstrap_context, bootstrap, 0, NULL, bstack, PAGE_SIZE, bpdir);
        context_make_active(&bootstrap_context);

        panic("\nReturned to kmain()!!!\n");
}

/**
 * This function is called from kmain, however it is not running in a
 * thread context yet. It should create the idle process which will
 * start executing idleproc_run() in a real thread context.  To start
 * executing in the new process's context call context_make_active(),
 * passing in the appropriate context. This function should _NOT_
 * return.
 *
 * Note: Don't forget to set curproc and curthr appropriately.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
bootstrap(int arg1, void *arg2)
{
        /*--taohu--------dbg----------------*/
        dbg(DBG_CORE,"Enter bootstrap()\n");

        /* necessary to finalize page table information */
        pt_template_init();

        /* ---------------------heguang-------------------- */
        proc_t *idle_process=proc_create("idle_proc");
        /*--taohu--------Gradeline requirement----------------*/
        KASSERT(NULL != idle_process); /* make sure that the "idle" process has
been created successfully */
        KASSERT(PID_IDLE==idle_process->p_pid);/* make sure that what has been
created is the "idle" process */
         /*--taohu-------------------------------------------*/
        
        kthread_t *idle_thread=kthread_create(idle_process,idleproc_run,0,NULL);
        /*--taohu--------Gradeline requirement----------------*/
        KASSERT(NULL != idle_thread); /* make sure that the thread for the "idle"
process has been created successfully */
         /*--taohu-------------------------------------------*/

        curproc=idle_process;
        curthr=idle_thread;

        /*curproc->p_state=PROC_RUNNING;*/
        curthr->kt_state=KT_RUN;
        context_make_active(&curthr->kt_ctx);

        /*--taohu--------dbg----------------*/
        dbg(DBG_CORE,"Leave bootstrap()\n");

        /* ---------------------heguang-------------------- */
       panic("weenix returned to bootstrap()!!! BAD!!!\n");
        return NULL;
}

/**
 * Once we're inside of idleproc_run(), we are executing in the context of the
 * first process-- a real context, so we can finally begin running
 * meaningful code.
 *
 * This is the body of process 0. It should initialize all that we didn't
 * already initialize in kmain(), launch the init process (initproc_run),
 * wait for the init process to exit, then halt the machine.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
static void *
idleproc_run(int arg1, void *arg2)
{
        /*--taohu--------dbg----------------*/
        dbg(DBG_CORE,"Enter idleproc_run()\n");

        int status;
        pid_t child;

        /* create init proc */
        kthread_t *initthr = initproc_create();

        init_call_all();
        GDB_CALL_HOOK(initialized);

        /* Create other kernel threads (in order) */

#ifdef __VFS__
        /* Once you have VFS remember to set the current working directory
         * of the idle and init processes */

        /* Here you need to make the null, zero, and tty devices using mknod */
        /* You can't do this until you have VFS, check the include/drivers/dev.h
         * file for macros with the device ID's you will need to pass to mknod */
#endif

        /* Finally, enable interrupts (we want to make sure interrupts
         * are enabled AFTER all drivers are initialized) */
        intr_enable();

        /* Run initproc */
        sched_make_runnable(initthr);
        /* Now wait for it */
        child = do_waitpid(-1, 0, &status);
        KASSERT(PID_INIT == child);

#ifdef __MTP__
        kthread_reapd_shutdown();
#endif


#ifdef __VFS__
        /* Shutdown the vfs: */
        dbg_print("weenix: vfs shutdown...\n");
        vput(curproc->p_cwd);
        if (vfs_shutdown())
                panic("vfs shutdown FAILED!!\n");

#endif

        /* Shutdown the pframe system */
#ifdef __S5FS__
        pframe_shutdown();
#endif

        dbg_print("\nweenix: halted cleanly!\n");
        GDB_CALL_HOOK(shutdown);
        hard_shutdown();
       /*--taohu--------dbg----------------*/
        dbg(DBG_CORE,"Enter idleproc_run()\n");
        return NULL;
}

/**
 * This function, called by the idle process (within 'idleproc_run'), creates the
 * process commonly refered to as the "init" process, which should have PID 1.
 *
 * The init process should contain a thread which begins execution in
 * initproc_run().
 *
 * @return a pointer to a newly created thread which will execute
 * initproc_run when it begins executing
 */
static kthread_t *
initproc_create(void)
{
    dbg(DBG_CORE,"Enter initproc_create()\n");
    proc_t *init_process=proc_create("init_process");
    KASSERT(NULL != init_process);
    KASSERT(PID_INIT==init_process->p_pid);
    kthread_t *init_thread=kthread_create(init_process,initproc_run,0,NULL);    
    KASSERT(NULL != init_thread);
    return init_thread;
    /* ---------------------heguang-------------------- */
     dbg(DBG_CORE,"Leave initproc_create()\n");
}

/**
 * The init thread's function changes depending on how far along your Weenix is
 * developed. Before VM/FI, you'll probably just want to have this run whatever
 * tests you've written (possibly in a new process). After VM/FI, you'll just
 * exec "/bin/init".
 *
 * Both arguments are unused.
 *
 * @param arg1 the first argument (unused)
 * @param arg2 the second argument (unused)
 */
 /* Yu Sun Code Start */
/* ----------------for test------------------- */
kmutex_t          mtx, pc_mutex;
static void      *deadlock_test(int arg1, void *arg2);
int               share_resource;
static void      *producer(int arg1, void *arg2);
static void      *consumer(int arg1, void *arg2);
ktqueue_t         kt_wproq, kt_wconq;


static void *
initproc_run(int arg1, void *arg2)
{
     /*--taohu--------dbg----------------*/
     dbg(DBG_CORE,"Enter initproc_run()\n");

    /* ----------------deadlock---------------------- */
    proc_t * proc1, * proc2;
    kthread_t * kthr1, * kthr2;
    kmutex_init(&mtx);
    proc1 = proc_create("proc1");
    kthr1=kthread_create(proc1,deadlock_test,0,NULL);
    proc2 = proc_create("proc2");
    kthr2=kthread_create(proc2,deadlock_test,0,NULL);
    sched_make_runnable(kthr1);
    sched_make_runnable(kthr2);
    /* ----------------deadlock---------------------- */

    /* ----------------producer/consumer---------------------- */
    proc_t * pproducer, * pconsumer;
    kthread_t * kproducer, * kconsumer;
    kmutex_init(&pc_mutex);
    pproducer = proc_create("producer");
    kproducer=kthread_create(pproducer,producer,0,NULL);
    pconsumer = proc_create("consumer");
    kconsumer=kthread_create(pconsumer,consumer,0,NULL);
    sched_queue_init(&kt_wproq);
    sched_queue_init(&kt_wconq);
    sched_make_runnable(kproducer);
    sched_make_runnable(kconsumer);
    /* ----------------producer/consumer---------------------- */

    /* ---------------------heguang-------------------- */

    int status[TEST_NUMS];
    pid_t child[TEST_NUMS];
    proc_t *process[TEST_NUMS];
    kthread_t *thread[TEST_NUMS];
    int i;
    for(i=0;i<TEST_NUMS - 4;i++)
    {
        process[i]=proc_create("test_process");
        thread[i]=kthread_create(process[i],test,0,NULL);
        sched_make_runnable(thread[i]);
    }
    i=0;
    while(!list_empty(&curproc->p_children))
    {
        child[i]=do_waitpid(-1,0,&status[i]);
        KASSERT(status[i]==0);
        dbg_print("process %d return.\n",(int)child[i]);
        i++;
    }

    /* ---------------------heguang-------------------- */

     /*--taohu--------dbg----------------*/
     dbg(DBG_CORE,"Leave initproc_run()\n");
    return NULL;
}

static void *
producer(int arg1, void *arg2) {
    int i = 0;
    while(i < 10) {
        dbg_print("in producer proc\n");
        kmutex_lock(&pc_mutex);

        dbg_print("producer proc get mutex\n");
        if(share_resource == 0) {
            share_resource = 1;
            i++;
            dbg_print("producer proc produce resourcs %d, wakeup consumer proc\n", i);
            kmutex_unlock(&pc_mutex);
            sched_wakeup_on(&kt_wconq);
        }
        else {
            dbg_print("producer proc wait for consumer proc\n");
            kmutex_unlock(&pc_mutex);
            sched_wakeup_on(&kt_wconq);
            sched_sleep_on(&kt_wproq);
        }
    }
    return NULL;
}

static void *
consumer(int arg1, void *arg2) {
    int i = 0;
    while(i < 10) {
        dbg_print("in consumer proc\n");
        kmutex_lock(&pc_mutex);
        dbg_print("consumer proc get mutex\n");
        if(share_resource == 1) {
            share_resource = 0;
            i++;
            dbg_print("consumer proc consume resourcs %d, wakeup producer proc\n", i);
            kmutex_unlock(&pc_mutex);
            sched_wakeup_on(&kt_wproq);
        }
        else {
            dbg_print("consumer proc wait for producer proc\n");
            kmutex_unlock(&pc_mutex);
            sched_wakeup_on(&kt_wproq);
            sched_sleep_on(&kt_wconq);
        }
    }
    return NULL;
}

static void *
deadlock_test(int arg1, void *arg2) {
    dbg_print("deadlock_test function start.\n");
    kmutex_lock(&mtx);
    dbg_print("deadlock_test function return.\n");
    return NULL;
}

static void *
test(int arg1, void *arg2)
{
    /* ---------------------heguang-------------------- */
    dbg_print("test function start.\n");
    /*add test code here.*/
    /*while(1) {

    }*/
    dbg_print("test function return.\n");
    return NULL;
}
 /* Yu Sun Code Finish */

/**
 * Clears all interrupts and halts, meaning that we will never run
 * again.
 */
static void
hard_shutdown()
{
#ifdef __DRIVERS__
        vt_print_shutdown();
#endif
        __asm__ volatile("cli; hlt");
}
