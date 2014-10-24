/* W4118 grouped round robin scheduler */

#include "sched.h"
#include <linux/slab.h>

#define	PRINTK	printk

/* TODO: @lfred init rq function */
void init_grr_rq(struct grr_rq *rt_rq, struct rq *rq) {
	PRINTK("init_grr_rq\n");
}

/*
 * The enqueue_task method is called before nr_running is
 * increased. Here we update the fair scheduling stats and
 * then put the task into the rbtree:
 */
static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	PRINTK("enqueue_task_grr\n");
#if 0
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

/* @lfred: need to check when this will be called. */
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
 */
static void task_tick_grr(struct rq *rq, struct task_struct *curr, int queued)
{
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
