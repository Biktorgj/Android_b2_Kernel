
#ifndef _LINUX_SYSTEM_LOAD_ANALYZER_H
#define _LINUX_SYSTEM_LOAD_ANALYZER_H

#define CPU_NUM	NR_CPUS
#define CONFIG_SLP_MINI_TRACER

enum {
	NR_RUNNING_TASK,
	PPMU_LAST_CPU_LOAD,
	PPMU_CPU_LOAD_SLOPE,
	PPMU_LAST_DMC0_LOAD,
	PPMU_LAST_DMC1_LOAD,

	PPMU_BUS_MIF_FREQ,
	PPMU_BUS_INT_FREQ,
	PPMU_BUS_MIF_LOAD,
	PPMU_BUS_INT_LOAD,

	PPMU_CURR_BUS_FREQ,
	BUS_LOCKED_FREQ,
	PPMU_BUS_FREQ_CAUSE,
};

struct saved_load_factor_tag {
	unsigned int nr_running_task;
	unsigned int ppmu_last_cpu_load;
	unsigned int ppmu_cpu_load_slope;
	unsigned int ppmu_last_dmc0_load;
	unsigned int ppmu_last_dmc1_load;

	unsigned int ppmu_bus_mif_freq;
	unsigned int ppmu_bus_int_freq;
	unsigned int ppmu_bus_mif_load;
	unsigned int ppmu_bus_int_load;

	unsigned int ppmu_curr_bus_freq;
	unsigned int bus_locked_freq;
	unsigned int ppmu_bus_freq_cause;
};

extern struct saved_load_factor_tag	saved_load_factor;

extern bool cpu_task_history_onoff;

void store_external_load_factor(int type, unsigned int data);

void store_cpu_load(unsigned int cpufreq, unsigned int cpu_load[]);

void cpu_load_touch_event(unsigned int event);

void __slp_store_task_history(unsigned int cpu, struct task_struct *task);

static inline void slp_store_task_history(unsigned int cpu
					, struct task_struct *task)
{
	__slp_store_task_history(cpu, task);
}

u64  get_load_analyzer_time(void);

void __slp_store_work_history(struct work_struct *work, work_func_t func
						, u64 start_time, u64 end_time);

void store_killed_task(struct task_struct *tsk);

#if defined(CONFIG_SLP_CHECK_BUS_LOAD)
void cpu_last_load_freq(unsigned int range);
#endif

extern int value_for_debug;

#if defined(CONFIG_SLP_MINI_TRACER)

#define TIME_ON		(1 << 0)
#define FLUSH_CACHE	(1 << 1)

extern int kernel_mini_tracer_i2c_log_on;

void kernel_mini_tracer(char *input_string, int option);
void kernel_mini_tracer_smp(char *input_string);
#endif


#endif
