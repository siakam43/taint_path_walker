/* c04 用例：常量类参数过滤（flags 仅与固定值比较，不流入其他位置） */

static int c04_sink(const char *p, unsigned int n)
{
    return (int)(p[0] + n);
}

int hal_cfg_apply2(unsigned int flags, const char *data, unsigned int len)
{
    if ((flags & 0x0F) == 0x01) {
        return -1;                 /* flags 仅参与位测试 */
    }
    return c04_sink(data, len);    /* data/len 污点路径 */
}
