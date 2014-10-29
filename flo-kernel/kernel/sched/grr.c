/* W4118 grouped round robin scheduler */
/* Includes */
/*****************************************************************************/
#include "sched.h"
#include <linux/slab.h>

/* Defines */
/*****************************************************************************/
#define	PRINTK	printk
#define M_GRR_TIME_SLICE 	100
#define M_GRR_LOAD_BALANCE_TIME 500

/* Utility functions */
/*****************************************************************************/
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

/* REAL thing here */
/*****************************************************************************/
/* init func: 
 *	For each cpu, it will be called once. Thus, the rq is a PER_CPU data 
 *	structure.
 */
void init_grr_rq(struct grr_rq *grr_rq, struct rq *rq) 
{
	grr_rq->mp_rq = rq;
	grr_rq->m_nr_running = 0;
	grr_rq->m_rebalance_cnt = 0;
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
	struct raw_spinlock *p_lock = &(rq->grr.m_runtime_lock);

	raw_spin_lock_irq(p_lock);

	/* init time slice */
	p->grr.time_slice = GRR_TIMESLICE;
	
	INIT_LIST_HEAD(&(p->grr.m_rq_list));

	list_add_tail(&(p->grr.m_rq_list), &(rq->grr.m_task_q));
	rq->grr.m_nr_running++;	

	raw_spin_unlock_irq(p_lock);
	
	inc_nr_running(rq);	
}

/*
 * The dequeue_task method is called before nr_running is
 * decreased. We remove the task from the rbtree and
 * update the fair scheduling stats:
 */
static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	struct raw_spinlock *p_lock = &rq->grr.m_runtime_lock;
	
	raw_spin_lock_irq(p_lock);

	list_del(&(p->grr.m_rq_list));
	rq->grr.m_nr_running--;	

	raw_spin_unlock_irq(p_lock);
	
	dec_nr_running(rq);
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
 *
 * Load Balancing: reference 'calc_load_account_idle'
 */
static struct task_struct *pick_next_task_grr(struct rq *rq)
{
#if 1
	struct sched_grr_entity* ent = NULL;
	struct task_struct *p = NULL;
	struct raw_spinlock *p_lock = &(rq->grr.m_runtime_lock);

	if (!rq->nr_running)
		return NULL;

	raw_spin_lock_irq(p_lock);
	
	if (rq->grr.m_nr_running != 0) {	

		ent = list_first_entry(
			&(rq->grr.m_task_q), 
			struct sched_grr_entity, 
			m_rq_list);   
		p = task_of_se(ent);
	}

	raw_spin_unlock_irq(p_lock);

	return p; 
#else	
	schedstat_inc(rq, sched_goidle);
	calc_load_account_idle(rq);
	return rq->idle;
#endif
}

/*
 * Account for a descheduled task:
 *	When the current task is about to be moved out from
 *	CPU, this function will be called to allow the scheduler to
 *	update the data structure.
 */
static void put_prev_task_grr(struct rq *rq, struct task_struct *prev)
{
	struct raw_spinlock *p_lock = &rq->grr.m_runtime_lock;

	raw_spin_lock_irq(p_lock);
	
	list_del(&(prev->grr.m_rq_list));
	list_add_tail(&(prev->grr.m_rq_list), &(rq->grr.m_task_q));
	
	raw_spin_lock_irq(p_lock);
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
	struct sched_grr_entity *se = &curr->grr;

	/* Update statistics */
	if (--rq->grr.m_rebalance_cnt) {
		if(rq->grr.m_rebalance_cnt == 0){
			/* "set flag for rebalanceing */
			rq->grr.m_rebalance_cnt = GRR_REBALANCE;
		}
	}
	
	/* @lfred:
		not sure if there is a chance that tick twice 
		before you schedule. We take it conservatively.
	*/
	if (se->time_slice != 0) { 
		se->time_slice--;
		return;
	}
		
	/* the running task is expired. */
	/* reset the time slice variable */
	se->time_slice = GRR_TIMESLICE;

	/* @lfred: 
	 * 	it can be a problem - how to sync among CPUs ? 
	 */
	if (rq->grr.m_nr_running > 1) {
		/* Time up for the current entity */
		/* put the current task to the end of the list */
		list_del(&(curr->grr.m_rq_list));
		list_add_tail(&(curr->grr.m_rq_list), &(rq->grr.m_task_q));
		set_tsk_need_resched(curr);
	} else {
		/* doing nothing - if only one task, we should just run*/
		return;
	}
}

/* Account for a task changing its policy or group.
 *
 * This routine is mostly called to set cfs_rq->curr field when a task
 * migrates between groups/classes.
 */
static void set_curr_task_grr(struct rq *rq)
{
}

/*
 * We switched to the sched_grr class.
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

