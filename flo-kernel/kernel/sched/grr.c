/* W4118 grouped round robin scheduler */
/* Includes */
/*****************************************************************************/
#include "sched.h"
#include <linux/slab.h>

/* Defines */
/*****************************************************************************/
#if 1
	#define	PRINTK	trace_printk
#else
	#define PRINTK(...) do{}while(0)
#endif

#define BOOL	int
#define	M_TRUE	1
#define M_FALSE	0

/* Prototypes */
/*****************************************************************************/
#ifdef CONFIG_SMP
static int grr_load_balance(struct rq *this_rq);
static struct task_struct *pick_next_task_grr(struct rq *rq);
static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags);
static void
dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags);
#endif	/* CONFIG_SMP */

/* Global variables */
/*****************************************************************************/
DEFINE_PER_CPU(cpumask_var_t, g_grr_load_balance_tmpmask);

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

static inline int is_on_grr_rq(struct sched_grr_entity *grr_se)
{
	return !list_empty(&grr_se->m_rq_list);
}

static void grr_reset_se(struct sched_grr_entity *grr_se)
{
	grr_se->m_time_slice = M_GRR_TIMESLICE;
	grr_se->m_is_timeup = M_FALSE;
}

/* Not necessary - just leave this */
static void grr_set_running_cpu(struct sched_grr_entity *grr_se, struct rq *rq)
{
#ifdef CONFIG_SMP
	grr_se->m_cpu_history |= (1 << rq->cpu);
#endif
}

static void grr_lock(struct grr_rq *p_grr_rq)
{
	struct raw_spinlock *p_lock = &(p_grr_rq->m_runtime_lock);
	raw_spin_lock(p_lock);
}

static void grr_unlock(struct grr_rq *p_grr_rq)
{
	struct raw_spinlock *p_lock = &(p_grr_rq->m_runtime_lock);
	raw_spin_unlock(p_lock);
}

void free_grr_sched_group(struct task_group *tg)
{
#ifdef CONFIG_SMP
	int i;

	for_each_possible_cpu(i) {
		if (tg->grr_rq)
			kfree(tg->grr_rq[i]);
		if (tg->grr_se)
			kfree(tg->grr_se[i]);
	}

	kfree(tg->grr_rq);
	kfree(tg->grr_se);
#endif
}

#ifdef CONFIG_SMP
void init_tg_grr_entry(struct task_group *tg, struct grr_rq *grr_rq,
		struct sched_grr_entity *grr_se, int cpu,
		struct sched_grr_entity *parent)
{
	struct rq *rq = cpu_rq(cpu);

	/* Set up info of GRR rq for the TG on this CPU */
	grr_rq->rq = rq;
	grr_rq->tg = tg;

	/* Attach GRR rq and se to the TG for this CPU */
	tg->grr_rq[cpu] = grr_rq;
	tg->grr_se[cpu] = grr_se;

	if (!grr_se)
		return;

	/* Initialze or inherit GRR rq from rq or parent */
	if (!parent)
		grr_se->grr_rq = &rq->grr;
	else
		grr_se->grr_rq = parent->my_q;

	grr_se->my_q = grr_rq;
	grr_se->parent = parent;
	INIT_LIST_HEAD(&grr_se->m_rq_list);
}

int alloc_grr_sched_group(
		struct task_group *tg, struct task_group *parent)
{
	struct grr_rq *grr_rq;
	struct sched_grr_entity *grr_se;
	int i;

	tg->grr_rq = kzalloc(sizeof(grr_rq) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->grr_rq)
		goto err;
	tg->grr_se = kzalloc(sizeof(grr_se) * nr_cpu_ids, GFP_KERNEL);
	if (!tg->grr_se)
		goto err;

	for_each_possible_cpu(i) {
		grr_rq = kzalloc_node(sizeof(struct grr_rq),
				     GFP_KERNEL, cpu_to_node(i));
		if (!grr_rq)
			goto err;

		grr_se = kzalloc_node(sizeof(struct sched_grr_entity),
				     GFP_KERNEL, cpu_to_node(i));
		if (!grr_se)
			goto err_free_rq;

		init_grr_rq(grr_rq, cpu_rq(i));
		init_tg_grr_entry(tg, grr_rq, grr_se, i, parent->grr_se[i]);
	}

	return 1;

err_free_rq:
	kfree(grr_rq);
err:
	return 0;
}
#else
void init_tg_grr_entry(struct task_group *tg, struct grr_rq *grr_rq,
		struct sched_grr_entity *grr_se, int cpu,
		struct sched_grr_entity *parent)
{
}

int alloc_grr_sched_group(
		struct task_group *tg, struct task_group *parent)
{
	return 1;
}
#endif

/* SMP Load balancing things */
/*****************************************************************************/
#ifdef CONFIG_SMP
/*
* Get the queue with the highest total number of tasks
* Bo: Need to consider find queue only within a group later
*/
static struct rq * grr_find_busiest_queue(const struct cpumask *cpus)
{
	struct rq *busiest = NULL;
	struct rq *rq;
	unsigned long max_load = 0;
	int i;

#if 0
    /* case for handle group */
#else
	for_each_cpu(i, cpus) {
		unsigned long curr_load;

		if (!cpumask_test_cpu(i, cpus))
			continue;

		rq = cpu_rq(i);
		curr_load = rq->grr.m_nr_running;

		if (curr_load > max_load) {
			max_load = curr_load;
			busiest = rq;
		}
	}
#endif
	return busiest;
}

/*
* Get the queue with the lowest total number of tasks
* Bo: Need to consider find queue only within a group later
*/
static struct rq * grr_find_least_busiest_queue(const struct cpumask *cpus)
{
	struct rq *least_busiest = NULL;
	struct rq *rq;

	unsigned long min_load = 0xffffffff;
	int i;

#if 0
    /* case for handle group */
#else
	for_each_cpu(i, cpus) {
		unsigned long curr_load;

		if (!cpumask_test_cpu(i, cpus))
			continue;

		rq = cpu_rq(i);
		curr_load = rq->grr.m_nr_running;

		if (curr_load < min_load) {
			min_load = curr_load;
			least_busiest = rq;
		}
	}
#endif
	return least_busiest;
}

/*
* Whenever, it is time to do load balance, this function will be called.
* The fuction will get the busiest queue's next eligble task,
* and put it into least busiest queue.
* Bo: 
* Ignore idle CPU to steal task from other CPU. 
* Ignore group concept.
*/

static int grr_load_balance(struct rq *this_rq)
{
	struct rq *busiest_rq;
    	struct rq *target_rq;
    	struct task_struct *busiest_rq_task;
	struct cpumask *cpus = __get_cpu_var(g_grr_load_balance_tmpmask);
    	BOOL is_task_moved = M_FALSE;
	int nr_busiest = 0, nr_target = 0;	
	unsigned long flags;

	cpumask_copy(cpus, cpu_active_mask);

	/* @lfred: why lock ? */
    	//grr_lock(&this_rq->grr);

	target_rq = grr_find_least_busiest_queue(cpus);
	busiest_rq = grr_find_busiest_queue(cpus);
	if (target_rq == NULL || busiest_rq == NULL)
		goto __do_nothing__;
	
	/* @lfred: if I am not the busiest, just go away. */
	if (busiest_rq != this_rq)
		goto __do_nothing__;

	if (busiest_rq == target_rq)
		goto __do_nothing__;

	/* get least and most busiest queue */
	PRINTK("I am doing load balancing0!!\n");
	
	/*********************************************************************/
	local_irq_save(flags);
	double_rq_lock(busiest_rq, target_rq);

	nr_busiest = busiest_rq->grr.m_nr_running;	
	nr_target = target_rq->grr.m_nr_running;
	PRINTK("nr_busiest:%d !!\n",nr_busiest);
	PRINTK("nr_target:%d !!\n",nr_target);
    	
	/* make sure load balance will not reverse */
    	if (nr_busiest > 1 && nr_target + 1 < nr_busiest) {
		/* Here, we will do task moving */
		PRINTK("I am doing load balancing1!!\n");
		busiest_rq_task = pick_next_task_grr(busiest_rq);
		dequeue_task_grr(busiest_rq, busiest_rq_task, 1);
		enqueue_task_grr(target_rq, busiest_rq_task, 1);
		PRINTK("I am doing load balancing2!!\n");
	
		/* lock both RQs */
		/* step 1: pick one task in the busiest rq	*/
		/* step 2: test is_allowed_on_target_cpu 	*/
		/* step 3: if step 2 is false, go to step 1.	*/
		/* step 4: do the migration 			*/ 
		/* unlock both RQs */

		is_task_moved = M_TRUE;
        
		/* unlock queues locked in find fucntions */ 
		//grr_unlock(&busiest_rq->grr);
        	//grr_unlock(&target_rq->grr);
    	}

    	/* unlock this queue locked at first place */ 
    	//grr_unlock(&this_rq->grr);
	PRINTK("I am doing load balancing 3!!\n");
	double_rq_unlock(busiest_rq, target_rq);
	local_irq_restore(flags);
	PRINTK("I am doing load balancing 4!!\n");

__do_nothing__:
    	return is_task_moved;
}

/* This function is used to test if the destination CPU is allowed */
BOOL is_allowed_on_target_cpu(struct task_struct *p, int target_cpu)
{
	if ((p->grr.m_cpu_history & (1 << target_cpu)) > 0)
		return M_FALSE;
	else
		return M_TRUE;
}

#endif /* CONFIG_SMP */

/* scheduler class functions */
/*****************************************************************************/
/* init func: 
 *	For each cpu, it will be called once. Thus, the rq is a PER_CPU data 
 *	structure.
 */
void init_grr_rq(struct grr_rq *grr_rq, struct rq *rq) 
{
	grr_rq->mp_rq = rq;
	grr_rq->m_nr_running = 0;
	grr_rq->m_rebalance_cnt = M_GRR_REBALANCE;
	INIT_LIST_HEAD(&grr_rq->m_task_q);
	raw_spin_lock_init(&grr_rq->m_runtime_lock);
}

#ifdef CONFIG_SMP
static void pre_schedule_grr(struct rq *rq, struct task_struct *prev)
{
	/* handle the case when rebalance is on */
        if (rq->grr.m_need_balance) {
		
		PRINTK("I am doing pre_schedule_grr\n");
                
		/* reset the rq variable */
		rq->grr.m_need_balance = M_FALSE;
		rq->grr.m_rebalance_cnt = M_GRR_REBALANCE;

#if 1
                /* take care of the rebalance here */
                grr_load_balance(rq);
#endif        
	}
}

static int
select_task_rq_grr(struct task_struct *p, int sd_flag, int flags)
{	
	return task_cpu(p);
}

#endif /* CONFIG_SMP */

/*
 * The enqueue_task method is called before nr_running is
 * increased. Here we update the fair scheduling stats and
 * then put the task into the rbtree:
 */
static void
enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags)
{
	grr_reset_se(&(p->grr));
	INIT_LIST_HEAD(&(p->grr.m_rq_list));

	/* critical section */
	grr_lock(&rq->grr);
	
	list_add_tail(&(p->grr.m_rq_list), &(rq->grr.m_task_q));
	rq->grr.m_nr_running++;	

	grr_unlock(&rq->grr);
	/* out of critical section */	

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
	/* critical section */
	grr_lock(&rq->grr);

	list_del_init(&(p->grr.m_rq_list));
	rq->grr.m_nr_running--;	

	grr_unlock(&rq->grr);
	/* out of critical section */	
	
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
#if 1
	/* if the current task is my, put it in the end of queue */
	if (rq->grr.m_nr_running != 1) {
	#if 0
		raw_spin_lock(p_lock);	
		list_del_init(&(rq->curr->grr.m_task_list));
		list_add_tail(	
	#endif
	}	
#else
	requeue_task_grr(rq, rq->curr, 0);
#endif
}

/*
 * Preempt the current task with a newly woken task if needed:
 */
static void 
check_preempt_curr_grr(struct rq *rq, struct task_struct *p, int flags)
{
#if 0
	if (	rq->curr->sched_class != &grr_sched_class && 
		p->sched_class == &grr_sched_class)
		resched_task(p);
#endif
}

/*
 * return the next task to run: select a task in my run queue if there is any
 * check pick_next_task @ core.c
 *
 * Load Balancing: reference 'calc_load_account_idle'
 */
static struct task_struct *pick_next_task_grr(struct rq *rq)
{
	struct task_struct *p = NULL;

	if (!rq->nr_running)
		return NULL;

	/* critical section */
	grr_lock(&rq->grr);

	p = rq->curr;
	
	if (rq->grr.m_nr_running > 0) {	

		/* when the timer interrupt says -> your time is up! */
		if (p->sched_class == &grr_sched_class && p->grr.m_is_timeup) {
			list_move_tail(&(p->grr.m_rq_list),
					&(rq->grr.m_task_q));
			grr_reset_se(&p->grr);
		}

		/* pick up the 1st one in the RQ */
		p = task_of_se(
			list_first_entry(
				&(rq->grr.m_task_q), 
				struct sched_grr_entity, 
				m_rq_list));   
	
		/* reset the running vars */	
		grr_reset_se(&(p->grr));

		/* record that the task has been run in the current cpu */
		grr_set_running_cpu (&(p->grr), rq);
	}

	grr_unlock(&rq->grr);
	/* out of critical section */
	
	return p; 
}

/*
 * Account for a descheduled task:
 *	When the current task is about to be moved out from
 *	CPU, this function will be called to allow the scheduler to
 *	update the data structure.
 */
static void put_prev_task_grr(struct rq *rq, struct task_struct *prev)
{
	struct list_head *taskq = &(rq->grr.m_task_q);
	struct list_head *t = &(prev->grr.m_rq_list);
//	struct list_head *pos = NULL;
	
	/* check if it is GRR class */
	if (prev->sched_class != &grr_sched_class)
		return;
	
	/* critical section */
	grr_lock(&rq->grr);
	
	/* 
		traverse the list and try to find the task
	  	The problem here is that the prev task may not be the one 
		handled by GRR policy
	*//*
	list_for_each(pos, taskq) {
		if (pos == t) {
			list_del_init(t);
			list_add_tail(t, taskq);
			break;
		}
	}*/
	if (is_on_grr_rq(&prev->grr))
		list_move_tail(t, taskq);

	grr_unlock(&rq->grr);
	/* out of critical section */
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
	BOOL need_resched = M_FALSE;

#ifdef CONFIG_SMP
	/* Update statistics */
	if ((--rq->grr.m_rebalance_cnt) == 0) {
		/* set flag for rebalanceing & set resched*/
		rq->grr.m_rebalance_cnt = M_GRR_REBALANCE;
		rq->grr.m_need_balance = M_TRUE;
		need_resched = M_TRUE;
	}
#endif

	if (curr->policy != SCHED_GRR)
		goto __grr_tick_end__;
	
	/* @lfred:
		not sure if there is a chance that tick twice 
		before you schedule. We take it conservatively.
	*/
	if (se->m_is_timeup == M_FALSE && se->m_time_slice > 0) { 
		se->m_time_slice--;
		goto __grr_tick_end__;
	}
		
	/* the running task is expired. */
	/* reset the time slice variable */
	se->m_time_slice = M_GRR_TIMESLICE;

	if (rq->grr.m_nr_running > 1) {
		/* Time up for the current entity */
		/* put the current task to the end of the list */
		need_resched = M_TRUE;
		
		/* if there is more than one task, we set time is up */
		se->m_is_timeup = M_TRUE;
	}
 
__grr_tick_end__:
	if (need_resched)
		set_tsk_need_resched(curr);

	return;
}

/* called when task is forked. */
void task_fork_grr (struct task_struct *p) {

	/* reset grr related fields */
	grr_reset_se(&(p->grr));
	p->grr.m_cpu_history = 0;
}

/* Account for a task changing its policy or group.
 *
 * This routine is mostly called to set cfs_rq->curr field when a task
 * migrates between groups/classes.
 */
static void set_curr_task_grr(struct rq *rq)
{
	/* add to the queue, and increment the count */	
}

/*
 * We switched to the sched_grr class.
 * @lfred: this is MUST for testing. 
 * The current design is we reset everything after you switch to GRR 
 * Since we are not able to track everything after not with-in our
 * control, we treat this task as a newly forked task.
 */
static void switched_to_grr(struct rq *rq, struct task_struct *p)
{
	/* reset grr related fields */
        grr_reset_se(&(p->grr));
        p->grr.m_cpu_history = 0;
	return;
}

/*
 * Priority of the task has changed. Check to see if we preempt
 * the current task.
 * @lfred: We dont really care about priorities. Dont even pretend
 * we care.
 */
static void
prio_changed_grr(struct rq *rq, struct task_struct *p, int oldprio)
{
	return;
}

/* return the time slice for the task -> in GRR, everybody's the same */
/* the jiffie = 100 HZ / 10  */
static unsigned int get_rr_interval_grr(struct rq *rq, struct task_struct *task)
{
	return (unsigned int)(HZ/10);
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
	.pre_schedule		= pre_schedule_grr,
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
	.task_fork		= task_fork_grr,

	/* void (*switched_from) (struct rq *this_rq, struct task_struct *task); */
	.switched_to		= switched_to_grr,
	
	.prio_changed		= prio_changed_grr,
	.get_rr_interval	= get_rr_interval_grr,

#ifdef CONFIG_FAIR_GROUP_SCHED
	/* void (*task_move_group) (struct task_struct *p, int on_rq); */
#endif
};
