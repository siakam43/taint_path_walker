/* 深度截断用例：入口 + 深链 */
int deep12(int v) { return v; }
int deep11(int v) { return deep12(v); }
int deep10(int v) { return deep11(v); }
int deep9(int v)  { return deep10(v); }
int deep8(int v)  { return deep9(v); }
int deep7(int v)  { return deep8(v); }
int deep6(int v)  { return deep7(v); }
int deep5(int v)  { return deep6(v); }
int deep4(int v)  { return deep5(v); }
int deep3(int v)  { return deep4(v); }
int deep2(int v)  { return deep3(v); }
int deep1(int v)  { return deep2(v); }
int deep0(int v)  { return deep1(v); }

int entry_deep(int v)
{
    return deep0(v);
}

/* 扇出截断用例：21 项函数表（超出单分发点 20 上限） */
typedef int (*op_fn)(unsigned int);

static int op_00(unsigned int v) { return (int)(v + 0); }
static int op_01(unsigned int v) { return (int)(v + 1); }
static int op_02(unsigned int v) { return (int)(v + 2); }
static int op_03(unsigned int v) { return (int)(v + 3); }
static int op_04(unsigned int v) { return (int)(v + 4); }
static int op_05(unsigned int v) { return (int)(v + 5); }
static int op_06(unsigned int v) { return (int)(v + 6); }
static int op_07(unsigned int v) { return (int)(v + 7); }
static int op_08(unsigned int v) { return (int)(v + 8); }
static int op_09(unsigned int v) { return (int)(v + 9); }
static int op_10(unsigned int v) { return (int)(v + 10); }
static int op_11(unsigned int v) { return (int)(v + 11); }
static int op_12(unsigned int v) { return (int)(v + 12); }
static int op_13(unsigned int v) { return (int)(v + 13); }
static int op_14(unsigned int v) { return (int)(v + 14); }
static int op_15(unsigned int v) { return (int)(v + 15); }
static int op_16(unsigned int v) { return (int)(v + 16); }
static int op_17(unsigned int v) { return (int)(v + 17); }
static int op_18(unsigned int v) { return (int)(v + 18); }
static int op_19(unsigned int v) { return (int)(v + 19); }
static int op_20(unsigned int v) { return (int)(v + 20); }

static op_fn g_ops[21] = {
    op_00, op_01, op_02, op_03, op_04, op_05, op_06, op_07, op_08, op_09,
    op_10, op_11, op_12, op_13, op_14, op_15, op_16, op_17, op_18, op_19,
    op_20
};

int entry_fanout(unsigned int idx, unsigned int v)
{
    return g_ops[idx](v);    /* 分发点：表项 21 > 20 上限，追踪前 20 */
}
