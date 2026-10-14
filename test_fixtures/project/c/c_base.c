#include "c_base.h"
#include <string.h>

static unsigned int g_cfg_mode;   /* 全局变量用例：跨函数共享污点 */

int hal_cfg_step3(const char *name)
{
    char local[32];
    strcpy(local, name);          /* name 污点 -> local */
    return (int)local[0];
}

int hal_cfg_step2(unsigned int len)
{
    unsigned int cnt = len;       /* len 污点 -> cnt */
    if (cnt > 16) {
        cnt = 16;                 /* 校验：仍继续传播（校验不终止传播） */
    }
    return (int)cnt;
}

int hal_cfg_step1(unsigned int mode)
{
    g_cfg_mode = mode;            /* mode 污点 -> 全局变量 */
    return hal_cfg_step2(mode);
}

int hal_cfg_check(void)
{
    return (int)(g_cfg_mode + 1); /* 读取全局污点 */
}

int hal_apply_cfg(struct sensor_cfg *cfg)
{
    int r = 0;
    r += hal_cfg_step1(cfg->mode);   /* 结构体成员 -> 调用 */
    r += hal_cfg_step2(cfg->len);
    r += hal_cfg_step3(cfg->name);   /* 结构体数组成员 */
    r += hal_cfg_check();            /* 全局污点经共享状态进入 */
    return r;
}
