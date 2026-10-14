#ifndef C_BASE_H
#define C_BASE_H

struct sensor_cfg {
    unsigned int len;
    unsigned int mode;
    char name[32];
};

int hal_apply_cfg(struct sensor_cfg *cfg);
int hal_cfg_step1(unsigned int mode);
int hal_cfg_step2(unsigned int len);
int hal_cfg_step3(const char *name);
int hal_cfg_check(void);

#endif
