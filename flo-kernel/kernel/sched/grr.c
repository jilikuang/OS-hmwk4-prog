
/* W4118 grouped round robin scheduler */
/* Includes */
/*****************************************************************************/
#include "sched.h"
#include <linux/slab.h>
#include <linux/nmi.h>

/* Defines */
/*****************************************************************************/
#if 1
	#define	PRINTK	trace_printk
#else
	#define PRINTK(...) do{}while(0)
#endif

#define GRR_DEBUG
#define BOOL	int
#define	M_TRUE	1
#define M_FALSE	0

#define check_rq_size(rq) \
		check_rq_size_func(rq,__func__,__LINE__)
/* Prototypes */
/*****************************************************************************/
#ifdef CONFIG_SMP
static int grr_load_balance(struct rq *this_rq);
static struct task_struct *pick_next_task_grr(struct rq *rq);
static void enqueue_task_grr(struct rq *rq, struct task_struct *p, int flags);
static void dequeue_task_grr(struct rq *rq, struct task_struct *p, int flags);
#endif	/* CONFIG_SMP */

/* Global variables */
/*****************************************************************************/
DEFINE_PER_CPU(cpumask_var_t, g_grr_load_balance_tmpmask);

/* Utility functions */
/*****************************************************************************/
static inline int count_rq_size(struct rq *rq)
{
	int cnt = 0;

#ifdef GRR_DEBUG
	struct list_head *pos = NULL;
	list_for_each(pos, &(rq->grr.m_task_q)) {
		cnt++;
	}
#endif

	return cnt;
}

/* debugging function: check the grr.m_nr_running is correct */
static inline BOOL check_rq_size_func(struct rq *rq, char const* func, int ln)
{
#ifdef GRR_DEBUG
	if (count_rq_size(rq) == rq->grr.m_nr_running) {
		printk("@lfred: check_rq_size passed @ %s:%d\n", func, ln);
		return M_TRUE;
	} else {
		printk("@lfred: check_rq_size failed @ %s:%d\n", func, ln);
		BUG();
		return M_FALSE;
	}
#else
	return M_TRUE;
#endif
}

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
	int i;

	for_each_possible_cpu(i) {
		if (tg->grr_rq)
			kfree(tg->grr_rq[i]);
		if (tg->grr_se)
			kfree(tg->grr_se[i]);
	}

	kfree(tg->grr_rq);
	kfree(tg->grr_se);
}

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

static int task_is_valid_on_cpu(struct task_struct *p, int cpu)
{
	int retval = 1;
#ifdef CONFIG_SMP
	read_lock(&cpu_grp.lock);
	if (is_tg_bg(task_group(p))) {
		if (cpu < cpu_grp.bg_cpu_start)
			retval = 0;
	} else if (is_tg_sys(task_group(p)) || is_tg_fg(task_group(p))) {
		if (cpu > cpu_grp.fg_cpu_end)
			retval = 0;
	}
	read_unlock(&cpu_grp.lock);
#endif
	return retval;
}

struct list_head grr_grp_mq;
DEFINE_SPINLOCK(grr_grp_mq_lock);

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

	unsigned long min_load = 0xdeadbeef;
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

#if 0
/* TODO */
/* implement the check if we need to rebalance */
static BOOL can_we_balance_on_the_cpu(struct sched_group *sg, int cpu)
{
	struct cpumask *sg_cpus, *sg_mask;
	int cpu, balance_cpu = -1;

	sg_cpus = sched_group_cpus(sg);
	sg_mask = sched_group_mask(sg);

	/* Try to find first idle cpu */
	if (cpumask_test_cpu(cpu, sg_mask))
		return M_TRUE;
         
	return M_FALSE;
}
#endif

/* TODO */
/* pick one of the eligible task from the source q to move */
static struct task_struct *pick_eligible_task(
	struct rq *src_rq,
	struct rq *dst_rq) 
{
	struct list_head *pos = NULL;
	struct task_struct *tsk = NULL;
	int dst_cpu = dst_rq->cpu;

	list_for_each(pos, &(src_rq->grr.m_task_q)) {
		
		tsk = task_of_se(container_of(pos, struct sched_grr_entity, m_rq_list));

		if (tsk == src_rq->curr || tsk->policy != 6)
			continue;	
	
		if (cpumask_test_cpu(dst_cpu, &(tsk->cpus_allowed))) {
 			printk ("@lfred: tsk %d is selected\n", tsk->pid);
			return tsk;
		}
	}

	printk ("@lfred: no tasks is allowed to migrate");
	return NULL;
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
	struct list_head *tlist = NULL;

	cpumask_copy(cpus, cpu_active_mask);

	if (!rcu_dereference_sched(this_rq->sd)) {
		printk("@lfred: zero domain, no load offloading\n");
		return is_task_moved;
	}
	
	target_rq = grr_find_least_busiest_queue(cpus);
	busiest_rq = grr_find_busiest_queue(cpus);
	
	if (target_rq == NULL || busiest_rq == NULL)
		return is_task_moved;
	
	printk("@lfred: doing load_balance @ cpu %d\n", this_rq->cpu);
	/*********************************************************************/
	double_lock_balance(busiest_rq, target_rq);
	
	/* @lfred: if I am not the busiest, just go away. */
	if (busiest_rq != this_rq)
		goto __do_nothing__;

	if (busiest_rq->grr.m_nr_running == target_rq->grr.m_nr_running)
		goto __do_nothing__;
	
	nr_busiest = busiest_rq->grr.m_nr_running;	
	nr_target = target_rq->grr.m_nr_running;
	
	/* make sure load balance will not reverse */
    	if (nr_busiest > 1 && nr_busiest - nr_target > 1) {
		
		/* Here, we will do task moving */
		busiest_rq_task = pick_eligible_task(busiest_rq, target_rq);

		if (busiest_rq_task == NULL)
			goto __do_nothing__;		

		tlist = &(busiest_rq_task->grr.m_rq_list);

		/* dequeue */
		list_del(tlist);
		busiest_rq->grr.m_nr_running--;	
		dec_nr_running(busiest_rq);

		/* enqueue */
		list_add_tail(tlist, &(target_rq->grr.m_task_q));
		target_rq->grr.m_nr_running++;	
		inc_nr_running(target_rq);	

		/* moving a task */
		set_task_cpu(busiest_rq_task, target_rq->cpu);
		check_preempt_curr(target_rq, busiest_rq_task, 0);
				
		/* return true flag */
		is_task_moved = M_TRUE;    
	}

__do_nothing__:
	double_unlock_balance(busiest_rq, target_rq);
	printk("@lfred: complete load_balance @ cpu %d\n", this_rq->cpu);
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
	grr_rq->m_rebalance_cnt = M_GRR_INIT_REBALANCE;
	INIT_LIST_HEAD(&grr_rq->m_task_q);
	raw_spin_lock_init(&grr_rq->m_runtime_lock);
}

#ifdef CONFIG_SMP
static void pre_schedule_grr(struct rq *rq, struct task_struct *prev)
{
	/* handle the case when rebalance is on */
        if (rq->grr.m_need_balance) {
                
		/* reset the rq variable */
		rq->grr.m_need_balance = M_FALSE;
		rq->grr.m_rebalance_cnt = M_GRR_REBALANCE;

                /* take care of the rebalance here */
                grr_load_balance(rq);
	}
}

static int
select_task_rq_grr(struct task_struct *p, int sd_flag, int flags)
{
        int cpu_picked = task_cpu(p);
        unsigned long min_load = 0xdeadbeef;
        struct rq* rq;
        int i;
        struct cpumask *cpus = __get_cpu_var(g_grr_load_balance_tmpmask);
        cpumask_copy(cpus, cpu_active_mask);

#if 0
    /* case for handle group */
#else
        for_each_cpu(i, cpus) {
                unsigned long curr_load;

                if (!cpumask_test_cpu(i, cpus))
                        continue;

                rq = cpu_rq(i);
                curr_load = rq->grr.m_nr_running;

                if (curr_load < min_load && task_is_valid_on_cpu(p, i)){
                        min_load = curr_load;
                        cpu_picked = i;
                }
        }
#endif
        return cpu_picked;
}

/* TODO: should we manage the re-schedule? */
static void
set_cpus_allowed_grr(struct task_struct *t, const struct cpumask *mask)
{
#if 0
	struct cpumask dstp;

	cpumask_and(&dstp, &(t->cpus_allowed), mask);

	if (cpumask_first(&dstp) == 0) {
		/* Fucka - you got no CPU to run */
		/* No where to move !? */
		BUG();
	} else {
		/* We have CPU to run.  */
	}
#endif
}
 
static void task_waking_grr(struct task_struct *p)
{
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

	check_rq_size(rq);

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

	check_rq_size(rq);
	
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

	check_rq_size(rq);
	
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

	check_rq_size(rq);
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
	if (is_on_grr_rq(&prev->grr)) {
#if 0
		if (!task_is_valid_on_cpu(prev, rq->cpu)) {
			struct rq *target_rq;
			/*TPRINTK("remove %d from %d to mq\n",
					prev->pid,
					rq->cpu);*/
			//dequeue_task_grr(rq, prev, 0);
			deactivate_task(rq, prev, 0);/*
			spin_lock(&grr_grp_mq_lock);
			list_add_tail(t, &grr_grp_mq);
			spin_unlock(&grr_grp_mq_lock);*/
			if (!is_tg_bg(task_group(prev))) {
				target_rq = cpu_rq(0);
			} else {
				target_rq = cpu_rq(nr_cpu_ids - 1);
			}
			double_lock_balance(rq, target_rq);
			set_task_rq(prev, target_rq->cpu);
			activate_task(target_rq, prev, 0);
			wake_up_idle_cpu(target_rq->cpu);
			double_unlock_balance(rq, target_rq);
		} else {
			list_move_tail(t, taskq);
		}
#else
		list_move_tail(t, taskq);
#endif
	}

	check_rq_size(rq);
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
	
	/* if not my task, we have to go away */
	if (curr->policy != SCHED_GRR)
		goto __grr_tick_end__;

#ifdef CONFIG_SMP
	/* Update statistics */
	if ((--rq->grr.m_rebalance_cnt) == 0) {
		/* set flag for rebalanceing & set resched*/
		rq->grr.m_rebalance_cnt = M_GRR_REBALANCE;
		rq->grr.m_need_balance = M_TRUE;
		need_resched = M_TRUE;
	}
#endif
	
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

	check_rq_size(rq);
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
	check_rq_size(rq);
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
	check_rq_size(rq);
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
	check_rq_size(rq);
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
	.set_cpus_allowed	= set_cpus_allowed_grr, 

	.task_waking		= task_waking_grr,
#if 0
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
	.task_move_group	= NULL,
	/* void (*task_move_group) (struct task_struct *p, int on_rq); */
#endif
};
