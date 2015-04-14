/*
 *  drivers/power/system_load_analyzer.c
 *
 *  Copyright (C)  2011 Samsung Electronics co. ltd
 *    Yong-U Baek <yu.baek@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/reboot.h>

#include <plat/cpu.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>

#include <linux/load_analyzer.h>

#if defined(CONFIG_SLP_MINI_TRACER)
#include <asm/cacheflush.h>
#include <mach/sec_debug.h>
#endif

#include <linux/vmalloc.h>
#include <linux/cpuidle.h>

#define CONFIG_SLP_CHECK_BUS_LOAD	1

#if defined(CONFIG_SLP_CHECK_BUS_LOAD) && defined(CONFIG_CLK_MON)
#define CONFIG_SLP_BUS_CLK_CHECK_LOAD 1
#include <linux/clk_mon.h>
#endif
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define CPU_NUM	NR_CPUS

#if defined(CONFIG_SLP_MINI_TRACER)
/*
 * TIZEN kernel trace queue system
*/

#define KENEL_TRACER_QUEUE_SIZE (32 * 1024)
#define MINI_TRACER_MAX_SIZE_STRING 512
atomic_t kenel_tracer_queue_cnt = ATOMIC_INIT(0);
static bool kernel_mini_trace_enable;
static char *kernel_tracer_queue;
int kernel_mini_tracer_i2c_log_on;

static char * kernel_mini_tracer_get_addr(void)
{
	unsigned int cnt;
	cnt = atomic_read(&kenel_tracer_queue_cnt);

	return (kernel_tracer_queue + cnt);
}

static void kernel_mini_tracer_char(char character)
{
	unsigned int cnt;

	atomic_inc(&kenel_tracer_queue_cnt);
	cnt = atomic_read(&kenel_tracer_queue_cnt);

	if (cnt >= KENEL_TRACER_QUEUE_SIZE) {
		atomic_set(&kenel_tracer_queue_cnt, 0);
		cnt = 0;
	}

	kernel_tracer_queue[cnt] = character;
}

static void kenel_mini_tracer_get_time(char *time_str)
{
	unsigned long long t;
	unsigned long  nanosec_rem;

	t = cpu_clock(UINT_MAX);
	nanosec_rem = do_div(t, 1000000000);
	sprintf(time_str, "%2lu.%04lu"
				, (unsigned long) t, nanosec_rem / 100000);
}

static int kernel_mini_tracer_saving_str(char *input_string)
{
	unsigned int  max_size_of_tracer = 1024;
	unsigned int tracer_cnt = 0;

	while ((*input_string) != 0 &&  tracer_cnt < max_size_of_tracer) {
		kernel_mini_tracer_char(*(input_string++));
		tracer_cnt++;
	}

	return tracer_cnt;

}

void kernel_mini_tracer(char *input_string, int option)
{
	char time_str[32];
	char *start_addr, *end_addr;
	int saved_char_cnt = 0;

	if (kernel_mini_trace_enable == 0)
		return;

	start_addr = kernel_mini_tracer_get_addr() +1;

	if ((option & TIME_ON) == TIME_ON) {
		saved_char_cnt += kernel_mini_tracer_saving_str("[");
		kenel_mini_tracer_get_time(time_str);
		saved_char_cnt += kernel_mini_tracer_saving_str(time_str);
		saved_char_cnt += kernel_mini_tracer_saving_str("] ");
	}

	saved_char_cnt += kernel_mini_tracer_saving_str(input_string);

	end_addr = kernel_mini_tracer_get_addr();

	if ((option & FLUSH_CACHE) == FLUSH_CACHE) {
		if (end_addr > start_addr)
			clean_dcache_area(start_addr, saved_char_cnt);
		else
			clean_dcache_area(kernel_tracer_queue, KENEL_TRACER_QUEUE_SIZE);
		#if defined (CONFIG_SEC_DEBUG_SCHED_LOG)
			sched_log_clean_cache();
		#endif
	}
}

void kernel_mini_tracer_smp(char *input_string)
{
	unsigned int this_cpu;

	this_cpu = raw_smp_processor_id();

	if (this_cpu == 0)
		kernel_mini_tracer("[C0] ", TIME_ON);
	else if (this_cpu == 1)
		kernel_mini_tracer("[C1] ", TIME_ON);
	else if (this_cpu == 2)
		kernel_mini_tracer("[C2] ", TIME_ON);
	else if (this_cpu == 3)
		kernel_mini_tracer("[C3] ", TIME_ON);

	kernel_mini_tracer(input_string, 0);


}
static int kernel_mini_tracer_init(void)
{
	kernel_tracer_queue = (char *)__get_free_pages(GFP_KERNEL
					, get_order(KENEL_TRACER_QUEUE_SIZE));
	if (kernel_tracer_queue == NULL)
		return -ENOMEM;

	memset(kernel_tracer_queue, 0, KENEL_TRACER_QUEUE_SIZE);
	kernel_mini_trace_enable = 1;

	return 0;
}

static int kernel_mini_tracer_exit(void)
{
	kernel_mini_trace_enable = 0;
	if (kernel_tracer_queue != NULL)  {
		__free_pages(virt_to_page(kernel_tracer_queue)
				, get_order(KENEL_TRACER_QUEUE_SIZE));
	}

	return 0;
}

#endif



static int get_index(int cnt, int ring_size, int diff)
{
	int ret = 0, modified_diff;

	if ((diff > ring_size) || (diff * (-1) > ring_size))
		modified_diff = diff % ring_size;
	else
		modified_diff = diff;

	ret = (ring_size + cnt + modified_diff) % ring_size;

	return ret;
}

static int wrapper_for_debug_fs(char __user *buffer, size_t count, loff_t *ppos, int (*fn)(char *, int))
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
	unsigned int ret = 0, size_for_copy = count;
	static unsigned int rest_size = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		ret = fn(buf, buf_size);

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	} else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		vfree(buf);

	return size_for_copy;
}

unsigned int cpu_load_history_num = 2000;

struct cpu_load_freq_history_tag {
	char time[16];
	unsigned long long time_stamp;
	unsigned int cpufreq;
	int cpu_max_locked_freq;
	int cpu_min_locked_freq;
	unsigned int cpu_load[CPU_NUM];
	unsigned int touch_event;
	unsigned int nr_onlinecpu;
	unsigned int nr_run_avg;
	unsigned int task_history_cnt[CPU_NUM];
	unsigned int work_history_cnt[CPU_NUM];

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
	unsigned int ppmu_cpu_load;
	unsigned int cpu_load_slope;
	unsigned int ppmu_dmc0_load;
	unsigned int ppmu_dmc1_load;

	unsigned int ppmu_bus_mif_freq;
	unsigned int ppmu_bus_int_freq;
	unsigned int ppmu_bus_mif_load;
	unsigned int ppmu_bus_int_load;

	unsigned int bus_freq;
	unsigned int bus_locked_freq;
	unsigned int bus_freq_cause;
	unsigned int gpu_freq;
	unsigned int gpu_utilization;
#endif
#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
	unsigned int power_domains;
	unsigned int clk_gates[CLK_GATES_NUM];
#endif

};

static struct cpu_load_freq_history_tag *cpu_load_freq_history;
static struct cpu_load_freq_history_tag *cpu_load_freq_history_view;
#define CPU_LOAD_FREQ_HISTORY_SIZE	\
	(sizeof(struct cpu_load_freq_history_tag) * cpu_load_history_num)

static unsigned int  cpu_load_freq_history_cnt;
static unsigned int  cpu_load_freq_history_view_cnt;

static  int  cpu_load_freq_history_show_cnt = 100;
#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
#define CPU_BUS_VIEW_DEFAULT_VALUE	100
static  int  cpu_bus_load_freq_history_show_cnt = CPU_BUS_VIEW_DEFAULT_VALUE;
#endif
#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
static  int  cpu_bus_clk_load_freq_history_show_cnt = 100;
#endif
struct cpu_process_runtime_tag temp_process_runtime;

#define CPU_TASK_HISTORY_NUM 30000
unsigned int cpu_task_history_num = CPU_TASK_HISTORY_NUM;

struct cpu_task_history_tag {
	unsigned long long time;
	struct task_struct *task;
	unsigned int pid;
};
static struct cpu_task_history_tag (*cpu_task_history)[CPU_NUM];
static struct cpu_task_history_tag	 (*cpu_task_history_view)[CPU_NUM];
#define CPU_TASK_HISTORY_SIZE	(sizeof(struct cpu_task_history_tag) \
					* cpu_task_history_num * CPU_NUM)

bool cpu_task_history_onoff;

struct list_head process_headlist;

static unsigned int  cpu_task_history_cnt[CPU_NUM];
static unsigned int  cpu_task_history_show_start_cnt;
static unsigned int  cpu_task_history_show_end_cnt;
static  int  cpu_task_history_show_select_cpu;

static unsigned long long  total_time, section_start_time, section_end_time;

void __slp_store_task_history(unsigned int cpu, struct task_struct *task)
{
	unsigned int cnt;

	if (cpu_task_history_onoff == 0)
		return ;

	if (++cpu_task_history_cnt[cpu] >= cpu_task_history_num)
		cpu_task_history_cnt[cpu] = 0;
	cnt = cpu_task_history_cnt[cpu];

	cpu_task_history[cnt][cpu].time = cpu_clock(UINT_MAX);
	cpu_task_history[cnt][cpu].task = task;
	cpu_task_history[cnt][cpu].pid = task->pid;
}




#define CPU_WORK_HISTORY_NUM 10000
unsigned int cpu_work_history_num = CPU_WORK_HISTORY_NUM;
static unsigned int  cpu_work_history_cnt[CPU_NUM];
static unsigned int  cpu_work_history_show_start_cnt;
static unsigned int  cpu_work_history_show_end_cnt;
static  int  cpu_work_history_show_select_cpu;

static struct list_head work_headlist;

struct cpu_work_history_tag {
	u64 start_time;
	u64 end_time;
	u64 occup_time;

	struct task_struct *task;
	unsigned int pid;
	struct work_struct *work;
	work_func_t	func;
};
static struct cpu_work_history_tag (*cpu_work_history)[CPU_NUM];
static struct cpu_work_history_tag	 (*cpu_work_history_view)[CPU_NUM];
#define CPU_WORK_HISTORY_SIZE	(sizeof(struct cpu_work_history_tag) \
					* cpu_work_history_num * CPU_NUM)

bool cpu_work_history_onoff;

u64  get_load_analyzer_time(void)
{
	return cpu_clock(UINT_MAX);
}

void __slp_store_work_history(struct work_struct *work, work_func_t func
						, u64 start_time, u64 end_time)
{
	unsigned int cnt, cpu;
	struct task_struct *task;

	if (cpu_work_history_onoff == 0)
		return ;

	cpu = raw_smp_processor_id();
	task = current;

	if (++cpu_work_history_cnt[cpu] >= cpu_work_history_num)
		cpu_work_history_cnt[cpu] = 0;
	cnt = cpu_work_history_cnt[cpu];

	cpu_work_history[cnt][cpu].start_time = start_time;
	cpu_work_history[cnt][cpu].end_time = end_time;
	cpu_work_history[cnt][cpu].occup_time = end_time - start_time;

//pr_info("start_time=%lld end_time=%lld occup_time=%lld", start_time, end_time, end_time - start_time);

	cpu_work_history[cnt][cpu].task = task;
	cpu_work_history[cnt][cpu].pid = task->pid;
	cpu_work_history[cnt][cpu].work = work;
	cpu_work_history[cnt][cpu].func = func;


}

struct saved_task_info_tag {
	unsigned int pid;
	char comm[TASK_COMM_LEN];
};
#define SAVED_TASK_INFO_NUM 500
#define SAVED_TASK_INFO_SIZE (sizeof(struct saved_task_info_tag) * SAVED_TASK_INFO_NUM)
static struct saved_task_info_tag *saved_task_info;
static unsigned int  saved_task_info_cnt;


bool saved_task_info_onoff;

static int store_killed_init(void)
{
	saved_task_info = vmalloc(SAVED_TASK_INFO_SIZE);
	if (saved_task_info == NULL)
		return -ENOMEM;

	return 0;
}

static void store_killed_exit(void)
{
	if (saved_task_info != NULL)
		vfree(saved_task_info);
}


static void store_killed_memset(void)
{
	memset(saved_task_info, 0, SAVED_TASK_INFO_SIZE);
}



void store_killed_task(struct task_struct *tsk)
{
	unsigned int cnt;

	if( saved_task_info_onoff == 0)
		return;

	if (++saved_task_info_cnt >= SAVED_TASK_INFO_NUM)
		saved_task_info_cnt = 0;

	cnt = saved_task_info_cnt;

	saved_task_info[cnt].pid = tsk->pid;
	strncpy(saved_task_info[cnt].comm, tsk->comm, TASK_COMM_LEN);
}


static int search_killed_task(unsigned int pid, char *task_name)
{
	unsigned int cnt = saved_task_info_cnt, i;

	for (i = 0; i< SAVED_TASK_INFO_NUM; i++) {
		if (saved_task_info[cnt].pid == pid) {
			strncpy(task_name, saved_task_info[cnt].comm, TASK_COMM_LEN);
			break;
		}
		cnt = get_index(cnt, SAVED_TASK_INFO_NUM, -1);
	}

	if( i == SAVED_TASK_INFO_NUM)
		return -1;
	else
		return 0;
}


struct cpu_process_runtime_tag {
	struct list_head list;
	unsigned long long runtime;
	struct task_struct *task;
	unsigned int pid;
	unsigned int cnt;
	unsigned int usage;
};

struct cpu_work_runtime_tag {
	struct list_head list;
	u64 start_time;
	u64 end_time;
	u64 occup_time;

	struct task_struct *task;
	unsigned int pid;
	unsigned int cnt;

	struct work_struct *work;
	work_func_t func;
};

static unsigned long long calc_delta_time(unsigned int cpu, unsigned int index)
{
	unsigned long long run_start_time, run_end_time;

	if (index == 0) {
		run_start_time
		    = cpu_task_history_view[cpu_task_history_num-1][cpu].time;
	} else
		run_start_time = cpu_task_history_view[index-1][cpu].time;

	if (run_start_time < section_start_time)
		run_start_time = section_start_time;

	run_end_time = cpu_task_history_view[index][cpu].time;

	if (run_end_time < section_start_time)
		return 0;

	if (run_end_time > section_end_time)
		run_end_time = section_end_time;

	return  run_end_time - run_start_time;
}


static void add_process_to_list(unsigned int cpu, unsigned int index)
{
	struct cpu_process_runtime_tag *new_process;

	new_process
		= kmalloc(sizeof(struct cpu_process_runtime_tag), GFP_KERNEL);
	new_process->runtime = calc_delta_time(cpu, index);
	new_process->cnt = 1;
	new_process->task = cpu_task_history_view[index][cpu].task;
	new_process->pid = cpu_task_history_view[index][cpu].pid;

	if (new_process->runtime != 0) {
		INIT_LIST_HEAD(&new_process->list);
		list_add_tail(&new_process->list, &process_headlist);
	} else
		kfree(new_process);

	return;
}

static void add_work_to_list(unsigned int cpu, unsigned int index)
{
	struct cpu_work_runtime_tag *new_work;

	new_work
		= kmalloc(sizeof(struct cpu_work_runtime_tag), GFP_KERNEL);

	new_work->occup_time =  cpu_work_history_view[index][cpu].occup_time;

	pr_info("cpu_work_history_view[%d][%d].occup_time=%lld new_work->occup_time=%lld",
		index, cpu, cpu_work_history_view[index][cpu].occup_time ,new_work->occup_time);

	new_work->cnt = 1;
	new_work->task = cpu_work_history_view[index][cpu].task;
	new_work->pid = cpu_work_history_view[index][cpu].pid;

	new_work->work = cpu_work_history_view[index][cpu].work;
	new_work->func = cpu_work_history_view[index][cpu].func;
	pr_info("%s %d\n", __FUNCTION__, __LINE__);

	if (new_work->occup_time != 0) {
		INIT_LIST_HEAD(&new_work->list);
		list_add_tail(&new_work->list, &work_headlist);
		pr_info("%s %d\n", __FUNCTION__, __LINE__);

	} else
		kfree(new_work);

	return;
}

static void del_process_list(void)
{
	struct cpu_process_runtime_tag *curr;
	struct list_head *p;

	list_for_each_prev(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		kfree(curr);
	}
	process_headlist.prev = NULL;
	process_headlist.next = NULL;

}

static void del_work_list(void)
{
	struct cpu_work_runtime_tag *curr;
	struct list_head *p;

	list_for_each_prev(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		kfree(curr);
	}
	work_headlist.prev = NULL;
	work_headlist.next = NULL;

}

static int comp_list_runtime(struct list_head *list1, struct list_head *list2)
{
	struct cpu_process_runtime_tag *list1_struct, *list2_struct;

	int ret = 0;
	 list1_struct = list_entry(list1, struct cpu_process_runtime_tag, list);
	 list2_struct = list_entry(list2, struct cpu_process_runtime_tag, list);

	if (list1_struct->runtime > list2_struct->runtime)
		ret = 1;
	else if (list1_struct->runtime < list2_struct->runtime)
		ret = -1;
	else
		ret  = 0;

	return ret;
}

static int comp_list_occuptime(struct list_head *list1, struct list_head *list2)
{
	struct cpu_work_runtime_tag *list1_struct, *list2_struct;

	int ret = 0;
	 list1_struct = list_entry(list1, struct cpu_work_runtime_tag, list);
	 list2_struct = list_entry(list2, struct cpu_work_runtime_tag, list);

	if (list1_struct->occup_time > list2_struct->occup_time)
		ret = 1;
	else if (list1_struct->occup_time < list2_struct->occup_time)
		ret = -1;
	else
		ret  = 0;

	return ret;
}



static void swap_process_list(struct list_head *list1, struct list_head *list2)
{
	struct list_head *list1_prev, *list1_next , *list2_prev, *list2_next;

	list1_prev = list1->prev;
	list1_next = list1->next;

	list2_prev = list2->prev;
	list2_next = list2->next;

	list1->prev = list2;
	list1->next = list2_next;

	list2->prev = list1_prev;
	list2->next = list1;

	list1_prev->next = list2;
	list2_next->prev = list1;

}

static unsigned int view_list(int max_list_num, char *buf, unsigned int buf_size, unsigned int ret)
{
	struct list_head *p;
	struct cpu_process_runtime_tag *curr;
	char task_name[80] = {0,};
	char *p_name = NULL;

	unsigned int cnt = 0, list_num = 0;
	unsigned long long  t, total_time_for_clc;

	bool found_in_killed_process_list = 0;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		list_num++;
	}

	for (cnt = 0; cnt < list_num; cnt++) {
		list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
			if (p->next != &process_headlist) {
				if (comp_list_runtime(p, p->next) == -1)
					swap_process_list(p, p->next);
			}
		}
	}

	total_time_for_clc = total_time;
	do_div(total_time_for_clc, 1000);

	cnt = 1;
	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		t = curr->runtime * 100;
		do_div(t, total_time_for_clc);
		curr->usage = t + 5;

		if ((curr != NULL) && (curr->task != NULL)
			&& (curr->task->pid == curr->pid)) {
			p_name = curr->task->comm;

		} else {
			if(search_killed_task(curr->pid, task_name) >= 0) {
				found_in_killed_process_list = 1;
			} else {
				snprintf(task_name, sizeof(task_name)
							, "NOT found task");
			}
			p_name = task_name;
		}

		if (ret < buf_size - 1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
				"[%3d] %16s(%5d/%s) %6d[sched] %13lld[ns]"\
				" %3d.%02d[%%]\n"
				, cnt++, p_name, curr->pid
				, ((found_in_killed_process_list) == 1) ? "X" : "O"
				, curr->cnt, curr->runtime
				, curr->usage/1000, (curr->usage%1000) /10);
		} else
			break;

		found_in_killed_process_list = 0;

		if (cnt > max_list_num)
			break;

	}

	return ret;
}


static unsigned int view_list_raw(char *buf, unsigned int buf_size, unsigned int ret)
{
	struct list_head *p;
	struct cpu_process_runtime_tag *curr;
	char task_name[80] = {0,};
	char *p_name = NULL;

	unsigned int cnt = 0, list_num = 0;
	unsigned long long  t, total_time_for_clc;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		list_num++;
	}

	for (cnt = 0; cnt < list_num; cnt++) {
		list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
			if (p->next != &process_headlist) {
				if (comp_list_runtime(p, p->next) == -1)
					swap_process_list(p, p->next);
			}
		}
	}

	total_time_for_clc = total_time;
	do_div(total_time_for_clc, 1000);

	cnt = 1;
	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		t = curr->runtime * 100;
		do_div(t, total_time_for_clc);
		curr->usage = t + 5;

		if ((curr != NULL) && (curr->task != NULL)
			&& (curr->task->pid == curr->pid)) {
			p_name = curr->task->comm;

		} else {
			snprintf(task_name, sizeof(task_name)
						, "NOT_FOUND");
			p_name = task_name;
		}

		if (ret < buf_size - 1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
				"%d %s %d %lld %d\n"
				, curr->pid, p_name
				, curr->cnt, curr->runtime
				, curr->usage);
		} else
			break;

	}

	return ret;
}

static unsigned int view_workfn_list(char *buf,  unsigned int buf_size, unsigned int ret)
{
	struct list_head *p;
	struct cpu_work_runtime_tag *curr;
	unsigned int cnt = 0, list_num = 0;

	list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		list_num++;
	}

	for (cnt = 0; cnt < list_num; cnt++) {
		list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
			if (p->next != &work_headlist) {
				if (comp_list_occuptime(p, p->next) == -1)
					swap_process_list(p, p->next);
			}
		}
	}

	cnt = 1;
	list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		if (ret < buf_size - 1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
			"[%2d]  %25pf(%4d) \t%11lld[ns]\n"
			, cnt++, curr->func, curr->cnt ,curr->occup_time);
		}
	}

	return ret;
}

static struct cpu_process_runtime_tag *search_exist_pid(unsigned int pid)
{
	struct list_head *p;
	struct cpu_process_runtime_tag *curr;

	list_for_each(p, &process_headlist) {
		curr = list_entry(p, struct cpu_process_runtime_tag, list);
		if (curr->pid == pid)
			return curr;
	}
	return NULL;
}

static struct cpu_work_runtime_tag *search_exist_workfn(work_func_t func)
{
	struct list_head *p;
	struct cpu_work_runtime_tag *curr;

	list_for_each(p, &work_headlist) {
		curr = list_entry(p, struct cpu_work_runtime_tag, list);
		if (curr->func == func)
			return curr;
	}
	return NULL;
}



static void clc_process_run_time(unsigned int cpu
			, unsigned int start_cnt, unsigned int end_cnt)
{
	unsigned  int cnt = 0,  start_array_num;
	unsigned int end_array_num, end_array_num_plus1;
	unsigned int i, loop_cnt;
	struct cpu_process_runtime_tag *process_runtime_data;
	unsigned long long t1, t2;

	start_array_num
	    = (cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu] + 1)
		% cpu_task_history_num;

	section_start_time
		= cpu_load_freq_history_view[start_cnt].time_stamp;
	section_end_time
		= cpu_load_freq_history_view[end_cnt].time_stamp;

	end_array_num
	= cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu];
	end_array_num_plus1
	= (cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu] + 1)
			% cpu_task_history_num;


	t1 = cpu_task_history_view[end_array_num][cpu].time;
	t2 = cpu_task_history_view[end_array_num_plus1][cpu].time;

	if (t2 < t1)
		end_array_num_plus1 = end_array_num;

	total_time = section_end_time - section_start_time;

	if (process_headlist.next != NULL)
		del_process_list();

	INIT_LIST_HEAD(&process_headlist);

	if (end_array_num_plus1 >= start_array_num)
		loop_cnt = end_array_num_plus1-start_array_num + 1;
	else
		loop_cnt = end_array_num_plus1
				+ cpu_task_history_num - start_array_num + 1;

	for (i = start_array_num, cnt = 0; cnt < loop_cnt; cnt++, i++) {
		if (i >= cpu_task_history_num)
			i = 0;
		process_runtime_data
			= search_exist_pid(cpu_task_history_view[i][cpu].pid);
		if (process_runtime_data == NULL)
			add_process_to_list(cpu, i);
		else {
			process_runtime_data->runtime
				+= calc_delta_time(cpu, i);
			process_runtime_data->cnt++;
		}
	}

}

static void clc_work_run_time(unsigned int cpu
			, unsigned int start_cnt, unsigned int end_cnt)
{
	unsigned  int cnt = 0,  start_array_num;
	unsigned int end_array_num, end_array_num_plus1;
	unsigned int i, loop_cnt;
	struct cpu_work_runtime_tag *work_runtime_data;
	unsigned long long t1, t2;

	start_array_num
	    = (cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu] + 1)
		% cpu_work_history_num;

	section_start_time
		= cpu_load_freq_history_view[start_cnt].time_stamp;
	section_end_time
		= cpu_load_freq_history_view[end_cnt].time_stamp;

	end_array_num
	= cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu];
	end_array_num_plus1
	= (cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu] + 1)
			% cpu_work_history_num;

	t1 = cpu_work_history_view[end_array_num][cpu].end_time;
	t2 = cpu_work_history_view[end_array_num_plus1][cpu].end_time;

	if (t2 < t1)
		end_array_num_plus1 = end_array_num;

	total_time = section_end_time - section_start_time;

	if (work_headlist.next != NULL)
		del_work_list();

	INIT_LIST_HEAD(&work_headlist);

	if (end_array_num_plus1 >= start_array_num)
		loop_cnt = end_array_num_plus1-start_array_num + 1;
	else
		loop_cnt = end_array_num_plus1
				+ cpu_work_history_num - start_array_num + 1;

	for (i = start_array_num, cnt = 0; cnt < loop_cnt; cnt++, i++) {
		if (i >= cpu_work_history_num)
			i = 0;

		work_runtime_data = search_exist_workfn(cpu_work_history_view[i][cpu].func);
		if (work_runtime_data == NULL)
			add_work_to_list(cpu, i);
		else {
			work_runtime_data->occup_time
				+= cpu_work_history_view[i][cpu].occup_time;
			work_runtime_data->cnt++;
		}
	}

}

static unsigned int  process_sched_time_view(unsigned int cpu,
			unsigned int start_cnt, unsigned int end_cnt
			, char *buf, unsigned int buf_size, unsigned int ret)
{
	unsigned  int i = 0, start_array_num;
	unsigned int end_array_num, start_array_num_for_time;

	start_array_num_for_time
	= cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu];
	start_array_num
	= (cpu_load_freq_history_view[start_cnt].task_history_cnt[cpu]+1)
			% cpu_task_history_num;
	end_array_num
		= cpu_load_freq_history_view[end_cnt].task_history_cnt[cpu];

	total_time = section_end_time - section_start_time;

	if (end_cnt == start_cnt+1) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] TOTAL SECTION TIME = %lld[ns]\n\n"
			, end_cnt, total_time);
	} else {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d~%d] TOTAL SECTION TIME = %lld[ns]\n\n"
				, start_cnt+1, end_cnt, total_time);
	}

	end_array_num = get_index(end_array_num, cpu_task_history_num, 2);

	for (i = start_array_num_for_time; i <= end_array_num; i++) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] %lld %16s %4d\n", i
			, cpu_task_history_view[i][cpu].time
			, cpu_task_history_view[i][cpu].task->comm
			, cpu_task_history_view[i][cpu].pid);
	}

	return ret;

}


void cpu_load_touch_event(unsigned int event)
{
	unsigned int cnt = 0;

	if (cpu_task_history_onoff == 0)
		return;

	cnt = cpu_load_freq_history_cnt;
	if (event == 0)
		cpu_load_freq_history[cnt].touch_event = 100;
	else if (event == 1)
		cpu_load_freq_history[cnt].touch_event = 1;

}
struct saved_load_factor_tag saved_load_factor;

void store_external_load_factor(int type, unsigned int data)
{
	switch (type) {

	case NR_RUNNING_TASK:
		saved_load_factor.nr_running_task = data;
		break;
#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
	case PPMU_LAST_CPU_LOAD:
		saved_load_factor.ppmu_last_cpu_load = data;
		break;

	case PPMU_CPU_LOAD_SLOPE:
		saved_load_factor.ppmu_cpu_load_slope = data;
		break;

	case PPMU_LAST_DMC0_LOAD:
		saved_load_factor.ppmu_last_dmc0_load = data;
		break;

	case PPMU_LAST_DMC1_LOAD:
		saved_load_factor.ppmu_last_dmc1_load = data;
		break;

	case PPMU_BUS_MIF_FREQ:
		saved_load_factor.ppmu_bus_mif_freq = data;
		break;

	case PPMU_BUS_INT_FREQ:
		saved_load_factor.ppmu_bus_int_freq = data;
		break;

	case PPMU_BUS_MIF_LOAD:
		saved_load_factor.ppmu_bus_mif_load = data;
		break;

	case PPMU_BUS_INT_LOAD:
		saved_load_factor.ppmu_bus_int_load = data;
		break;

	case PPMU_CURR_BUS_FREQ:
		saved_load_factor.ppmu_curr_bus_freq = data;
		break;

	case BUS_LOCKED_FREQ:
		saved_load_factor.bus_locked_freq = data;
		break;

	case PPMU_BUS_FREQ_CAUSE:
		saved_load_factor.ppmu_bus_freq_cause = data;
		break;
#endif
	default:
		break;
	}
}

extern int mali_gpu_clk;
extern u32 mali_dvfs_utilization;
void store_cpu_load(unsigned int cpufreq, unsigned int cpu_load[])
{
	unsigned int j = 0, cnt = 0;
	unsigned long long t;
	unsigned long  nanosec_rem;
	int cpu_max, cpu_min;
	struct cpufreq_policy *policy;

	if (cpu_task_history_onoff == 0)
		return;

	if (++cpu_load_freq_history_cnt >= cpu_load_history_num)
		cpu_load_freq_history_cnt = 0;

	cnt = cpu_load_freq_history_cnt;
	cpu_load_freq_history[cnt].cpufreq = cpufreq;

	policy = cpufreq_cpu_get(0);

	cpu_min = pm_qos_request(PM_QOS_CPU_FREQ_MIN);
	if (cpu_min < policy->min)
		cpu_min = policy->min;

	cpu_max = pm_qos_request(PM_QOS_CPU_FREQ_MAX);
	if (cpu_max > policy->max)
		cpu_max = policy->max;

	cpu_load_freq_history[cnt].cpu_min_locked_freq = cpu_min;
	cpu_load_freq_history[cnt].cpu_max_locked_freq = cpu_max;

	t = cpu_clock(UINT_MAX);
	cpu_load_freq_history[cnt].time_stamp = t;
	nanosec_rem = do_div(t, 1000000000);
	sprintf(cpu_load_freq_history[cnt].time, "%2lu.%02lu"
				, (unsigned long) t, nanosec_rem / 10000000);

	for (j = 0; j < CPU_NUM; j++)
		cpu_load_freq_history[cnt].cpu_load[j] = cpu_load[j];

	cpu_load_freq_history[cnt].touch_event = 0;
	cpu_load_freq_history[cnt].nr_onlinecpu = num_online_cpus();

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
	cpu_load_freq_history[cnt].nr_run_avg
					= saved_load_factor.nr_running_task;

	cpu_load_freq_history[cnt].ppmu_bus_mif_freq
					= saved_load_factor.ppmu_bus_mif_freq;
	cpu_load_freq_history[cnt].ppmu_bus_int_freq
					= saved_load_factor.ppmu_bus_int_freq;
	cpu_load_freq_history[cnt].ppmu_bus_mif_load
					= saved_load_factor.ppmu_bus_mif_load;
	cpu_load_freq_history[cnt].ppmu_bus_int_load
					= saved_load_factor.ppmu_bus_int_load;

	cpu_load_freq_history[cnt].gpu_freq
					= mali_gpu_clk;
	cpu_load_freq_history[cnt].gpu_utilization
					= mali_dvfs_utilization;
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
{
	unsigned int pm_domains;
	unsigned int clk_gates[CLK_GATES_NUM];
	clk_mon_power_domain(&pm_domains);
	clk_mon_clock_gate(clk_gates);

	cpu_load_freq_history[cnt].power_domains = pm_domains;
	memcpy(cpu_load_freq_history[cnt].clk_gates
					, clk_gates, CLK_GATES_NUM*sizeof(unsigned int));
}
#endif

	for (j = 0; j < CPU_NUM; j++) {
		cpu_load_freq_history[cnt].task_history_cnt[j]
			= cpu_task_history_cnt[j];
	}


	for (j = 0; j < CPU_NUM; j++) {
		cpu_load_freq_history[cnt].work_history_cnt[j]
			= cpu_work_history_cnt[j];
	}


}


static int atoi(const char *str)
{
	int result = 0;
	int count = 0;
	if (str == NULL)
		return -1;
	while (str[count] && str[count] >= '0' && str[count] <= '9') {
		result = result * 10 + str[count] - '0';
		++count;
	}
	return result;
}

unsigned int show_cpu_load_freq_sub(int cnt, int show_cnt, char *buf, unsigned int buf_size, int ret)
{
	int j, delta = 0;

	if ((cnt - show_cnt) < 0) {
		delta = cnt - show_cnt;
		cnt = cpu_load_history_num + delta;
	} else
		cnt -= show_cnt;

	if ((cnt+1 >= cpu_load_history_num)
			|| (cpu_load_freq_history_view[cnt+1].time == 0))
		cnt = 0;
	else
		cnt++;

	for (j = 0; j < show_cnt; j++) {
		if (cnt > cpu_load_history_num-1)
			cnt = 0;

		ret +=  snprintf(buf + ret, buf_size - ret
		, "%10s\t%d.%d    %d.%d/%d.%d    [%5d]     %3d     %3d     %3d "
			"\t%2d.%02d\n"
		, cpu_load_freq_history_view[cnt].time
		, cpu_load_freq_history_view[cnt].cpufreq/1000000
		, (cpu_load_freq_history_view[cnt].cpufreq/100000) % 10
		, cpu_load_freq_history_view[cnt].cpu_min_locked_freq/1000000
		, (cpu_load_freq_history_view[cnt].cpu_min_locked_freq/100000) % 10
		, cpu_load_freq_history_view[cnt].cpu_max_locked_freq/1000000
		, (cpu_load_freq_history_view[cnt].cpu_max_locked_freq/100000) %10
		, cnt
		, cpu_load_freq_history_view[cnt].cpu_load[0]
		, cpu_load_freq_history_view[cnt].cpu_load[1]
		, cpu_load_freq_history_view[cnt].nr_onlinecpu
		, cpu_load_freq_history_view[cnt].nr_run_avg/100
		, cpu_load_freq_history_view[cnt].nr_run_avg%100);

		++cnt;
	}
	return ret;

}

enum {
	NORMAL_MODE,
	RECOVERY_MODE,
};
static int before_cpu_task_history_num;
static int before_cpu_load_history_num;

static int cpu_load_analyzer_init(int mode)
{

	if (mode == RECOVERY_MODE) {
		cpu_task_history_num = before_cpu_task_history_num;
		cpu_load_history_num = before_cpu_load_history_num;
	}

	cpu_load_freq_history =	(struct cpu_load_freq_history_tag (*)) \
	__get_free_pages(GFP_KERNEL, get_order(CPU_LOAD_FREQ_HISTORY_SIZE));
	if (cpu_load_freq_history == NULL)
		return -ENOMEM;
	cpu_load_freq_history_view = (struct cpu_load_freq_history_tag (*)) \
	__get_free_pages(GFP_KERNEL, get_order(CPU_LOAD_FREQ_HISTORY_SIZE));
	if (cpu_load_freq_history_view == NULL)
		return -ENOMEM;

	cpu_task_history = vmalloc(CPU_TASK_HISTORY_SIZE);
	if (cpu_task_history == NULL)
		return -ENOMEM;

	cpu_task_history_view = vmalloc(CPU_TASK_HISTORY_SIZE);
	if (cpu_task_history_view == NULL)
		return -ENOMEM;
	cpu_work_history = (struct cpu_work_history_tag (*)[CPU_NUM]) \
		__get_free_pages(GFP_KERNEL, get_order(CPU_WORK_HISTORY_SIZE));

	if (cpu_work_history == NULL)
		return -ENOMEM;
	cpu_work_history_view = (struct cpu_work_history_tag (*)[CPU_NUM]) \
		__get_free_pages(GFP_KERNEL, get_order(CPU_WORK_HISTORY_SIZE));
	if (cpu_work_history_view == NULL)
		return -ENOMEM;

	if (store_killed_init() != 0)
		return -ENOMEM;

	memset(cpu_load_freq_history, 0, CPU_LOAD_FREQ_HISTORY_SIZE);
	memset(cpu_load_freq_history_view, 0, CPU_LOAD_FREQ_HISTORY_SIZE);
	memset(cpu_task_history, 0, CPU_TASK_HISTORY_SIZE);
	memset(cpu_task_history_view, 0, CPU_TASK_HISTORY_SIZE);

	memset(cpu_work_history, 0, CPU_WORK_HISTORY_SIZE);
	memset(cpu_work_history_view, 0, CPU_WORK_HISTORY_SIZE);

	store_killed_memset();

	cpu_work_history_onoff = 1;

	cpu_task_history_onoff = 1;

	saved_task_info_onoff = 1;

	return 0;
}



static int cpu_load_analyzer_exit(void)
{

	before_cpu_task_history_num = cpu_task_history_num;
	before_cpu_load_history_num = cpu_load_history_num;

	cpu_task_history_onoff = 0;

	cpu_work_history_onoff = 0;

	saved_task_info_onoff = 0;

	msleep(1); /* to prevent wrong access to released memory */

	if (cpu_load_freq_history != NULL) {
		__free_pages(virt_to_page(cpu_load_freq_history)
				, get_order(CPU_LOAD_FREQ_HISTORY_SIZE));
	}
	if (cpu_load_freq_history_view != NULL) {
		__free_pages(virt_to_page(cpu_load_freq_history_view)
				, get_order(CPU_LOAD_FREQ_HISTORY_SIZE));
	}

	if (cpu_task_history != NULL)
		vfree(cpu_task_history);
	if (cpu_task_history_view != NULL)
		vfree(cpu_task_history_view);

	if (cpu_work_history != NULL) {
		__free_pages(virt_to_page(cpu_work_history)
				, get_order(CPU_WORK_HISTORY_SIZE));
	}
	if (cpu_work_history_view != NULL) {
		__free_pages(virt_to_page(cpu_work_history_view)
				, get_order(CPU_WORK_HISTORY_SIZE));
	}

	store_killed_exit();

	return 0;
}

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
unsigned int show_cpu_bus_load_freq_sub(int cnt,
					int show_cnt, char *buf, int ret)
{
	int j, delta = 0;

	if ((cnt - show_cnt) < 0) {
		delta = cnt - show_cnt;
		cnt = cpu_load_history_num + delta;
	} else
		cnt -= show_cnt;

	if ((cnt+1 >= cpu_load_history_num)
			|| (cpu_load_freq_history_view[cnt+1].time == 0))
		cnt = 0;
	else
		cnt++;

	for (j = 0; j < show_cnt; j++) {
		if (cnt > cpu_load_history_num-1)
			cnt = 0;

		if (ret < PAGE_SIZE - 1) {
		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "%10s\t%d.%d    %d.%d/%d.%d    [%5d]     %3d     %3d     %3d "
				"\t%2d.%02d      %3d      %3d      %3d     %3d      %3d     %3d\n"
			, cpu_load_freq_history_view[cnt].time
			, cpu_load_freq_history_view[cnt].cpufreq/1000000
			, (cpu_load_freq_history_view[cnt].cpufreq/100000) % 10
			, cpu_load_freq_history_view[cnt].cpu_min_locked_freq/1000000
			, (cpu_load_freq_history_view[cnt].cpu_min_locked_freq/100000) % 10
			, cpu_load_freq_history_view[cnt].cpu_max_locked_freq/1000000
			, (cpu_load_freq_history_view[cnt].cpu_max_locked_freq/100000) %10
			, cnt
			, cpu_load_freq_history_view[cnt].cpu_load[0]
			, cpu_load_freq_history_view[cnt].cpu_load[1]
			, cpu_load_freq_history_view[cnt].nr_onlinecpu
			, cpu_load_freq_history_view[cnt].nr_run_avg/100
			, cpu_load_freq_history_view[cnt].nr_run_avg%100
			, cpu_load_freq_history_view[cnt].ppmu_bus_mif_freq/1000
			, cpu_load_freq_history_view[cnt].ppmu_bus_mif_load
			, cpu_load_freq_history_view[cnt].ppmu_bus_int_freq/1000
			, cpu_load_freq_history_view[cnt].ppmu_bus_int_load
			, cpu_load_freq_history_view[cnt].gpu_freq
			, cpu_load_freq_history_view[cnt].gpu_utilization);
		} else
			break;
		++cnt;
	}

	return ret;
}
#endif

#ifdef CONFIG_SLP_BUS_CLK_CHECK_LOAD
unsigned int show_cpu_bus_clk_load_freq_sub(int cnt
					, int show_cnt, char *buf, int ret)
{
	int j, delta = 0;

	if ((cnt - show_cnt) < 0) {
		delta = cnt - show_cnt;
		cnt = cpu_load_history_num + delta;
	} else
		cnt -= show_cnt;

	if ((cnt+1 >= cpu_load_history_num)
			|| (cpu_load_freq_history_view[cnt+1].time == 0))
		cnt = 0;
	else
		cnt++;

	for (j = 0; j < show_cnt; j++) {

		if (cnt > cpu_load_history_num-1)
			cnt = 0;

		if (ret < PAGE_SIZE - 1) {
		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "%10s\t%d.%d    %d.%d/%d.%d    [%5d]     %3d     %3d     %3d " \
				"\t%2d.%02d      %3d      %3d      %3d     %3d      %3d     %3d" \
				"        %5X\t%11X%10X%10X%10X\n"
			, cpu_load_freq_history_view[cnt].time
			, cpu_load_freq_history_view[cnt].cpufreq/1000000
			, (cpu_load_freq_history_view[cnt].cpufreq/100000) % 10
			, cpu_load_freq_history_view[cnt].cpu_min_locked_freq/1000000
			, (cpu_load_freq_history_view[cnt].cpu_min_locked_freq/100000) % 10
			, cpu_load_freq_history_view[cnt].cpu_max_locked_freq/1000000
			, (cpu_load_freq_history_view[cnt].cpu_max_locked_freq/100000) %10
			, cnt
			, cpu_load_freq_history_view[cnt].cpu_load[0]
			, cpu_load_freq_history_view[cnt].cpu_load[1]
			, cpu_load_freq_history_view[cnt].nr_onlinecpu
			, cpu_load_freq_history_view[cnt].nr_run_avg/100
			, cpu_load_freq_history_view[cnt].nr_run_avg%100
			, cpu_load_freq_history_view[cnt].ppmu_bus_mif_freq/1000
			, cpu_load_freq_history_view[cnt].ppmu_bus_mif_load
			, cpu_load_freq_history_view[cnt].ppmu_bus_int_freq/1000
			, cpu_load_freq_history_view[cnt].ppmu_bus_int_load
			, cpu_load_freq_history_view[cnt].gpu_freq
			, cpu_load_freq_history_view[cnt].gpu_utilization
			, cpu_load_freq_history_view[cnt].power_domains
			, cpu_load_freq_history_view[cnt].clk_gates[0]
			, cpu_load_freq_history_view[cnt].clk_gates[1]
			, cpu_load_freq_history_view[cnt].clk_gates[2]
			, cpu_load_freq_history_view[cnt].clk_gates[3]
			);
		} else
			break;
		++cnt;
	}

	return ret;

}
#endif

static void set_cpu_load_freq_history_array_range(const char *buf)
{
	int show_array_num = 0, select_cpu;
	char cpy_buf[80] = {0,};
	char *p1, *p2, *p_lf;

	p1 = strstr(buf, "-");
	p2 = strstr(buf, "c");
	p_lf = strstr(buf, "\n");

	if (p2 == NULL)
		p2 = strstr(buf, "C");

	if ( (p2 != NULL) && ((p_lf - p2)  > 0)) {
		select_cpu = atoi(p2+1);
		if (select_cpu >= 0 && select_cpu < 4)
			cpu_task_history_show_select_cpu = select_cpu;
		else
			cpu_task_history_show_select_cpu = 0;
	} else
		cpu_task_history_show_select_cpu = -1;

	if (p1 != NULL) {
		strncpy(cpy_buf, buf, sizeof(cpy_buf) - 1);
		*p1 = '\0';
		cpu_task_history_show_start_cnt
			= get_index(atoi(cpy_buf) ,cpu_load_history_num ,-1);
		cpu_task_history_show_end_cnt = atoi(p1+1);

	} else {
		show_array_num = atoi(buf);
		cpu_task_history_show_start_cnt
			= get_index(show_array_num, cpu_load_history_num, -1);
		cpu_task_history_show_end_cnt = show_array_num;
	}

	pr_info("start_cnt=%d end_cnt=%d\n"
		, cpu_task_history_show_start_cnt, cpu_task_history_show_end_cnt);

}

static void set_cpu_load_freq_work_history_array_range(const char *buf)
{
	int show_array_num = 0, select_cpu;
	char cpy_buf[80] = {0,};
	char *p1, *p2, *p_lf;

	p1 = strstr(buf, "-");
	p2 = strstr(buf, "c");
	p_lf = strstr(buf, "\n");

	if (p2 == NULL)
		p2 = strstr(buf, "C");

	if ( (p2 != NULL) && ((p_lf - p2)  > 0)) {
		select_cpu = atoi(p2+1);
		if (select_cpu >= 0 && select_cpu < 4)
			cpu_work_history_show_select_cpu = select_cpu;
		else
			cpu_work_history_show_select_cpu = 0;
	} else
		cpu_work_history_show_select_cpu = -1;


	if (p1 != NULL) {
		strncpy(cpy_buf, buf, sizeof(cpy_buf) - 1);
		*p1 = '\0';
		cpu_work_history_show_start_cnt
			= get_index(atoi(cpy_buf) ,cpu_load_history_num, -1);
		cpu_work_history_show_end_cnt = atoi(p1+1);
	} else {
		show_array_num = atoi(buf);
		cpu_work_history_show_start_cnt
			= get_index(show_array_num,cpu_load_history_num, -1);
		cpu_work_history_show_end_cnt = show_array_num;
	}

	pr_info("show_start_cnt=%d show_end_cnt=%d\n"
		, cpu_work_history_show_start_cnt, cpu_work_history_show_end_cnt);

}


enum {
	WRONG_CPU_NUM = -1,
	WRONG_TIME_STAMP = -2,
	OVERFLOW_ERROR = -3,
};
int check_valid_range(unsigned int cpu, unsigned int start_cnt
						, unsigned int end_cnt)
{
	int ret = 0;
	unsigned long long t1, t2;
	unsigned int end_sched_cnt = 0, end_sched_cnt_margin;
	unsigned int load_cnt, last_load_cnt, overflow = 0;
	unsigned int cnt, search_cnt;
	unsigned int upset = 0;

	unsigned int i;

	if (cpu > CPU_NUM)
		return WRONG_CPU_NUM;

	t1 = cpu_load_freq_history_view[start_cnt].time_stamp;
	t2 = cpu_load_freq_history_view[end_cnt].time_stamp;

	if ((t2 <= t1) || (t1 == 0) || (t2 == 0)) {
		pr_info("[time error] t1=%lld t2=%lld\n", t1, t2);
		return WRONG_TIME_STAMP;
	}

	last_load_cnt = cpu_load_freq_history_view_cnt;

	cnt = cpu_load_freq_history_view[last_load_cnt].task_history_cnt[cpu];
	t1 = cpu_task_history_view[cnt][cpu].time;
	search_cnt = cnt;
	for (i = 0;  i < cpu_task_history_num; i++) {
		search_cnt = get_index(search_cnt, cpu_task_history_num, 1);
		t2 = cpu_task_history_view[search_cnt][cpu].time;

		if (t2 < t1) {
			end_sched_cnt = search_cnt;
			break;
		}

		if (i >= cpu_task_history_num - 1)
			end_sched_cnt = cnt;
	}

	load_cnt = last_load_cnt;
	for (i = 0;  i < cpu_load_history_num; i++) {
		unsigned int sched_cnt, sched_before_cnt;
		unsigned int sched_before_cnt_margin;

		sched_cnt
			= cpu_load_freq_history_view[load_cnt]\
						.task_history_cnt[cpu];
		load_cnt = get_index(load_cnt, cpu_load_history_num, -1);

		sched_before_cnt
			= cpu_load_freq_history_view[load_cnt]\
						.task_history_cnt[cpu];

		if (sched_before_cnt > sched_cnt)
			upset++;

		end_sched_cnt_margin
			= get_index(end_sched_cnt, cpu_task_history_num, 1);
		sched_before_cnt_margin
			= get_index(sched_before_cnt, cpu_task_history_num, -1);

		/* "end_sched_cnt -1" is needed
		  *  because of calulating schedule time */
		if ((upset >= 2) || ((upset == 1)
			&& (sched_before_cnt_margin < end_sched_cnt_margin))) {
			overflow = 1;
			pr_err("[LA] overflow cpu=%d upset=%d sched_before_cnt_margin=%d" \
				"end_sched_cnt_margin=%d end_sched_cnt=%d" \
				"sched_before_cnt=%d sched_cnt=%d load_cnt=%d" \
				, cpu , upset, sched_before_cnt_margin
				, end_sched_cnt_margin, end_sched_cnt
				, sched_before_cnt, sched_cnt, load_cnt);
			break;
		}

		if (load_cnt == start_cnt)
			break;
	}

	if (overflow == 0)
		ret = 0;
	else {
		ret = OVERFLOW_ERROR;
		pr_info("[overflow error]\n");
	}
	return ret;
}


int check_running_buf_print(int max_list_num, char *buf, int buf_size, int ret);

void cpu_print_buf_to_klog(char *buffer)
{
	#define MAX_PRINT_LINE_NUM	1000
	char *p = NULL;
	int cnt = 0;

	do {
		p = strsep(&buffer, "\n");
		if (p)
			pr_info("%s\n", p);
	}while ((p!=NULL) && (cnt++ < MAX_PRINT_LINE_NUM));
}

#ifdef CONFIG_SLP_CHECK_BUS_LOAD
void cpu_last_load_freq(unsigned int range, int max_list_num)
{
	#define BUF_SIZE (1024 * 1024)

	int ret = 0, cnt = 0;
	int start_cnt = 0, end_cnt = 0;
	char *buf;
	char range_buf[64] = {0,};

	buf = vmalloc(BUF_SIZE);

	cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;
	memcpy(cpu_load_freq_history_view, cpu_load_freq_history
				, CPU_LOAD_FREQ_HISTORY_SIZE);
	memcpy(cpu_task_history_view, cpu_task_history
					, CPU_TASK_HISTORY_SIZE);
#if defined (CONFIG_CHECK_WORK_HISTORY)
	memcpy(cpu_work_history_view, cpu_work_history
				, CPU_WORK_HISTORY_SIZE);
#endif

	ret +=  snprintf(buf + ret, PAGE_SIZE - ret
		, "=======================================" \
		"========================================\n");

	ret +=  snprintf(buf + ret, PAGE_SIZE - ret
		, "     TIME    CPU_FREQ  NIN/MAX    [INDEX]     " \
		"CPU0    CPU1   ONLINE     NR_RUN\n");


	cnt = cpu_load_freq_history_view_cnt - 1;
 	ret = show_cpu_load_freq_sub(cnt, range, buf, (BUF_SIZE - ret) ,ret);
	ret +=  snprintf(buf + ret, BUF_SIZE - ret, "\n");

	end_cnt = cnt;
	start_cnt = get_index(end_cnt, cpu_load_history_num, (0 -range + 1) );

	sprintf(range_buf, "%d-%d", start_cnt ,end_cnt);
	set_cpu_load_freq_history_array_range(range_buf);

	ret = check_running_buf_print(max_list_num, buf, (BUF_SIZE - ret), ret);

	cpu_print_buf_to_klog(buf);

	vfree(buf);

}
#endif



#define CPU_SHOW_LINE_NUM	50
static ssize_t cpu_load_freq_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;
		memcpy(cpu_load_freq_history_view, cpu_load_freq_history
					, CPU_LOAD_FREQ_HISTORY_SIZE);
		memcpy(cpu_task_history_view, cpu_task_history
					, CPU_TASK_HISTORY_SIZE);
		remained_line = cpu_load_freq_history_show_cnt;
		cnt = cpu_load_freq_history_view_cnt - 1;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "=======================================" \
			"========================================\n");

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "     TIME    CPU_FREQ  NIN/MAX    [INDEX]     " \
			"CPU0    CPU1   ONLINE     NR_RUN\n");
	}

	if (remained_line >= CPU_SHOW_LINE_NUM) {
		show_line_num = CPU_SHOW_LINE_NUM;
		remained_line -= CPU_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);
 	ret = show_cpu_load_freq_sub(cnt, show_line_num, buf, count,ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);


	return ret;
}

static ssize_t cpu_load_freq_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);
	if (show_num <= cpu_load_history_num)
		cpu_load_freq_history_show_cnt = show_num;
	else
		return -EINVAL;

	return count;
}

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
#define CPU_BUS_SHOW_LINE_NUM	30
static ssize_t cpu_bus_load_freq_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {
		cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;
		memcpy(cpu_load_freq_history_view, cpu_load_freq_history
					, CPU_LOAD_FREQ_HISTORY_SIZE);
		memcpy(cpu_task_history_view, cpu_task_history
					, CPU_TASK_HISTORY_SIZE);
		memcpy(cpu_work_history_view, cpu_work_history
						, CPU_WORK_HISTORY_SIZE);

		remained_line = cpu_bus_load_freq_history_show_cnt;
		cnt = cpu_load_freq_history_view_cnt - 1;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "================================="
			"=================================="
			"=================================="
			"=================================\n");


		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "     TIME    CPU_FREQ  NIN/MAX    [INDEX]     " \
			"CPU0    CPU1   ONLINE     NR_RUN    MIF_F    MIF_L    INT_F   INT_L" \
			"    GPU_F  GPU_UTIL\n");

	}

	if (remained_line >= CPU_BUS_SHOW_LINE_NUM) {
		show_line_num = CPU_BUS_SHOW_LINE_NUM;
		remained_line -= CPU_BUS_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);


	pr_info("show_cpu_bus_load_freq_sub cnt=%d show_line_num=%d\n", cnt, show_line_num);
	ret = show_cpu_bus_load_freq_sub(cnt, show_line_num, buf,  ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);


	return ret;
}


static ssize_t cpu_bus_load_freq_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);
	if (show_num <= cpu_load_history_num)
		cpu_bus_load_freq_history_show_cnt = show_num;
	else
		return -EINVAL;

	return count;
}

#endif

static unsigned int  work_time_list_view(unsigned int cpu,
			unsigned int start_cnt, unsigned int end_cnt
			, char *buf, unsigned int buf_size, unsigned int ret)
{
	unsigned  int i = 0, start_array_num;
	unsigned int end_array_num;

	start_array_num
	= (cpu_load_freq_history_view[start_cnt].work_history_cnt[cpu]+1)
			% cpu_work_history_num;
	end_array_num
		= cpu_load_freq_history_view[end_cnt].work_history_cnt[cpu];


	section_start_time
		= cpu_load_freq_history_view[start_cnt].time_stamp;
	section_end_time
		= cpu_load_freq_history_view[end_cnt].time_stamp;

	total_time = section_end_time - section_start_time;

	if (end_cnt == start_cnt+1) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] TOTAL SECTION TIME = %lld[ns]\n\n"
			, end_cnt, total_time);
	} else {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d~%d] TOTAL SECTION TIME = %lld[ns]\n\n"
			, get_index(start_cnt, cpu_work_history_num, 1)
			, end_cnt, total_time);
	}

	for (i = start_array_num; i <= end_array_num; i++) {
		ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] %lld-%lld delta=%lld %16s %pf(%pf)\n", i
			, cpu_work_history_view[i][cpu].start_time
			, cpu_work_history_view[i][cpu].end_time
			, cpu_work_history_view[i][cpu].occup_time
			, cpu_work_history_view[i][cpu].task->comm
			, cpu_work_history_view[i][cpu].work
			, cpu_work_history_view[i][cpu].func);
	}

	return ret;

}

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
#define CPU_BUS_CLK_SHOW_LINE_NUM	20
static ssize_t cpu_bus_clk_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = 0;
	char *buf;
	static int cnt = 0, show_line_num = 0,  remained_line = 0;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (*ppos == 0) {

		cpu_load_freq_history_view_cnt = cpu_load_freq_history_cnt;
		memcpy(cpu_load_freq_history_view, cpu_load_freq_history
						, CPU_LOAD_FREQ_HISTORY_SIZE);

		memcpy(cpu_task_history_view, cpu_task_history
						, CPU_TASK_HISTORY_SIZE);

		remained_line = cpu_bus_clk_load_freq_history_show_cnt;
		cnt = cpu_load_freq_history_view_cnt - 1;
		cnt = get_index(cnt, cpu_load_history_num, (0 - remained_line));

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "========================================="
			"=========================================="
			"=========================================="
			"=========================================="
			"==============================\n");

		ret +=  snprintf(buf + ret, PAGE_SIZE - ret
			, "     TIME    CPU_FREQ  NIN/MAX    [INDEX]     " \
			"CPU0    CPU1   ONLINE     NR_RUN    MIF_F    MIF_L    INT_F   INT_L" \
			"    GPU_F  GPU_UTIL    P-DOMAIN" \
			"     CLK1      CLK2      CLK3      CLK4\n");

	}

	if (remained_line >= CPU_BUS_CLK_SHOW_LINE_NUM) {
		show_line_num = CPU_BUS_CLK_SHOW_LINE_NUM;
		remained_line -= CPU_BUS_CLK_SHOW_LINE_NUM;

	} else {
		show_line_num = remained_line;
		remained_line = 0;
	}
	cnt = get_index(cnt, cpu_load_history_num, show_line_num);

	ret = show_cpu_bus_clk_load_freq_sub(cnt, show_line_num, buf,  ret);

	if (ret >= 0) {
		if (copy_to_user(buffer, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);


	return ret;
}

static ssize_t cpu_bus_clk_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);
	if (show_num <= cpu_load_history_num)
		cpu_bus_clk_load_freq_history_show_cnt = show_num;
	else
		return -EINVAL;

	return count;
}
#endif

int check_running_buf_print(int max_list_num, char *buf, int buf_size, int ret)
{
	int i = 0;
	int ret_check_valid;
	unsigned long long t;
	unsigned int cpu;

	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned long  msec_rem;

	for (i = 0; i < CPU_NUM ; i++) {
		if (cpu_task_history_show_select_cpu != -1)
			if (i != cpu_task_history_show_select_cpu)
				continue;
		cpu = i;
		ret_check_valid = check_valid_range(cpu, start_cnt, end_cnt);
		if (ret_check_valid < 0)	{
			ret +=  snprintf(buf + ret, buf_size - ret
				, "[ERROR] cpu[%d] Invalid range !!! err=%d\n"
				, cpu, ret_check_valid);
			pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n"
				, cpu, ret_check_valid);
			continue;
		}

		clc_process_run_time(i, start_cnt, end_cnt);

		t = total_time;
		msec_rem = do_div(t, 1000000);

		if (end_cnt == start_cnt+1) {
			ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n"
			, end_cnt	, (unsigned long)t, msec_rem);
		} else {
			ret +=  snprintf(buf + ret, buf_size - ret,
			"[%d~%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n"
			, start_cnt+1, end_cnt, (unsigned long)t
			, msec_rem);
		}

		ret += snprintf(buf + ret, buf_size - ret,
			"####################################"
			" CPU %d ##############################\n", i);

		if (cpu_task_history_show_select_cpu == -1)
			ret = view_list(max_list_num, buf, buf_size, ret);
		else if (i == cpu_task_history_show_select_cpu)
			ret = view_list(max_list_num, buf, buf_size, ret);

		if (ret < buf_size - 1)
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
	}

	return ret;
}

static ssize_t check_running_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
 	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
 	unsigned int i, ret = 0, size_for_copy = count;
	int ret_check_valid;
	static unsigned int rest_size = 0;

	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned long  msec_rem;
	unsigned long long t;
	unsigned int cpu;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		if ((end_cnt) == (start_cnt + 1)) {
			ret +=  snprintf(buf + ret, buf_size - ret
			, "=======================================" \
			"========================================\n");

			ret +=  snprintf(buf + ret, buf_size - ret
				, "     TIME    CPU_FREQ  NIN/MAX    [INDEX]     " \
					"CPU0    CPU1   ONLINE     NR_RUN\n");

			ret = show_cpu_load_freq_sub((int)end_cnt+2
							, 5, buf, buf_size, ret);
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		for (i = 0; i < CPU_NUM ; i++) {
			if (cpu_task_history_show_select_cpu != -1)
				if (i != cpu_task_history_show_select_cpu)
					continue;
			cpu = i;
			ret_check_valid = check_valid_range(cpu, start_cnt, end_cnt);
			if (ret_check_valid < 0)	{
				ret +=  snprintf(buf + ret, buf_size - ret
					, "[ERROR] cpu[%d] Invalid range !!! err=%d\n"
					, cpu, ret_check_valid);
				pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n"
					, cpu, ret_check_valid);
				continue;
			}

			clc_process_run_time(i, start_cnt, end_cnt);

			t = total_time;
			msec_rem = do_div(t, 1000000);

			if (end_cnt == start_cnt+1) {
				ret +=  snprintf(buf + ret, buf_size - ret,
				"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n"
				, end_cnt	, (unsigned long)t, msec_rem);
			} else {
				ret +=  snprintf(buf + ret, buf_size - ret,
				"[%d~%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n"
				, start_cnt+1, end_cnt, (unsigned long)t
				, msec_rem);
			}

			ret += snprintf(buf + ret, buf_size - ret,
				"####################################"
				" CPU %d ##############################\n", i);

			if (cpu_task_history_show_select_cpu == -1)
				ret = view_list(INT_MAX, buf, buf_size, ret);
			else if (i == cpu_task_history_show_select_cpu)
				ret = view_list(INT_MAX, buf, buf_size, ret);

			if (ret < buf_size - 1)
				ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	}else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	pr_info("YUBAEK *ppos =%lld size_for_copy = %d rest_size =%d ret =%d\n",*ppos,  size_for_copy, rest_size, ret);

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		vfree(buf);

	return size_for_copy;
}


static ssize_t check_running_raw_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
	unsigned int i, ret = 0, size_for_copy = count;
	int ret_check_valid;
	static unsigned int rest_size = 0;

	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;
	unsigned long  msec_rem;
	unsigned long long t;
	unsigned int cpu;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		if ((end_cnt) == (start_cnt + 1)) {
			ret +=  snprintf(buf + ret, buf_size - ret
			, "=======================================" \
			"========================================\n");

			ret +=  snprintf(buf + ret, buf_size - ret
				, "    TIME    CPU_FREQ   [INDEX]\tCPU0 \tCPU1" \
					  " \tCPU2\tCPU3\tONLINE\tNR_RUN\n");

			ret = show_cpu_load_freq_sub((int)end_cnt+2
							, 5, buf, buf_size, ret);
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		for (i = 0; i < CPU_NUM ; i++) {
			if (cpu_task_history_show_select_cpu != -1)
				if (i != cpu_task_history_show_select_cpu)
					continue;
			cpu = i;
			ret_check_valid = check_valid_range(cpu, start_cnt, end_cnt);
			if (ret_check_valid < 0)	{
				ret +=  snprintf(buf + ret, buf_size - ret
					, "[ERROR] cpu[%d] Invalid range !!! err=%d\n"
					, cpu, ret_check_valid);
				pr_info("[ERROR] cpu[%d] Invalid range !!! err=%d\n"
					, cpu, ret_check_valid);
				continue;
			}

			clc_process_run_time(i, start_cnt, end_cnt);

			t = total_time;
			msec_rem = do_div(t, 1000000);

			ret += snprintf(buf + ret, buf_size - ret, "# %d\n", i);

			if (end_cnt == start_cnt+1) {
				ret +=  snprintf(buf + ret, buf_size - ret,
				"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n"
				, end_cnt	, (unsigned long)t, msec_rem);
			}

			if (cpu_task_history_show_select_cpu == -1)
				ret = view_list_raw(buf, buf_size, ret);
			else if (i == cpu_task_history_show_select_cpu)
				ret = view_list_raw(buf, buf_size, ret);

		}

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	}else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	pr_info("YUBAEK *ppos =%lld size_for_copy = %d rest_size =%d ret =%d\n",*ppos,  size_for_copy, rest_size, ret);

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		vfree(buf);

	return size_for_copy;
}


static ssize_t check_work_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 256);
	unsigned int i, ret = 0, size_for_copy = count;
	static unsigned int rest_size = 0;

	unsigned int start_cnt = cpu_work_history_show_start_cnt;
	unsigned int end_cnt = cpu_work_history_show_end_cnt;
	unsigned long  msec_rem;
	unsigned long long t;
	unsigned int cpu;

	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = vmalloc(buf_size);

		if (!buf)
			return -ENOMEM;

		if ((end_cnt) == (start_cnt + 1)) {
			ret +=  snprintf(buf + ret, buf_size - ret
			, "=======================================" \
			"========================================\n");

			ret +=  snprintf(buf + ret, buf_size - ret
				, "    TIME    CPU_FREQ   [INDEX]\tCPU0 \tCPU1" \
					  " \tCPU2\tCPU3\tONLINE\tNR_RUN\n");

			ret = show_cpu_load_freq_sub((int)end_cnt+2
							, 5, buf, buf_size, ret);
			ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		for (i = 0; i < CPU_NUM ; i++) {
			if (cpu_work_history_show_select_cpu != -1)
				if (i != cpu_work_history_show_select_cpu)
					continue;
			cpu = i;
			if (check_valid_range(cpu, start_cnt, end_cnt) ==  -1)	{
				ret +=  snprintf(buf + ret, buf_size - ret
					, "[ERROR] cpu[%d] Invalid range !!!\n", cpu);
				pr_info("[ERROR] cpu[%d] Invalid range !!!\n", cpu);
				continue;
			}

			clc_work_run_time(i, start_cnt, end_cnt);

			t = total_time;
			msec_rem = do_div(t, 1000000);

			if (end_cnt == start_cnt+1) {
				ret +=  snprintf(buf + ret, buf_size - ret,
				"[%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n"
				, end_cnt	, (unsigned long)t, msec_rem);
			} else {
				ret +=  snprintf(buf + ret, buf_size - ret,
				"[%d~%d] TOTAL SECTION TIME = %ld.%ld[ms]\n\n"
				, start_cnt+1, end_cnt, (unsigned long)t
				, msec_rem);
			}

			ret += snprintf(buf + ret, buf_size - ret,
				"##################################"
				" CPU %d #############################\n", i);

			if (cpu_work_history_show_select_cpu == -1)
				ret = view_workfn_list(buf, buf_size, ret);
			else if (i == cpu_work_history_show_select_cpu)
				ret = view_workfn_list(buf, buf_size, ret);

			if (ret < buf_size - 1)
				ret +=  snprintf(buf + ret, buf_size - ret, "\n\n");
		}

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	}else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	pr_info("YUBAEK *ppos =%lld size_for_copy = %d rest_size =%d ret =%d\n",*ppos,  size_for_copy, rest_size, ret);

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			vfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		vfree(buf);

	return size_for_copy;
}



static ssize_t check_running_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	set_cpu_load_freq_history_array_range(user_buf);

	return count;
}

static ssize_t check_work_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	set_cpu_load_freq_work_history_array_range(user_buf);

	return count;
}


#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
static unsigned int checking_clk_index;
static ssize_t check_clk_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
 	static char *buf = NULL;
	int buf_size = (PAGE_SIZE * 3);
 	unsigned int ret = 0, size_for_copy = count;
	static unsigned int rest_size = 0;

	unsigned int *clk_gates;
	unsigned int power_domains;


	if (*ppos < 0 || !count)
		return -EINVAL;

	if (*ppos == 0) {
		buf = kmalloc(buf_size, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		clk_gates = cpu_load_freq_history_view[checking_clk_index].clk_gates;
		power_domains = cpu_load_freq_history_view[checking_clk_index].power_domains;

		ret = clk_mon_get_clock_info(power_domains, clk_gates, buf);

		if (ret <= count) {
			size_for_copy = ret;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size = ret -size_for_copy;
		}
	}else {
		if (rest_size <= count) {
			size_for_copy = rest_size;
			rest_size = 0;
		} else {
			size_for_copy = count;
			rest_size -= size_for_copy;
		}
	}

	if (size_for_copy >  0) {
		int offset = (int) *ppos;
		if (copy_to_user(buffer, buf + offset , size_for_copy)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += size_for_copy;
	} else
		kfree(buf);

	return size_for_copy;
}

static ssize_t check_clk_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int show_num = 0;

	show_num = atoi(user_buf);
	if (show_num <= cpu_load_history_num)
		checking_clk_index = show_num;
	else
		return -EINVAL;

	return count;
}
#endif

enum {
	SCHED_HISTORY_NUM,
	GOVERNOR_HISTORY_NUM,
};

static int load_analyzer_reset(int mode, int value)
{
	int ret = 0;

	cpu_load_analyzer_exit();

	switch (mode) {

	case SCHED_HISTORY_NUM:
		cpu_task_history_num = value;
		break;

	case GOVERNOR_HISTORY_NUM:
		cpu_load_history_num = value;
		break;

	default:
		break;
	}

	 ret = cpu_load_analyzer_init(NORMAL_MODE);
	 if (ret < 0) {
		 cpu_load_analyzer_init(RECOVERY_MODE);
		return -EINVAL;
	 }

	return 0;
}

#define CPU_LOAD_SCHED_NUM_MIN	1000
#define CPU_LOAD_SCHED_NUM_MAX	1000000
static ssize_t saved_sched_num_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int saved_sched_num = 0;

	saved_sched_num = atoi(user_buf);
	if ((saved_sched_num < CPU_LOAD_SCHED_NUM_MIN)
		&& (saved_sched_num > CPU_LOAD_SCHED_NUM_MAX)) {

		pr_info("Wrong range value saved_sched_num = %d\n", saved_sched_num);

		return -EINVAL;
	}

	if (load_analyzer_reset(SCHED_HISTORY_NUM, saved_sched_num) < 0)
		return -EINVAL;

	return count;
}


static int saved_sched_num_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", cpu_task_history_num);

	return ret;
}

static ssize_t saved_sched_num_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, saved_sched_num_read_sub);

	return size_for_copy;
}



static int check_running_detail_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0;
	unsigned int start_cnt = cpu_task_history_show_start_cnt;
	unsigned int end_cnt = cpu_task_history_show_end_cnt;

	for (i = 0; i < CPU_NUM; i++) {
		ret += snprintf(buf + ret, buf_size - ret,
			"#############################" \
			   " CPU %d #############################\n", i);

		ret = process_sched_time_view(i, start_cnt, end_cnt, buf, buf_size, ret);
	}

	return ret;
}

static ssize_t check_running_detail(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, check_running_detail_sub);

	return size_for_copy;
}



static int check_work_detail_sub(char *buf, int buf_size)
{
	int ret = 0, i = 0;

	unsigned int start_cnt = cpu_work_history_show_start_cnt;
	unsigned int end_cnt = cpu_work_history_show_end_cnt;


	for (i = 0; i < CPU_NUM; i++) {
		ret += snprintf(buf + ret, buf_size - ret,
			"#############################" \
			   " CPU %d #############################\n", i);

		ret = work_time_list_view(i, start_cnt, end_cnt, buf, buf_size, ret);
	}

	return ret;
}

static ssize_t check_work_detail(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, check_work_detail_sub);

	return size_for_copy;
}



static int not_lpa_cause_check_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret += snprintf(buf + ret, buf_size - ret, "%s\n",  get_not_w_aftr_cause());

	return ret;
}


static ssize_t not_lpa_cause_check(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, not_lpa_cause_check_sub);

	return size_for_copy;
}

static ssize_t cpuidle_log_en(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	cpuidle_aftr_log_en(atoi(user_buf));

	return count;
}



int value_for_debug;
static int debug_value_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", value_for_debug);

	return ret;
}

static ssize_t debug_value_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, debug_value_read_sub);

	return size_for_copy;
}

static ssize_t debug_value_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{

	value_for_debug = atoi(user_buf);

	return count;
}



static struct pm_qos_request pm_qos_min_cpu, pm_qos_max_cpu;
static int la_fixed_cpufreq_value;

static int fixed_cpu_freq_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", la_fixed_cpufreq_value);

	return ret;
}

static ssize_t fixed_cpu_freq_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, fixed_cpu_freq_read_sub);

	return size_for_copy;
}

static ssize_t fixed_cpu_freq_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	struct cpufreq_policy *policy;

	int fixed_cpufreq_value;

	if ((user_buf[0] == '-') && (user_buf[1] == '1'))
		fixed_cpufreq_value = -1;
	else
		fixed_cpufreq_value = atoi(user_buf);

	if (fixed_cpufreq_value == -1) {
		la_fixed_cpufreq_value = fixed_cpufreq_value;
		if (pm_qos_request_active(&pm_qos_min_cpu))
			pm_qos_remove_request(&pm_qos_min_cpu);

		if (pm_qos_request_active(&pm_qos_max_cpu))
			pm_qos_remove_request(&pm_qos_max_cpu);
	} else {
		policy = cpufreq_cpu_get(0);

		if ((fixed_cpufreq_value < policy->min)
			|| (fixed_cpufreq_value > policy->max)
			|| ((fixed_cpufreq_value % 10000) !=0)) {
			pr_err("Invalid cpufreq : %d\n", fixed_cpufreq_value);
			return -EINVAL;
		}

		la_fixed_cpufreq_value = fixed_cpufreq_value;
		if (!pm_qos_request_active(&pm_qos_max_cpu)) {
			pm_qos_add_request(&pm_qos_max_cpu
				, PM_QOS_CPU_FREQ_MAX, fixed_cpufreq_value);
		}

		if (!pm_qos_request_active(&pm_qos_min_cpu)) {
			pm_qos_add_request(&pm_qos_min_cpu
				, PM_QOS_CPU_FREQ_MIN, fixed_cpufreq_value);
		}
	}

	return count;
}


static int saved_load_analyzer_data_read_sub(char *buf, int buf_size)
{
	int ret = 0;

	ret +=  snprintf(buf + ret, buf_size - ret, "%d\n", cpu_load_history_num);

	return ret;
}

static ssize_t saved_load_analyzer_data_read(struct file *file,
	char __user *buffer, size_t count, loff_t *ppos)
{
	unsigned int size_for_copy;

	size_for_copy = wrapper_for_debug_fs(buffer, count, ppos, saved_load_analyzer_data_read_sub);

	return size_for_copy;
}

#define CPU_LOAD_HISTORY_NUM_MIN	1000
#define CPU_LOAD_HISTORY_NUM_MAX	8000
static ssize_t saved_load_analyzer_data_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	int history_num;
	history_num = atoi(user_buf);

	if ((history_num < CPU_LOAD_SCHED_NUM_MIN)
		&& (history_num > CPU_LOAD_SCHED_NUM_MAX)) {

		pr_info("Wrong range value cpu_load_history_num = %d\n", history_num);

		return -EINVAL;
	}

	if (load_analyzer_reset(GOVERNOR_HISTORY_NUM, history_num) < 0)
		return -EINVAL;
	return count;
}

static ssize_t last_cpu_load_write(struct file *file,
				      const char __user *user_buf, size_t count,
				      loff_t *ppos)
{
	#define MAX_RANGE_NUM	1000
	int range_num, max_list_num;

	char *p1 = NULL, *p_lf = NULL;

	p_lf = strstr(user_buf, "\n");

	p1= strstr(user_buf, "TOP");

	if (p1 == NULL)
		p1= strstr(user_buf, "top");

	if (p_lf != NULL) {
		if (p1 - p_lf > 0)
			p1 = NULL;
	}

	if (p1 != NULL)
		max_list_num = atoi(p1+3);
	else
		max_list_num = INT_MAX;

	range_num = atoi(user_buf);
	pr_info("range_num=%d\n", range_num);

	if ((range_num > 0) && (range_num <= MAX_RANGE_NUM))
		cpu_last_load_freq(range_num, max_list_num);
	else
		return -EINVAL;


	return count;
}


static const struct file_operations cpu_load_freq_fops = {
	.owner = THIS_MODULE,
	.read = cpu_load_freq_read,
	.write =cpu_load_freq_write,
};

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
static const struct file_operations cpu_bus_load_freq_fops = {
	.owner = THIS_MODULE,
	.read = cpu_bus_load_freq_read,
	.write =cpu_bus_load_freq_write,
};
#endif

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
static const struct file_operations cpu_bus_clk_fops = {
	.owner = THIS_MODULE,
	.read = cpu_bus_clk_read,
	.write =cpu_bus_clk_write,
};

static const struct file_operations check_clk_fops = {
	.owner = THIS_MODULE,
	.read = check_clk_read,
	.write =check_clk_write,
};
#endif

static const struct file_operations check_running_fops = {
	.owner = THIS_MODULE,
	.read = check_running_read,
	.write =check_running_write,
};

static const struct file_operations check_running_raw_fops = {
	.owner = THIS_MODULE,
	.read = check_running_raw_read,
	.write =check_running_write,
};

static const struct file_operations check_work_fops = {
	.owner = THIS_MODULE,
	.read = check_work_read,
	.write =check_work_write,
};

static const struct file_operations check_running_detail_fops = {
	.owner = THIS_MODULE,
	.read = check_running_detail,
};

static const struct file_operations check_work_detail_fops = {
	.owner = THIS_MODULE,
	.read = check_work_detail,
};

static const struct file_operations not_lpa_cause_check_fops = {
	.owner = THIS_MODULE,
	.read = not_lpa_cause_check,
	.write =cpuidle_log_en,
};

static const struct file_operations saved_sched_num_fops = {
	.owner = THIS_MODULE,
	.read = saved_sched_num_read,
	.write =saved_sched_num_write,
};

static const struct file_operations saved_load_analyzer_data_fops = {
	.owner = THIS_MODULE,
	.read = saved_load_analyzer_data_read,
	.write =saved_load_analyzer_data_write,
};


static const struct file_operations debug_value_fops = {
	.owner = THIS_MODULE,
	.read = debug_value_read,
	.write =debug_value_write,
};

static const struct file_operations fixed_cpu_freq_fops = {
	.owner = THIS_MODULE,
	.read = fixed_cpu_freq_read,
	.write =fixed_cpu_freq_write,
};

static const struct file_operations last_cpu_load_fops = {
	.owner = THIS_MODULE,
	.write =last_cpu_load_write,
};

static int __init system_load_analyzer_init(void)
{
	int ret = 0;
	struct dentry *d;

	d = debugfs_create_dir("load_analyzer", NULL);
	if (!d) {
		pr_err("create load_analyzer debugfs\n");
		return -1;
	}

	if (!debugfs_create_file("cpu_load_freq", 0600
		, d, NULL,&cpu_load_freq_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "cpu_load_freq");

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
	if (!debugfs_create_file("cpu_bus_load_freq", 0600
		, d, NULL,&cpu_bus_load_freq_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "cpu_bus_load_freq");
#endif
	if (!debugfs_create_file("check_running", 0600
		, d, NULL,&check_running_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "check_running");

	if (!debugfs_create_file("check_running_raw", 0600
		, d, NULL,&check_running_raw_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "check_running_raw");

	if (!debugfs_create_file("check_work", 0600
		, d, NULL,&check_work_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "check_work");

	if (!debugfs_create_file("saved_sched_num", 0600
		, d, NULL,&saved_sched_num_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "saved_sched_num");

	if (!debugfs_create_file("saved_load_analyzer_data_num", 0600
		, d, NULL,&saved_load_analyzer_data_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "saved_load_analyzer_data_num");

	if (!debugfs_create_file("check_running_detail", 0600
		, d, NULL,&check_running_detail_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "check_running_detail");

	if (!debugfs_create_file("check_work_detail", 0600
		, d, NULL,&check_work_detail_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "check_work_detail");

	if (!debugfs_create_file("not_lpa_cause_check", 0600
		, d, NULL,&not_lpa_cause_check_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "not_lpa_cause_check");

#if defined(CONFIG_SLP_BUS_CLK_CHECK_LOAD)
	if (!debugfs_create_file("cpu_bus_clk_freq", 0600
		, d, NULL,&cpu_bus_clk_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "cpu_bus_clk_freq");

	if (!debugfs_create_file("check_clk", 0600
		, d, NULL,&check_clk_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "check_clk");
#endif

	if (!debugfs_create_file("debug_value", 0600
		, d, NULL,&debug_value_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "debug_value");

	if (!debugfs_create_file("fixed_cpu_freq", 0600
		, d, NULL,&fixed_cpu_freq_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "fixed_cpu_freq");

	if (!debugfs_create_file("last_cpu_load", 0200
		, d, NULL,&last_cpu_load_fops))   \
			pr_err("%s : debugfs_create_file, error\n", "last_cpu_load");

	if (cpu_load_analyzer_init(NORMAL_MODE) != 0)
		pr_info("[%s] cpu_load_analyzer_init ERROR", __func__);


#if defined(CONFIG_SLP_MINI_TRACER)
	if (kernel_mini_tracer_init() != 0)
		pr_info("[%s] kernel_mini_tracer_init ERROR", __func__);
#endif

	return ret;
}

static void __exit system_load_analyzer_exit(void)
{
		cpu_load_analyzer_exit();

#if defined(CONFIG_SLP_MINI_TRACER)
	kernel_mini_tracer_exit();
#endif

}

MODULE_AUTHOR("Yong-U, Baek <yu.baek@samsung.com>");
MODULE_DESCRIPTION("'SLP power debuger");
MODULE_LICENSE("GPL");


module_init(system_load_analyzer_init);
module_exit(system_load_analyzer_exit);

