/* W4118 round robin scheduler */

#include "sched.h"
#include <linux/slab.h>

/* TODO: @lfred init rq function */
void init_rr_rq(struct rr_rq *rt_rq, struct rq *rq) {
}

static void
enqueue_task_rr(struct rq *rq, struct task_struct *p, int flags)
{
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

static void
dequeue_task_rr(struct rq *rq, struct task_struct *p, int flags)
{
	raw_spin_unlock_irq(&rq->lock);
	printk(KERN_ERR "bad: scheduling from the idle thread!\n");
	dump_stack();
	raw_spin_lock_irq(&rq->lock);
}

static void yield_task_rr(struct rq *rq)
{
#if 0
	requeue_task_rr(rq, rq->curr, 0);
#endif
}

static void check_preempt_curr_rr(struct rq *rq, struct task_struct *p, int flags)
{
	resched_task(rq->idle);
}

static struct task_struct *pick_next_task_rr(struct rq *rq)
{
	schedstat_inc(rq, sched_goidle);
	calc_load_account_idle(rq);
	return rq->idle;
}

static void put_prev_task_rr(struct rq *rq, struct task_struct *prev)
{
}

static void task_tick_rr(struct rq *rq, struct task_struct *curr, int queued)
{
}

static void set_curr_task_rr(struct rq *rq)
{
}

static void switched_to_rr(struct rq *rq, struct task_struct *p)
{
	BUG();
}

static void
prio_changed_rr(struct rq *rq, struct task_struct *p, int oldprio)
{
	BUG();
}

static unsigned int get_rr_interval_rr(struct rq *rq, struct task_struct *task)
{
	return 0;
}

/*
 * Simple, special scheduling class for the per-CPU idle tasks:
 */
const struct sched_class rr_sched_class = {
	.next			= &idle_sched_class,
	
	.enqueue_task		= enqueue_task_rr,
	.dequeue_task		= dequeue_task_rr,
	.yield_task		= yield_task_rr,
	/* .yield_to_task		= yield_to_task_fair, */

	.check_preempt_curr	= check_preempt_curr_rr,

	.pick_next_task		= pick_next_task_rr,
	.put_prev_task		= put_prev_task_rr,

#ifdef CONFIG_SMP
	.select_task_rq		= select_task_rq_rr,
	
#if 0
	.rq_online		= rq_online_fair,
	.rq_offline		= rq_offline_fair,

	.task_waking		= task_waking_fair,
#endif
#endif

	.set_curr_task          = set_curr_task_rr,
	.task_tick		= task_tick_rr,

	.get_rr_interval	= get_rr_interval_rr,

	.prio_changed		= prio_changed_rr,
	.switched_to		= switched_to_rr,
};
