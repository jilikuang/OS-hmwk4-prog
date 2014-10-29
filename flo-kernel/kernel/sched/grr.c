/* W4118 grouped round robin scheduler */

#include "sched.h"
#include <linux/slab.h>

/*****************************************************************************/
#define	PRINTK	printk
#define TIME_SLICE 		100
#define LOAD_BALANCE_TIME 	500
/*****************************************************************************/

/* Utility functions */
static inline struct task_struct *task_of_se(struct sched_grr_entity *grr_se)
{
	return container_of(grr_se, struct task_struct, grr);
}

static inline struct rq *rq_of_grr_rq(struct grr_rq *grr_rq)
{
	return container_of(grr_rq, struct rq, grr);
}

static inline struct grr_rq *grr_rq_of_se(struct sched_grr_entity *grr_se)
{
	struct task_struct *p = task_of_se(grr_se);
	struct rq *rq = task_rq(p);

	return &rq->grr;
}

/* TODO: @lfred init rq function 
 *	For each cpu, it will be called once. Thus, the rq is a PER_CPU data 
 *	structure.
 */
void init_grr_rq(struct grr_rq *grr_rq, struct rq *rq) 
{
	grr_rq->mp_rq = rq;
	grr_rq->m_nr_running = 0;
	INIT_LIST_HEAD(&grr_rq->m_task_q);
	raw_spin_lock_init(&grr_rq->m_runtime_lock);
}

/*
 * The enqueue_task method is called before nr_running is
 * increased. Here we update the fair scheduling stats and
 * then put the task into the rbtree:
 */
static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
#if 1
	INIT_LIST_HEAD(&(p->grr.m_rq_list));

	/* @lfred: need to enable */
	//inc_nr_running(rq);	
#else
	struct sched_rt_entity *rt_se = &p->rt;

	if (flags & ENQUEUE_WAKEUP)
		rt_se->timeout = 0;

	enqueue_rt_entity(rt_se, flags & ENQUEUE_HEAD);

	if (!task_current(rq, p) && p->rt.nr_cpus_allowed > 1)
		enqueue_pushable_task(rq, p);

	inc_nr_running(rq);
#endif
}

/*
 * The dequeue_task method is called before nr_running is
 * decreased. We remove the task from the rbtree and
 * update the fair scheduling stats:
 */
static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	PRINTK("dequeue_task_grr\n");
#if 0
	raw_spin_unlock_irq(&rq->lock);
	printk(KERN_ERR "bad: scheduling from the idle thread!\n");
	dump_stack();
	raw_spin_lock_irq(&rq->lock);
#endif
}

/* 
 * @lfred: This will be called when yield_sched is called
 * That is, the current task should be preempted and put
 * in the end of rq. If there is no more task, maybe we
 * should keep the calling task to run.
 */
static void yield_task_grr(struct rq *rq)
{
	PRINTK("yield_task_grr\n");
#if 0
	requeue_task_grr(rq, rq->curr, 0);
#endif
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void check_preempt_curr_grr(struct rq *rq, struct task_struct *p, int flags)
{
	PRINTK("check_preempt_curr_grr\n");
#if 0
	resched_task(rq->idle);
#endif
}

/*
 * return the next task to run: select a task in my run queue if there is any
 * check pick_next_task @ core.c
 * always return NULL for now, and no RR tasks are scheduled.
 */
static struct task_struct *pick_next_task_grr(struct rq *rq)
{
	PRINTK("pick_next_task_grr\n");
	return NULL;


#if 0	
	schedstat_inc(rq, sched_goidle);
	calc_load_account_idle(rq);
	return rq->idle;
#endif
}

/*
 * Account for a descheduled task:
 */
static void put_prev_task_grr(struct rq *rq, struct task_struct *prev)
{
	PRINTK("put_prev_task_grr\n");
}

/*
 * scheduler tick hitting a task of our scheduling class:
 * No print is permitted @ interrupt context or interrupt disabled.
 *
 * Job:
 *	1. Update the time slice of the runningt task.
 *	2. Update statistics information (check update_curr_rt)
 *
 * Note:
 * 	rq->lock is acquired before calling the functions.
 * 	interrupts are disables when calling this.
 */
static void task_tick_grr(struct rq *rq, struct task_struct *curr, int queued)
{
	struct sched_grr_entity *grr_se = &curr->grr;

	/* Update statistics */

	/*
	if (--rebalance_cnt) {
		"set flag for rebalanceing"
		rebalance_cnt = GRR_REBALANCE;
	}
	*/

	if (--grr_se->time_slice)
		return;

	grr_se->time_slice = GRR_TIMESLICE;

	/* Time up for the current entity */
}

/* Account for a task changing its policy or group.
 *
 * This routine is mostly called to set cfs_rq->curr field when a task
 * migrates between groups/classes.
 */
static void set_curr_task_grr(struct rq *rq)
{
	PRINTK("set_curr_task_grr\n");
}

/*
 * We switched to the sched_rr class.
 * @lfred: this is MUST for testing. We need to allow the task to become a rr task.
 */
static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	return;
}

/*
 * Priority of the task has changed. Check to see if we preempt
 * the current task.
 * @lfred: should we implement this ? return will be fine ?
 */
static void
prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{
	return;
}

static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	PRINTK("get_rr_interval_grr\n");
	return 0;
}

/*
 * Simple, special scheduling class for the per-CPU idle tasks:
 */
const struct sched_class grr_sched_class = {
	
	.next			= &fair_sched_class,
	
	.enqueue_task		= enqueue_task_grr,
	.dequeue_task		= dequeue_task_grr,
	.yield_task		= yield_task_grr,
	/* .yield_to_task		= yield_to_task_fair, */

	.check_preempt_curr	= check_preempt_curr_grr,

	.pick_next_task		= pick_next_task_grr,
	.put_prev_task		= put_prev_task_grr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_grr,
	
#if 0
	void (*pre_schedule) (struct rq *this_rq, struct task_struct *task);
	void (*post_schedule) (struct rq *this_rq);
	void (*set_cpus_allowed)(struct task_struct*, const struct cpumask*);

	.rq_online		= rq_online_fair,
	.rq_offline		= rq_offline_fair,

	.task_waking		= task_waking_fair,
	void (*task_woken) (struct rq *this_rq, struct task_struct *task);
#endif
#endif

	.set_curr_task          = set_curr_task_grr,
	.task_tick		= task_tick_grr,
	/* void (*task_fork) (struct task_struct *p); */

	/* void (*switched_from) (struct rq *this_rq, struct task_struct *task); */
	.switched_to		= switched_to_grr,
	
	.prio_changed		= prio_changed_grr,
	.get_rr_interval	= get_rr_interval_grr,

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* void (*task_move_group) (struct task_struct *p, int on_rq); */
#endif
};
