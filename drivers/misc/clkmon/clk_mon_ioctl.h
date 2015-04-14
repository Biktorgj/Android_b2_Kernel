#ifndef _CLK_MON_IOCTL_H_
#define _CLK_MON_IOCTL_H_

#define CLK_MON_DEV_NAME        "clk_mon"
#define CLK_MON_DEV_PATH        "/dev/clk_mon"

#define CLK_MON_MAGIC                   254
#define CLK_MON_IOCTL_MAX_NR    10

#define CLK_MON_IOC_CHECK_REG                   \
        _IOWR(CLK_MON_MAGIC, 4, struct clk_mon_ioc_buf)
#define CLK_MON_IOC_CHECK_POWER_DOMAIN  \
        _IOR(CLK_MON_MAGIC, 5, struct clk_mon_ioc_buf)
#define CLK_MON_IOC_CHECK_CLOCK_DOMAIN  \
        _IOR(CLK_MON_MAGIC, 6, struct clk_mon_ioc_buf)
#define CLK_MON_IOC_SET_REG                             \
        _IOW(CLK_MON_MAGIC, 7, struct clk_mon_reg_info)

#define CLK_MON_MAX_REG_NUM             256
#define CLK_MON_MAX_NAME_LEN    30

struct clk_mon_reg_info {
        char name[CLK_MON_MAX_NAME_LEN];
        void *addr;
        unsigned int value;
};

struct clk_mon_ioc_buf {
        struct clk_mon_reg_info reg[CLK_MON_MAX_REG_NUM];
        int nr_addrs;
};

#endif
