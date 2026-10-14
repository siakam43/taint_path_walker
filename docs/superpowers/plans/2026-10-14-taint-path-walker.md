# taint-path-walker 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付可在 Claude Code 中运行的 taint-path-walker skill（纯 markdown 单文件 `taint-path-walker/SKILL.md`）与配套测试夹具 `test_fixtures/`，完成端到端验收与基线录制。

**Architecture:** 测试先行——先构建合成 C/C++ 夹具工程（"测试"），再编写 SKILL.md（"实现"），然后对夹具逐用例执行 skill 并按"必须成立的链集合与解析结论"验收，修订提示词直至通过，最后录制基线并全量回归。设计依据：`docs/superpowers/specs/2026-10-14-taint-path-walker-design.md`（下称"设计文档"，其 §N 指节号）。

**Tech Stack:** Markdown 提示词；合成 C/C++ 夹具代码（gcc/g++ 仅做 `-fsyntax-only` 语法检查）；jq/diff 核对 JSON；无构建系统。

**环境说明（重要）：** 当前目录非 git 仓库 → **本计划所有任务均无 git 提交步骤**，任务以"保存 + 验证"收尾。所有文件路径以仓库根 `/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/` 为相对基准。

**开发期如何"运行 skill"：** skill 尚未安装为斜杠命令。执行阶段的"运行"= Read 全文 `taint-path-walker/SKILL.md`，然后严格按其全部规则，以给定 `project_dir` 与 `entry_json` 执行一次完整分析流程（执行者自身就是运行该 skill 的主 agent；运行 skill 时不得再派发 sub-agent 分析）。

---

## 文件结构总览

```
taint_path_walker/
├── taint-path-walker/SKILL.md            # 任务 8-12 编写
├── test_fixtures/
│   ├── TEST_PLAN.md                      # 任务 7 初稿，任务 17 定稿
│   ├── project/                          # 任务 2-6 编写（合成工程）
│   │   ├── c/c_base.h  c_base.c  c_channel.c
│   │   ├── cpp/vtable.h  vtable.cpp  overload.cpp  templates.cpp  ctor_conv.cpp
│   │   │        smart_lambda.cpp  mi.cpp
│   │   └── mech/dedup.c  limits.cpp  cycle.c  external.c
│   ├── entries/                          # 任务 7 编写（19 个入口文件）
│   │   └── {c01,c02,c03,cpp01..cpp09,m01..m05,bad01,bad02}_entry.json
│   └── expected/                         # 任务 17 从验收通过的运行结果录制
│       └── {TreeID}.json
└── docs/superpowers/plans/2026-10-14-taint-path-walker.md   # 本计划
```

---

### Task 1: 创建目录骨架

**Files:**
- Create: `taint-path-walker/`（空目录，SKILL.md 在任务 8 创建）
- Create: `test_fixtures/project/c/`、`test_fixtures/project/cpp/`、`test_fixtures/project/mech/`
- Create: `test_fixtures/entries/`、`test_fixtures/expected/`

- [ ] **Step 1: 创建目录**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker
mkdir -p taint-path-walker test_fixtures/project/c test_fixtures/project/cpp test_fixtures/project/mech test_fixtures/entries test_fixtures/expected
```

- [ ] **Step 2: 验证**

在仓库根执行: `ls -d taint-path-walker test_fixtures/project/c test_fixtures/project/cpp test_fixtures/project/mech test_fixtures/entries test_fixtures/expected`
Expected: 六个目录全部列出，无 "No such file or directory"。

---

### Task 2: C 基础夹具源码

**Files:**
- Create: `test_fixtures/project/c/c_base.h`
- Create: `test_fixtures/project/c/c_base.c`
- Create: `test_fixtures/project/c/c_channel.c`

用途：覆盖用例 c01（多级传播+结构体成员+全局变量）、c02（channel_read 入口）、c03（框架参数过滤）。对应验收点见 Task 7 的 TEST_PLAN.md。

- [ ] **Step 1: 写 `c_base.h`**

```c
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
```

- [ ] **Step 2: 写 `c_base.c`**

```c
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
```

- [ ] **Step 3: 写 `c_channel.c`**

```c
#include <string.h>

struct hal_req {
    unsigned int cmd;
    unsigned int size;
    char data[64];
};

extern int shm_read_block(void *dst, unsigned int len);  /* 信道读取（项目外实现） */

static int hal_process_cmd(const char *p, unsigned int n)
{
    return (int)(p[0] + n);
}

int hal_poll_shared(void)
{
    struct hal_req req;
    if (shm_read_block(&req, sizeof(req)) != 0) {
        return -1;                       /* 信道读取: req 成为初始污点 */
    }
    return hal_process_cmd(req.data, req.size);
}

int hal_dispatch(void *ctx, struct hal_req *req)
{
    (void)ctx;                           /* ctx: 框架上下文指针，仅透传 */
    if (req->size > sizeof(((struct hal_req *)0)->data)) {
        return -1;                       /* 校验：长度上限 */
    }
    return hal_process_cmd(req->data, req->size);
}
```

- [ ] **Step 4: 语法检查（若 gcc 可用，否则跳过此步）**

Run: `if command -v gcc >/dev/null; then gcc -fsyntax-only test_fixtures/project/c/c_base.c test_fixtures/project/c/c_channel.c && echo SYNTAX_OK; else echo GCC_UNAVAILABLE; fi`
Expected: 输出 `SYNTAX_OK`（或 `GCC_UNAVAILABLE` 时跳过）。

- [ ] **Step 5: 保存 + 自检**

Read 三个文件复核与计划一致；确认 `hal_process_cmd` 为 `static` 仅在本文件使用、`shm_read_block` 只有声明无定义（这是 c02 用例的关键前提）。

---

### Task 3: C++ 夹具源码 A（虚函数、重载）

**Files:**
- Create: `test_fixtures/project/cpp/vtable.h`
- Create: `test_fixtures/project/cpp/vtable.cpp`
- Create: `test_fixtures/project/cpp/overload.cpp`

用途：覆盖 cpp01（多覆写动态派发）、cpp02（静态确定单目标）、cpp03（重载）。

- [ ] **Step 1: 写 `vtable.h`**

```cpp
#ifndef VTABLE_H
#define VTABLE_H

namespace isp { namespace hal {

class BusBase {
public:
    virtual int transfer(const char *data, unsigned int len);
    virtual ~BusBase() {}
};

class SpiBus : public BusBase {
public:
    int transfer(const char *data, unsigned int len) override;
};

class I2cBus : public BusBase {
public:
    int transfer(const char *data, unsigned int len) override;
};

class UartBus : public BusBase {
public:
    int transfer(const char *data, unsigned int len) override;
};

int bus_send(BusBase *bus, const char *data, unsigned int len);
int bus_send_static(const char *data, unsigned int len);

} }

#endif
```

- [ ] **Step 2: 写 `vtable.cpp`**

```cpp
#include "vtable.h"
#include <string.h>

namespace isp { namespace hal {

static int g_bus_sink;

int BusBase::transfer(const char *data, unsigned int len)
{
    (void)data;
    g_bus_sink += (int)len;          /* 基类默认实现 */
    return 0;
}

int SpiBus::transfer(const char *data, unsigned int len)
{
    char tmp[32];
    unsigned int n = len > 31 ? 31 : len;   /* 校验 */
    memcpy(tmp, data, n);                    /* data -> tmp */
    g_bus_sink += tmp[0];
    return (int)n;
}

int I2cBus::transfer(const char *data, unsigned int len)
{
    g_bus_sink += (int)(len + (unsigned int)data[0]);
    return 0;
}

int UartBus::transfer(const char *data, unsigned int len)
{
    g_bus_sink += (int)((unsigned int)data[0] * 2);
    (void)len;
    return 0;
}

int bus_send(BusBase *bus, const char *data, unsigned int len)
{
    return bus->transfer(data, len);   /* 分发点：不可静态确定 → 枚举全部覆写 + 基类默认实现 */
}

int bus_send_static(const char *data, unsigned int len)
{
    SpiBus spi;                        /* 局部具体对象 → 静态可确定 */
    return spi.transfer(data, len);    /* 仅解析 SpiBus::transfer */
}

} }
```

- [ ] **Step 3: 写 `overload.cpp`**

```cpp
namespace isp { namespace util {

int convert(int v);
int convert(const char *s);
int convert(int v, int scale);

int convert(int v)
{
    return v * 2;
}

int convert(const char *s)
{
    return (int)s[0];
}

int convert(int v, int scale)
{
    return v * scale;
}

int entry_overload(const char *s, unsigned int n)
{
    int a = convert(s);            /* 重载: const char* */
    int b = convert((int)n);       /* 重载: int */
    int c = convert((int)n, a);    /* 重载: (int, int) */
    return a + b + c;
}

} }
```

- [ ] **Step 4: 语法检查（若 g++ 可用，否则跳过此步）**

Run: `if command -v g++ >/dev/null; then g++ -fsyntax-only -std=c++11 test_fixtures/project/cpp/vtable.cpp test_fixtures/project/cpp/overload.cpp && echo SYNTAX_OK; else echo GXX_UNAVAILABLE; fi`
Expected: 输出 `SYNTAX_OK`（或 `GXX_UNAVAILABLE` 时跳过）。

- [ ] **Step 5: 保存 + 自检**

Read 复核；确认 vtable.cpp 中三个派生类覆写定义 + BusBase 默认实现都在本文件内（cpp01 候选集完整）。

---

### Task 4: C++ 夹具源码 B（模板、构造/拷贝/运算符/隐式转换）

**Files:**
- Create: `test_fixtures/project/cpp/templates.cpp`
- Create: `test_fixtures/project/cpp/ctor_conv.cpp`

用途：覆盖 cpp04（模板）、cpp05（构造/拷贝构造/operator=/operator[]）、cpp06（隐式转换）。

- [ ] **Step 1: 写 `templates.cpp`**

```cpp
namespace isp { namespace util {

template <typename T>
class RingBuf {
public:
    void put(T v) { m_v = v; }
    T get(void) const { return m_v; }
private:
    T m_v;
};

template <typename T>
T clamp_val(T v) { return v > (T)0 ? v : (T)0; }

int entry_template(int raw)
{
    RingBuf<int> rb;
    rb.put(raw);                  /* 模板成员 put */
    int c = clamp_val(raw);       /* 函数模板 */
    return rb.get() + c;          /* 模板成员 get */
}

} }
```

- [ ] **Step 2: 写 `ctor_conv.cpp`**

```cpp
namespace isp { namespace hal {

class Frame {
public:
    Frame(unsigned int id, const char *data, unsigned int len);
    Frame(const Frame &o);
    Frame &operator=(const Frame &o);
    char &operator[](unsigned int i);
    unsigned int m_id;
    char m_data[32];
};

Frame::Frame(unsigned int id, const char *data, unsigned int len)
{
    unsigned int n = len > 31 ? 31 : len;
    m_id = id;
    for (unsigned int i = 0; i < n; i++) {
        m_data[i] = data[i];      /* data/len 污点 -> 成员 */
    }
}

Frame::Frame(const Frame &o)
{
    m_id = o.m_id;
    for (int i = 0; i < 32; i++) {
        m_data[i] = o.m_data[i];
    }
}

Frame &Frame::operator=(const Frame &o)
{
    m_id = o.m_id;
    m_data[0] = o.m_data[0];
    return *this;
}

char &Frame::operator[](unsigned int i)
{
    return m_data[i % 32];        /* i 污点作索引 */
}

int entry_frame(unsigned int id, const char *data, unsigned int len, unsigned int sel)
{
    Frame f(id, data, len);       /* 构造 */
    Frame g = f;                  /* 拷贝构造 */
    Frame h(0, "", 0);
    h = g;                        /* operator= */
    return (int)h[sel];           /* operator[] */
}

class Tag {
public:
    Tag(int v) { m_v = v; }       /* 带参构造（可隐式转换） */
    int m_v;
};

int tag_sink(Tag t)
{
    return t.m_v;
}

int entry_implicit(int raw)
{
    return tag_sink(raw);         /* int -> Tag 隐式转换 + 按值传参 */
}

} }
```

- [ ] **Step 3: 语法检查（若 g++ 可用，否则跳过）**

Run: `if command -v g++ >/dev/null; then g++ -fsyntax-only -std=c++11 test_fixtures/project/cpp/templates.cpp test_fixtures/project/cpp/ctor_conv.cpp && echo SYNTAX_OK; else echo GXX_UNAVAILABLE; fi`
Expected: `SYNTAX_OK`。

- [ ] **Step 4: 保存 + 自检**

Read 复核；确认 Frame 的两个构造函数定义起始行不同（cpp05 靠 begin_line 区分同名构造）。

---

### Task 5: C++ 夹具源码 C（智能指针、lambda、多重继承）

**Files:**
- Create: `test_fixtures/project/cpp/smart_lambda.cpp`
- Create: `test_fixtures/project/cpp/mi.cpp`

用途：覆盖 cpp07（智能指针 + 静态确定虚调用）、cpp08（lambda）、cpp09（多重继承覆写枚举）。

- [ ] **Step 1: 写 `smart_lambda.cpp`**

```cpp
#include <memory>

namespace isp { namespace hal {

struct Pkt {
    unsigned int len;
    char payload[32];
};

class Sink {
public:
    virtual int feed(const char *p, unsigned int n);
    virtual ~Sink() {}
};

class LogSink : public Sink {
public:
    int feed(const char *p, unsigned int n) override;
};

int Sink::feed(const char *p, unsigned int n)
{
    (void)p;
    return (int)n;
}

int LogSink::feed(const char *p, unsigned int n)
{
    return (int)(p[0] + n);
}

int entry_smart(const char *data, unsigned int len)
{
    std::shared_ptr<Sink> s = std::make_shared<LogSink>();  /* 构造来源可确定 → LogSink */
    std::shared_ptr<Pkt> pkt = std::make_shared<Pkt>();
    pkt->len = len;                       /* operator-> 穿透：成员赋值 */
    unsigned int n = len > 31 ? 31 : len;
    for (unsigned int i = 0; i < n; i++) {
        pkt->payload[i] = data[i];
    }
    return s->feed(pkt->payload, pkt->len);   /* 虚调用，静态可确定 */
}

int entry_lambda(const char *data, unsigned int len)
{
    auto f = [data, len](unsigned int i) {
        return (int)(data[i % len] + len);    /* lambda 体: 污点经捕获进入 */
    };
    return f(1);
}

} }
```

- [ ] **Step 2: 写 `mi.cpp`**

```cpp
namespace isp { namespace hal {

class TimerBase {
public:
    virtual int on_tick(unsigned int v) { return (int)v; }   /* 基类默认实现 */
    virtual ~TimerBase() {}
};

class CtrlBase {
public:
    virtual int on_ctrl(unsigned int v) { return (int)v; }   /* 基类默认实现 */
    virtual ~CtrlBase() {}
};

class DualUnit : public TimerBase, public CtrlBase {
public:
    int on_tick(unsigned int v) override;
    int on_ctrl(unsigned int v) override;
};

int DualUnit::on_tick(unsigned int v)
{
    return (int)(v * 3);
}

int DualUnit::on_ctrl(unsigned int v)
{
    return (int)(v + 7);
}

int entry_mi(CtrlBase *c, unsigned int v)
{
    return c->on_ctrl(v);        /* 分散点：候选 = DualUnit::on_ctrl + CtrlBase::on_ctrl */
}

} }
```

- [ ] **Step 3: 语法检查（若 g++ 可用，否则跳过）**

Run: `if command -v g++ >/dev/null; then g++ -fsyntax-only -std=c++11 test_fixtures/project/cpp/smart_lambda.cpp test_fixtures/project/cpp/mi.cpp && echo SYNTAX_OK; else echo GXX_UNAVAILABLE; fi`
Expected: `SYNTAX_OK`。

- [ ] **Step 4: 保存 + 自检**

Read 复核；确认 mi.cpp 中 `CtrlBase::on_ctrl` 为类内内联定义（cpp09 基类默认实现候选）。

---

### Task 6: 机制类夹具源码

**Files:**
- Create: `test_fixtures/project/mech/dedup.c`
- Create: `test_fixtures/project/mech/limits.cpp`
- Create: `test_fixtures/project/mech/cycle.c`
- Create: `test_fixtures/project/mech/external.c`

用途：覆盖 m01（子序列去重）、m02（深度截断）、m03（扇出截断）、m04（环终止）、m05（项目外源码缺失）。

- [ ] **Step 1: 写 `dedup.c`**

```c
int m_leaf_c(int v)
{
    return v;
}

int m_b(int v)
{
    return m_leaf_c(v);
}

int m_entry(int v)
{
    int r = m_b(v);        /* 边 E->B */
    r += m_leaf_c(v);      /* 边 E->C（直调） */
    return r;
}
```

- [ ] **Step 2: 写 `limits.cpp`**

```cpp
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
```

- [ ] **Step 3: 写 `cycle.c`**

```c
int cyc_a(int v);

int cyc_b(int v)
{
    return cyc_a(v + 1);     /* B -> A */
}

int cyc_a(int v)
{
    return cyc_b(v - 1);     /* A -> B：环 */
}

int entry_cycle(int v)
{
    return cyc_a(v);         /* E -> A -> B -> (A 重复，环终止) */
}
```

- [ ] **Step 4: 写 `external.c`**

```c
extern int vendor_crypto_sign(int v);   /* 实现在项目外，无源码 */

int entry_ext(int v)
{
    return vendor_crypto_sign(v);       /* 污点传到项目外 → 无法追踪 */
}
```

- [ ] **Step 5: 语法检查（若编译器可用，否则跳过）**

Run: `if command -v gcc >/dev/null; then gcc -fsyntax-only test_fixtures/project/mech/dedup.c test_fixtures/project/mech/cycle.c test_fixtures/project/mech/external.c && echo C_SYNTAX_OK; else echo GCC_UNAVAILABLE; fi; if command -v g++ >/dev/null; then g++ -fsyntax-only -std=c++11 test_fixtures/project/mech/limits.cpp && echo CPP_SYNTAX_OK; else echo GXX_UNAVAILABLE; fi`
Expected: `SYNTAX_OK`。

- [ ] **Step 6: 保存 + 自检**

Read 复核；确认 `limits.cpp` 中 `op_fn` 表恰好 21 项、`external.c` 的 `vendor_crypto_sign` 全项目仅此一处 extern 声明（无定义）。

---

### Task 7: 入口文件（entries/）与 TEST_PLAN.md 初稿

**Files:**
- Create: `test_fixtures/entries/{c01,c02,c03,cpp01..cpp09,m01..m05,bad01,bad02}_entry.json`（19 个）
- Create: `test_fixtures/TEST_PLAN.md`

- [ ] **Step 1: 写 17 个正常入口文件**

全部位于 `test_fixtures/entries/`，`file` 字段使用绝对路径前缀 `/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/`：

```json
// c01_entry.json
{"name": "hal_apply_cfg", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/c/c_base.c", "form": "parameter_input", "taint_data": "cfg", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0"}

// c02_entry.json
{"name": "hal_poll_shared", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/c/c_channel.c", "form": "channel_read", "taint_data": "共享内存 shm_read_block -> req", "local_module": "ISP 固件", "partner_module": "DDR 控制器", "para_index": "-1"}

// c03_entry.json
{"name": "hal_dispatch", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/c/c_channel.c", "form": "parameter_input", "taint_data": "ctx/req", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0/1"}

// cpp01_entry.json
{"name": "isp::hal::bus_send", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/vtable.cpp", "form": "parameter_input", "taint_data": "data/len", "local_module": "ISP 固件", "partner_module": "Sensor 总线", "para_index": "1/2"}

// cpp02_entry.json
{"name": "isp::hal::bus_send_static", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/vtable.cpp", "form": "parameter_input", "taint_data": "data/len", "local_module": "ISP 固件", "partner_module": "Sensor 总线", "para_index": "0/1"}

// cpp03_entry.json
{"name": "isp::util::entry_overload", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/overload.cpp", "form": "parameter_input", "taint_data": "s/n", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0/1"}

// cpp04_entry.json
{"name": "isp::util::entry_template", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/templates.cpp", "form": "parameter_input", "taint_data": "raw", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0"}

// cpp05_entry.json
{"name": "isp::hal::entry_frame", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/ctor_conv.cpp", "form": "parameter_input", "taint_data": "id/data/len/sel", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0/1/2/3"}

// cpp06_entry.json
{"name": "isp::hal::entry_implicit", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/ctor_conv.cpp", "form": "parameter_input", "taint_data": "raw", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0"}

// cpp07_entry.json
{"name": "isp::hal::entry_smart", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/smart_lambda.cpp", "form": "parameter_input", "taint_data": "data/len", "local_module": "ISP 固件", "partner_module": "Sensor 总线", "para_index": "0/1"}

// cpp08_entry.json
{"name": "isp::hal::entry_lambda", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/smart_lambda.cpp", "form": "parameter_input", "taint_data": "data/len", "local_module": "ISP 固件", "partner_module": "Sensor 总线", "para_index": "0/1"}

// cpp09_entry.json
{"name": "isp::hal::entry_mi", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/cpp/mi.cpp", "form": "parameter_input", "taint_data": "v", "local_module": "ISP 固件", "partner_module": "Sensor 总线", "para_index": "1"}

// m01_entry.json
{"name": "m_entry", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/mech/dedup.c", "form": "parameter_input", "taint_data": "v", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0"}

// m02_entry.json
{"name": "entry_deep", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/mech/limits.cpp", "form": "parameter_input", "taint_data": "v", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0"}

// m03_entry.json
{"name": "entry_fanout", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/mech/limits.cpp", "form": "parameter_input", "taint_data": "idx/v", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0/1"}

// m04_entry.json
{"name": "entry_cycle", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/mech/cycle.c", "form": "parameter_input", "taint_data": "v", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "0"}

// m05_entry.json
{"name": "entry_ext", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/mech/external.c", "form": "parameter_input", "taint_data": "v", "local_module": "ISP 固件", "partner_module": "外部签名模块", "para_index": "0"}
```

注意：每个文件单独写入，不要在一个 JSON 文件里写多行注释（上面用 `// 文件名` 仅为计划标注）。JSON 内容为单行对象即可。

- [ ] **Step 2: 写 2 个错误类入口文件**

```json
// bad01_entry.json —— 故意缺 form 字段
{"name": "hal_apply_cfg", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/c/c_base.c", "taint_data": "cfg", "para_index": "0"}

// bad02_entry.json —— 入口函数不存在
{"name": "not_exist_fn", "file": "/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project/c/c_base.c", "form": "parameter_input", "taint_data": "v", "para_index": "0"}
```

- [ ] **Step 3: 写 `TEST_PLAN.md` 初稿（含全部用例验收点）**

```markdown
# taint-path-walker 系统测试计划

## 测试项目结构

test_fixtures/project/
├── c/       c_base.{h,c}  c_channel.c          # C 基础用例
├── cpp/     vtable.{h,cpp} overload.cpp templates.cpp ctor_conv.cpp smart_lambda.cpp mi.cpp
├── mech/    dedup.c limits.cpp cycle.c external.c   # 机制类用例
test_fixtures/entries/    # 各用例入口文件 {TreeID}_entry.json
test_fixtures/expected/   # 验收通过后录制的基线 {TreeID}.json

## 开发期运行方式

skill 未安装为命令时：Read 全文 taint-path-walker/SKILL.md，严格按其流程以
project_dir = test_fixtures/project、entry_json = test_fixtures/entries/<用例> 执行一次完整分析。
每次重跑前：rm -rf test_fixtures/project/.ethunter_out/taint-path-walker

## 用例与验收点（主验收依据：必须成立的链集合与解析结论）

| 用例 | 入口 | 验收点 |
|------|------|--------|
| c01 | hal_apply_cfg | 链集合恰好 = { [hal_apply_cfg, hal_cfg_step1, hal_cfg_step2], [hal_apply_cfg, hal_cfg_step3], [hal_apply_cfg, hal_cfg_check] }；[E,hal_cfg_step2] 直调链被去重但其调用点并入链1（链1 的 E.calls 含 step1 与 step2 两条；[E,step3] 的 E.calls 为 step3；[E,check] 的 E.calls 为 check 且 taint_args 为空数组）；step2 内的校验不阻挡传播 |
| c02 | hal_poll_shared | 恰好 1 条链 [hal_poll_shared, hal_process_cmd]；不得出现 shm_read_block 节点；calls.taint_args = [p, n] |
| c03 | hal_dispatch | ctx（框架指针）未产生任何追踪；恰好 1 条链 [hal_dispatch, hal_process_cmd] |
| cpp01 | isp::hal::bus_send | 分发点解析出 4 个候选：BusBase::transfer、SpiBus::transfer、I2cBus::transfer、UartBus::transfer；输出 4 条链 |
| cpp02 | isp::hal::bus_send_static | 恰好 1 条链 [bus_send_static, SpiBus::transfer]；不得出现其他覆写或基类 |
| cpp03 | isp::util::entry_overload | 3 条链，convert 三个重载各一条（按 begin_line 区分）；callee 同名不被误合并 |
| cpp04 | isp::util::entry_template | 3 条兄弟链：entry_template→RingBuf::put（taint_args [v]）、→clamp_val（[v]）、→RingBuf::get（[]，污点经接收对象成员态进入）；定义处限定名，不写 <int> 等实例化参数 |
| cpp05 | isp::hal::entry_frame | 4 条链，分别指向 Frame::Frame(3参)构造、Frame::Frame(拷贝)构造、Frame::operator=、Frame::operator[]；两个构造按 begin_line 区分；不传污点的构造调用点（Frame h(0,"",0) 所在行）不得进入 calls |
| cpp06 | isp::hal::entry_implicit | 2 条兄弟链：[entry_implicit, Tag::Tag]（隐式转换构造，taint_args [v]）与 [entry_implicit, tag_sink]（[t]），两条边源于同一调用行 |
| cpp07 | isp::hal::entry_smart | s->feed 精确解析为 LogSink::feed（唯一目标，不得枚举 Sink::feed）；pkt-> 成员写入经穿透处理（不得出现 shared_ptr::operator-> 节点） |
| cpp08 | isp::hal::entry_lambda | lambda 节点名形如 <lambda@cpp/smart_lambda.cpp:行号>；calls.taint_args 为空数组（污点经捕获传入） |
| cpp09 | isp::hal::entry_mi | 恰好 2 条链：DualUnit::on_ctrl 与 CtrlBase::on_ctrl（多重继承合并 + 基类默认实现） |
| m01 | m_entry | 输出恰好 1 条链 [m_entry, m_b, m_leaf_c]；m_entry.calls 含 m_b 与 m_leaf_c 两个调用点条目 |
| m02 | entry_deep | 链 [entry_deep, deep0..deep8] 长度恰 10；state 截断记录含 deep8->deep9 |
| m03 | entry_fanout | 20 条链 [entry_fanout, op_00..op_19]；state 截断记录含 op_20 |
| m04 | entry_cycle | 链 [entry_cycle, cyc_a, cyc_b]；state 环终止记录；不再延伸 |
| m05 | entry_ext | 单元素链 [entry_ext]；state untraceable 含 vendor_crypto_sign（reason=源码缺失，tried 非空） |
| resume | c01 | 完成态 state + 删除输出后重跑：输出与直跑一致（走恢复路径）；构造半成品 state（末条终态路径移回 queue）重跑：输出仍与直跑一致 |
| bad01 | 缺 form | 报错并结束；不生成 {TreeID}.json；错误信息包含缺失字段 |
| bad02 | 入口不存在 | 报错并结束；不生成 {TreeID}.json；错误信息包含函数名 |

## 运行前注意

- 重跑前必须删除 test_fixtures/project/.ethunter_out/taint-path-walker/（否则触发预查重跳过）。
- 若仓库目录被移动，刷新 entries 中的绝对路径：
  OLD=/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project
  NEW=$(cd test_fixtures/project && pwd)
  grep -rl "$OLD" test_fixtures/entries | xargs sed -i "s|$OLD|$NEW|g"
```

- [ ] **Step 4: 验证 JSON 合法性**

Run: `cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker && for f in test_fixtures/entries/*.json; do python3 -c "import json,sys; json.load(open('$f'))" || echo "INVALID: $f"; done; ls test_fixtures/entries | wc -l`
Expected: 无 INVALID 输出；文件数为 19。

- [ ] **Step 5: 保存 + 自检**

Read 复核 TEST_PLAN.md 与入口文件；确认 19 个文件名均符合 `{TreeID}_entry.json` 模式（TreeID 分别为 c01、c02、c03、cpp01..cpp09、m01..m05、bad01、bad02）。

---

### Task 8: SKILL.md 骨架 + 第一章 + 第二章

**Files:**
- Create: `taint-path-walker/SKILL.md`

先读设计文档 §1、§3、§4、§5、§13 作为写作依据（`docs/superpowers/specs/2026-10-14-taint-path-walker-design.md`）。SKILL.md 是写给"运行时主 agent"的提示词：命令式、规则化、可执行；全中文；术语严格采用设计文档 §13 术语表（下同）。

- [ ] **Step 1: 写 frontmatter + 第一章**

frontmatter 逐字采用：

```markdown
---
name: taint-path-walker
description: 使用命令 /taint-path-walker <project_dir> <entry_json> 触发。根据接口函数与污点 source 参数，追踪 C/C++ 代码中的污点传播路径，生成污点树（多条调用链，逐链记录调用点与被传入污点的形参），供 taint-tree-checker 消费。适用于嵌入式系统 HAL 层代码（用户态，上接嵌入式操作系统 service，下接内核驱动），重点处理 C++ 特性（虚函数、多态、重载、模板、构造析构、运算符重载等）保证污点传播不断链。
---
```

第一章"角色与任务概述"须包含：角色定位（嵌入式 C/C++ 污点追踪专家）；任务（从入口函数与初始污点出发自行追踪污点传播，生成污点树）；产物与下游关系（`{TreeID}.json` 将作为 taint-tree-checker 的输入）；适用业务（HAL 层）；核心原则（原样写入）：**追踪完备优先——不到追踪上限不自行停止；宁可多枚举目标不可断链；不可臆造（无法追踪必须标注并写明已尝试的搜索方式）**；明确"本 skill 不判定漏洞"。

- [ ] **Step 2: 写第二章"输入解析与初始化"**

须覆盖（依据设计文档 §3、§5；规则用编号列出，措辞命令式）：

1. 命令格式 `/taint-path-walker <project_dir> <entry_json>`；两参数均必填，缺失或只给一个 → 打印用法提示并结束。
2. entry json 七个字段的表格（§5 字段表原意）；六个校验规则（§5 校验规则 1-6，逐条写出），校验失败 → 向用户报错并结束、不产出树文件。
3. TreeID 规则：文件名去掉 `_entry.json`；不符合 `{TreeID}_entry.json` → 报错结束。
4. 输出目录 `<project_dir>/.ethunter_out/taint-path-walker/`：`mkdir -p` 创建；失败降级当前目录；预查重：`{TreeID}.json` 已存在 → 打印"该入口已有分析结果：{路径}，跳过分析。"并结束。
5. macro.json：双方法检查 `<project_dir>/.ethunter_out/macro.json`，存在则加载（`#ifdef` 判断时参考），不存在跳过。
6. codegraph 检测：双方法检查 `<project_dir>/.codegraph`；存在 → 优先 `codegraph_explore`；不存在/未安装/未配置 MCP → 常规工具（grep/find/Read/ast-grep）代替，不中断分析。
7. 分析范围三情况（§3.4 A/B/C，含 find 命令原文，扩展名：c h cpp cxx cc hpp hxx hh inl ipp）。
8. 路径规范：分隔符统一 `/`；去掉前导 `./`；含 `..` 非法（报错结束）；输出 file 一律 rel。
9. 文件存在性检查规则：双方法交叉确认（`ls` + `Read`），**禁止 `test -f`/`[ -f ]`**；不一致取第三方多数；两次都无输出/都报错 → 停止分析并报告。规则表格照 §3.4 末条写。

- [ ] **Step 3: 自检**

通读两章：核对术语与设计文档 §13 一致；每条规则可执行（无"适当处理"类模糊措辞）；引用设计文档 §5 的字段与校验规则无遗漏。发现偏差立即修正。

---

### Task 9: SKILL.md 第三章"初始污点标记"

**Files:**
- Modify: `taint-path-walker/SKILL.md`（在第二章后追加）

先读设计文档 §7.1。须覆盖：

1. `parameter_input`/`both`：按 `para_index`（`/` 分割，0-based，与 `taint_data` 项逐项对应）找到对应位置的形参，标记初始污点；**内部参数二次过滤**（语义判断，给例）：框架/内核回调上下文指针（如 `void *ctx` 仅透传）、纯输出参数（只写不读）、常量类参数 → 剔除不追踪。明确"即使 para_index 标出了该参数，语义判断为内部参数仍须剔除"。
2. `channel_read`（para_index 为 `-1` 项）：按 `taint_data` 描述在入口函数体内定位信道读取点（例：`msgrcv → msg_buf`、`shm_read_block() 填充 req`、共享内存指针），把读取结果变量标记为初始污点；定位不到 → 报错并结束（输入与代码不符）。明确"信道读取调用本身不产生调用边（数据是读出方向）"。
3. `both`：两部分都执行并合并初始污点集合。
4. 全局/静态变量：污点写入后视为跨函数共享状态，树内后续函数读取视为污点传播，直至被覆盖；C++ static 成员同规则。
5. `this` 本身非污点；其指向成员按结构体成员规则传播。

- [ ] **Step 1: 写第三章**（按上述 5 点，规则化）
- [ ] **Step 2: 自检**：通读本charpter，确认五点齐全、与 §7.1 无出入、术语一致。

---

### Task 10: SKILL.md 第四章"污点追踪与污点树生成"（核心章，分 3 步写）

**Files:**
- Modify: `taint-path-walker/SKILL.md`（第三章后追加）

先读设计文档 §7.2–§7.6。本章是流程主体，必须自包含（运行时不查阅其他 skill 文档）。分三步写入。

- [ ] **Step 1: 写 4.1 污点传播模式清单（自包含转写）**

从 `taint_path_checker/taint-tree-checker/SKILL.md` 第四章 4.2"污点传播模式清单"**转写**五类模式表到本 SKILL.md（不得引用外部路径）：（1）直接传播；（2）指针与内存操作；（3）全局变量与静态变量；（4）结构体与联合体；（5）嵌入式特有模式。保留各表"模式/示例/说明"三列结构与全部行。表后加"自定义函数识别"段：按函数语义（命名、参数模式、上下文）识别封装函数，不只看函数名。并追加以一行："C++ 特有传播模式见 4.3 节。"

- [ ] **Step 2: 写 4.2 工作队列追踪模型 + 4.3 C++ 特性处理专节**

4.2 须覆盖（依据 §7.2，写成可执行流程）：

- 工作项定义 = （链前缀 path，当前函数 F，F 内污点变量前沿 frontier）；初始工作项 = （`[入口]`，入口函数，初始污点集合）。
- 处理流程（编号列出）：完整阅读 F 全部代码（含所有分支）→ 对 frontier 每个污点变量追踪函数内传播（赋值/成员/指针/引用/分支）→ 按四种去向处理：传污点给函数 g 的调用点 → 建调用边（F→g、行号、taint_args）并入队新工作项；污点消亡（被非污点值覆盖/不再使用）→ 支线截止于 F；分发点 → 先解析候选（见 4.3 规则与上限）再逐一入队；项目外无源码/目标不可解析 → 记"无法追踪清单"（含已尝试的搜索方式），链截止于本函数。
- 调用点集合的归属：元素 X 的 `calls` 收录 X 向本链中位于 X 之后的全部元素的传污点调用点（同一后继的多次调用合并、按行号升序）；`callee` 取相应后继元素的限定名（含被去重吞并的直调边）。`taint_args` 为空数组的三种合法情形（污点经全局/静态变量进入；污点经接收对象成员态进入且被调函数无污点形参；污点经 lambda 捕获进入）。
- 路径→树→链：叶节点路径构成污点树；全部工作项处理后，叶路径即候选链。
- 防环：同一链中出现相同函数定义（同 file+begin_line）→ 停止延伸该分支，记录"环终止"。
- **校验不终止传播**：值经校验/净化后仍来源于外部输入，继续追踪；仅当被非污点值覆盖或消亡才终止。

4.3 C++ 特性处理专节：将设计文档 §7.4 的 12 项表格原意转写为可执行规则（逐行保留处理策略要点），并以"处理原则"段收尾：**宁可多枚举目标（上限内）不可断链；静态可确定时精确解析，避免路径爆炸；模板不逐实例展开；外部库无源码不臆造**。补充实施细则（必须明确写出）：
- 虚函数候选集 = 全项目内该函数的所有覆写实现 + 基类非纯虚默认实现；多重/虚继承合并所有基类来源；虚调用静态可确定（局部具体对象、`new`/`make_shared<具体类>`、明确类型成员或全局对象、实参具体类型）→ 只追踪确定目标。
- 覆写枚举方法：grep 派生类声明（`: public X`、`: X`、`override`）后逐个核对是否覆写目标虚函数；codegraph 可用时优先。
- 解析结果写入 dispatch_cache，同一分发点不重复枚举。
- 重载：按实参数目/类型匹配签名；无法确定 → 全部同名候选逐一追踪（受扇出上限）。
- 模板：按定义体追踪一次，函数名用定义处限定名（不带 `<实例化参数>`）；存在匹配特化则追踪特化。
- 构造函数/拷贝构造/移动构造/析构：识别隐式调用（按值传参、返回对象、成员初始化列表、声明即构造、临时对象、隐式转换）；构造函数体内污点去向（存成员）继续追踪。
- 运算符重载与智能指针穿透：`operator=`/`operator[]`/`operator()`/`operator->` 等识别为函数调用；智能指针 `->` 直接穿透到目标对象成员/方法（不产生 operator-> 节点）。
- 引用与别名、lambda（命名 `<lambda@相对路径:行号>`，捕获按引用=别名）、成员函数指针/回调（注册点解析）、成员变量（static 成员按全局变量）、类型转换（不消除污点）、隐式转换（按函数调用追踪）。

- [ ] **Step 3: 写 4.4 追踪上限 + 4.5 完整性与自检**

4.4（依据 §7.5，数值逐字）：深度上限 10 层（入口为第 1 层；第 10 层仍传播 → 停止延伸，链截止于边界节点，记录截断：函数名+行号+传播去向）；扇出上限 20（单个分发点按声明/注册/表项顺序取前 20 完整追踪，其余逐条记录函数名+行号）；"不到上限不要自行停止"；"截断≠终止"。

4.5（依据 §7.6）：终态四类（传播停止/离开项目范围/分发点已完整枚举或截断有记录/无法追踪）逐条列出；自检项三条（分支是否未入队、调用点是否遗漏——回调用方源码逐点核对、链是否断链）；"自检不通过不得进入输出阶段"。

- [ ] **Step 4: 自检**

通读第四章：传播模式表与 taint-tree-checker 4.2 对照（五类齐全）；C++ 12 项与设计 §7.4 对照无遗漏；上限数值出现且为 10/20；术语（工作项/前沿/分发点/截断/环终止）与术语表一致。

---

### Task 11: SKILL.md 第五章"输出"

**Files:**
- Modify: `taint-path-walker/SKILL.md`（第四章后追加）

先读设计文档 §6。须逐字包含输出 JSON 示例（§6 示例原文），并把 10 条规则清单转写为提示词规则。补充明确（本次计划新增的澄清，必须写入）：

- 去重的序列比较基于**链元素三元组（func, file, begin_line）**，不是仅函数名——例如同名重载的两个不同函数不得被误判为同序列。
- `taint_args` 可为空数组（三种情形见 4.2：全局/静态变量、接收对象成员态、lambda 捕获）。
- `calls` 归属语义：元素 X 收录 X 向本链中位于 X 之后的全部元素的传污点调用点；`callee` 取本链中相应后继元素的限定名（含被去重吞并的直调边）。
- 单元素链合法（`[[入口]]`）。
- 链排序：按逐元素 `file:begin_line` 序列字典序升序。
- 写盘时机：完整性自检通过后一次性写出（UTF-8，缩进 4 空格）；中断只留中间状态。
- 输出中禁止出现绝对路径（file 一律 rel）。

- [ ] **Step 1: 写第五章**
- [ ] **Step 2: 自检**：JSON 示例与 §6 逐字一致；10 条规则 + 6 条补充全部在文中；示例中顶层键 = 链首元素 func + `$` + TreeID 的关系写明。

---

### Task 12: SKILL.md 第六~十章

**Files:**
- Modify: `taint-path-walker/SKILL.md`（第五章后追加）

先读设计文档 §8、§9、§11、§12 与 §8.4。

- [ ] **Step 1: 写第六章"断点续分析与防遗忘"**

须覆盖：状态文件路径 `tmp/{TreeID}_state.json`；状态文件 JSON 结构示例（§8.1 原文，含 end_reason 与取值说明）；保存时机（每处理完一个工作项立即落盘）；恢复流程四分支（§8.2，含预查重跳过提示原文）；防遗忘四机制（§8.3）。

- [ ] **Step 2: 写第七章"执行检查清单"**

按设计 §8.4 检查清单逐条列出（10 条：phase=done、输出存在、首元素与顶层键一致、calls 非空且 callee 指向本链后续元素、行号已核对、去重已执行、排序已执行、截断/环终止/无法追踪已记录、file 为 rel、边缘情形已按 6.7/6.8 处理），并加"输出文件不存在立即补做"。

- [ ] **Step 3: 写第八章"权限申请"**

表格照 §11 原文（Bash/Read/Write/codegraph MCP 可选/ast-grep 可选），附"codegraph 未安装/未配置 MCP → 常规工具代替，不中断分析"。

- [ ] **Step 4: 写第九章"自检提醒"**

（1）思维陷阱表：以设计 §8.4 的 7 行示例为基础扩展为完整表格；（2）边缘情况处理表：转写设计 §9 的全部 11 行。

- [ ] **Step 5: 写第十章"约束规则"**

（1）所有分析由主 agent 完成，不调用 sub-agent；（2）分析质量优先于效率；（3）不在输出中评定漏洞（只产出树）；（4）全中文、术语一致（引用术语表并入本章或独立节）；（5）状态文件每工作项落盘；（6）文件存在性检查双方法规则照 §3.4 执行。

- [ ] **Step 6: 全文终检**

通读 SKILL.md 全文：十章齐全且与设计 §4 大纲一致；术语全篇一致（对照 §13 术语表逐词检索：污点/初始污点/污点源/污点传播/污点树/链/链元素/调用边/调用点/污点形参/分发点/覆写/限定名/工作项/前沿/终态/截断/环终止/无法追踪清单/子序列去重）；无"TBD/TODO/类似上节"等占位；无矛盾（如上限数值、去重规则、写盘时机在全文各处一致）。修正发现的问题。

---

### Task 13: 端到端验收 — C 用例（c01、c02、c03）

**Files:**
- 输出物（运行生成）: `test_fixtures/project/.ethunter_out/taint-path-walker/{TreeID}.json` 与 `tmp/{TreeID}_state.json`

按 TEST_PLAN.md 的验收点逐用例执行。三个用例共用同一夹具工程，但每次运行只分析一个 entry。

- [ ] **Step 1: 运行 c01**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker
rm -rf test_fixtures/project/.ethunter_out/taint-path-walker
```
然后 Read 全文 `taint-path-walker/SKILL.md`，严格按其流程执行：project_dir=`test_fixtures/project`，entry_json=`test_fixtures/entries/c01_entry.json`。

- [ ] **Step 2: 核对 c01 验收点**

Read `test_fixtures/project/.ethunter_out/taint-path-walker/c01.json` 与 state 文件，对照 TEST_PLAN c01 行逐项核对（链集合、逐链 E.calls 归属、check 的 taint_args 为空数组）。

- [ ] **Step 3: 不通过则修订并重跑**

不通过时：先判断是夹具代码问题还是提示词问题——夹具问题改夹具（并同步 TEST_PLAN），提示词问题修订 SKILL.md 对应章节（第三章全局变量/第四章工作队列/第五章 calls 语义等）。修订后回到 Step 1 重跑，直至全部验收点通过。**不得为通过而放宽验收点。**

- [ ] **Step 4: 对 c02、c03 重复 Step 1-3**

c02 重点：shm_read_block 不得成为链节点（信道读出方向不建边）。c03 重点：ctx 被二次过滤。

- [ ] **Step 5: 保存**

确认三个用例的输出文件与 state 文件已落盘（保留供 Task 17 录制）。

---

### Task 14: 端到端验收 — C++ 用例（cpp01..cpp09）

**Files:**
- 输出物（运行生成）: 同上目录

- [ ] **Step 1: 运行 cpp01..cpp09 并逐一核对验收点**

每个用例：`rm -rf test_fixtures/project/.ethunter_out/taint-path-walker` → 按 SKILL.md 流程执行（entry = 对应 cppXX_entry.json）→ 对照 TEST_PLAN 该行验收点核对输出与 state。

重点项：
- cpp01：4 个候选齐全（含 BusBase 默认实现）；
- cpp02：严格单一目标（枚举了其他覆写/基类即不通过——测试"静态可确定→精确解析"规则）；
- cpp03：三个重载各自成链、不被误合并；
- cpp05：两个同名构造按 begin_line 区分；
- cpp07：精确解析 LogSink::feed、无 operator-> 节点；
- cpp08：lambda 命名与空 taint_args；
- cpp09：恰好两个候选。

- [ ] **Step 2: 不通过则修订并重跑**

按 Task 13 Step 3 的同一原则处理（夹具 vs 提示词），修订 SKILL.md 第四章 4.3（C++ 专节）或第五章等对应位置，重跑至通过。

- [ ] **Step 3: 保存**

确认 9 个用例输出齐备。

---

### Task 15: 端到端验收 — 机制用例（m01..m05）与断点续跑

**Files:**
- 输出物（运行生成）: 同上目录

- [ ] **Step 1: 运行 m01..m05 并逐一核对验收点**

流程同 Task 14 Step 1。重点：
- m01：恰好 1 条链、E.calls 两个条目（验证子序列去重 + 调用点合并）；
- m02：链长恰 10、截断记录在 state；
- m03：20 条链、op_20 截断记录；
- m04：环终止记录、不无限延伸；
- m05：单元素链 + untraceable 记录（reason=源码缺失、tried 非空）。

- [ ] **Step 2: 断点续跑一致性验证（resume 用例）**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker
# ① 先完整跑一次 c01 并保存输出为参照
cp test_fixtures/project/.ethunter_out/taint-path-walker/c01.json /tmp/c01_direct.json
# ② 模拟"分析完成但输出丢失"：删除输出、保留 state，重跑
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
```
按 SKILL.md 流程重跑 c01（state 存在 → 应走恢复路径），diff 两次输出：
```bash
diff /tmp/c01_direct.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo RESUME_OK
```
Expected: `RESUME_OK`（一致）。

- [ ] **Step 3: 半成品 state 恢复验证**

用 python3 将 c01 state 的最后一条 terminal_paths 条目移回 queue（steps 片段）：

```bash
python3 - <<'EOF'
import json
p = "test_fixtures/project/.ethunter_out/taint-path-walker/tmp/c01_state.json"
s = json.load(open(p))
if s["terminal_paths"]:
    item = s["terminal_paths"].pop()
    s["queue"].append({"path": item["path"], "frontier": {"func_id": item["path"][-1], "tainted_vars": ["data"]}, "status": "pending"})
    s["phase"] = "tracing"
    json.dump(s, open(p, "w"), ensure_ascii=False, indent=2)
    print("STATE_REWOUND")
else:
    print("NO_TERMINAL_PATHS")
EOF
```
随后按 SKILL.md 流程重跑 c01，diff：
```bash
diff /tmp/c01_direct.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo RESUME_MID_OK
```
Expected: `STATE_REWOUND` 与 `RESUME_MID_OK`（从半成品状态恢复后结果不变）。
注意：该基线由真实 state 结构决定，`func_id` 与 `frontier.tainted_vars` 的取值以实际 state 为准——若结构字段不同（如首次实现时用了别的字段名），以 Task 15 实际生成的 state 为准调整脚本，并在 TEST_PLAN.md 记录实际转回方式。

- [ ] **Step 4: 不通过则修订并重跑**

按同一原则修订（第六章恢复流程/状态结构或各流程章节），重跑至通过。

---

### Task 16: 端到端验收 — 错误用例（bad01、bad02）

**Files:**
- 输出物: 无（预期不产生树文件）

- [ ] **Step 1: 运行 bad01**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker
rm -rf test_fixtures/project/.ethunter_out/taint-path-walker
```
按 SKILL.md 流程执行，entry=`test_fixtures/entries/bad01_entry.json`。
Expected: 报错信息指出缺失字段（form）；`test_fixtures/project/.ethunter_out/taint-path-walker/bad01.json` 不存在。

- [ ] **Step 2: 运行 bad02**

按 SKILL.md 流程执行，entry=`test_fixtures/entries/bad02_entry.json`。
Expected: 报错信息包含函数名 not_exist_fn；`bad02.json` 不存在。

- [ ] **Step 3: 不通过则修订并重跑**

不通过（比如生成了错误树文件）→ 修订 SKILL.md 第二章校验规则与第五章写盘规则，重跑至通过。

---

### Task 17: 录制基线 + TEST_PLAN.md 定稿

**Files:**
- Create: `test_fixtures/expected/{TreeID}.json`（17 个：c01..c03、cpp01..cpp09、m01..m05）
- Modify: `test_fixtures/TEST_PLAN.md`

- [ ] **Step 1: 复制验收通过的输出为基线**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker
for id in c01 c02 c03 cpp01 cpp02 cpp03 cpp04 cpp05 cpp06 cpp07 cpp08 cpp09 m01 m02 m03 m04 m05; do
  cp "test_fixtures/project/.ethunter_out/taint-path-walker/$id.json" "test_fixtures/expected/$id.json"
done
ls test_fixtures/expected | wc -l
```
Expected: 17。

- [ ] **Step 2: TEST_PLAN.md 定稿**

在初稿基础上补充：基线说明（expected/ 为 2026-10-14 录制、验收通过版本的产物；LLM 输出非严格字节级确定，主验收依据为用例表的验收点，基线 JSON 作比对参考）；resume 用例的实际操作命令（Task 15 验证过的版本）；错误用例的运行与预期（Task 16）。

- [ ] **Step 3: 保存 + 自检**

Read 核对 TEST_PLAN.md：19 个用例全部有验收点；运行方式可照抄执行。

---

### Task 18: 全量回归 + 一致性终检

**Files:**
- 无新增；验证全部交付物

- [ ] **Step 1: 清空后按 TEST_PLAN 全量回归**

```bash
cd /home/admin/cc/wksp/siakam_security_skills/taint_path_walker
rm -rf test_fixtures/project/.ethunter_out/taint-path-walker
```
对 17 个正常用例逐个执行（按 SKILL.md 流程），核对验收点；对 2 个错误用例核对报错行为。回归中发现的问题按既定原则修订并重跑受影响用例。

- [ ] **Step 2: 基线比对**

对 17 个用例：
```bash
for id in c01 c02 c03 cpp01 cpp02 cpp03 cpp04 cpp05 cpp06 cpp07 cpp08 cpp09 m01 m02 m03 m04 m05; do
  if diff -q "test_fixtures/expected/$id.json" "test_fixtures/project/.ethunter_out/taint-path-walker/$id.json" >/dev/null; then
    echo "$id: MATCH"
  else
    echo "$id: DIFF（核对验收点；若仅为等价差异，更新基线并记录）"
  fi
done
```
Expected: 全部 MATCH；出现 DIFF 时逐条核对验收点，验收成立才更新基线。

- [ ] **Step 3: 交付物一致性终检**

- `taint-path-walker/SKILL.md`：十章齐全、frontmatter 完整、名称 `taint-path-walker`；
- `test_fixtures/`：project/ 代码、entries/ 19 个、expected/ 17 个、TEST_PLAN.md 齐全；
- 设计文档 §4 大纲与 SKILL.md 章节逐一对应；术语表（§13）全部术语在 SKILL.md 中出现且语义一致；
- 抽查 SKILL.md 无绝对路径示例、无未定义概念引用。

- [ ] **Step 4: 向用户汇报**

汇报：交付物清单（SKILL.md、test_fixtures 各目录）、用例通过情况（19/19 或列出例外与原因）、基线录制路径。

---

## 自检记录（计划编写时的覆盖核对）

- 设计文档 §1-§13 均有对应任务：§1→Task 8 第一章；§2→Task 1、8-12；§3→Task 8；§4→Task 8-12；§5→Task 7、8；§6→Task 11；§7→Task 9、10；§8→Task 12、15；§9→Task 12 第九章表、13-16 验证；§10→Task 2-7、13-17；§11→Task 12 第八章；§12→Task 12 第十章；§13→Task 8-12 术语核对。
- 无占位符：全部夹具代码、入口 JSON、验收点、命令为完整内容。
- 类型/命名一致性：TreeID（c01/cpp01/m01…）、函数名、文件名在 Task 2-7 定义后，Task 13-18 引用一致；验收点中的函数名与实际夹具定义逐一对应。
