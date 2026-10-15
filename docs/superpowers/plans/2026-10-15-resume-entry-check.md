# taint-path-walker 任务恢复检查前置 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把"任务恢复检查"前置到 SKILL.md 第二章执行路径上（§2.5 新增第 4 项），命中即转入 §6.2 断点续跑，消除"有中间结果却从头分析"的风险；同步设计文档与 TEST_PLAN 注释。

**Architecture:** 纯提示词文本改动（无代码）：SKILL.md 四处编辑（§2.5 标题+新增项、第二章执行顺序线、§6.2 入口说明、§9.1 陷阱行）→ 设计文档三处同步 → TEST_PLAN 一行注释 → 验收复跑（全新路径 + B2/B3 resume 流程，复用既有 baseline 与脚本）→ 终检推送。规格依据：`docs/superpowers/specs/2026-10-15-resume-entry-check-design.md`（下称"规格"，§N 指节号）。

**Tech Stack:** Markdown 提示词；git（仓库已存在，master 分支，远端 origin=github.com/siakam43/taint_path_walker）；diff/python3 核对。

**环境说明：** 仓库根 `/home/admin/cc/wksp/siakam_security_skills/taint_path_walker`。任务逐个提交（`docs(...)`/`test(...)` 风格，结尾空行 + `Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>`）；最后一个任务统一推送。**开发期运行 skill** = Read 全文 `taint-path-walker/SKILL.md`，以执行者自身为运行时主 agent（不派发 sub-agent）按其流程执行。

---

### Task 1: SKILL.md 四处改动

**Files:**
- Modify: `taint-path-walker/SKILL.md`（4 处：第 26 行、§2.5、§6.2、§9.1）

- [ ] **Step 1: §2.5 标题**

Edit 旧文：`### 2.5 输出目录与预查重`
新文：`### 2.5 输出目录、预查重与任务恢复检查`

- [ ] **Step 2: §2.5 新增第 4 项（追加在第 3 项预查重之后，即 §2.6 标题之前）**

用 Edit，以 `### 2.6 加载编译宏配置（macro.json）` 为锚点——旧文为该标题行，新文为"新增段 + 空行 + 该标题行"（保持 §2.5 与 §2.6 之间的空行分隔）。插入的新段为：

```
4. **任务恢复检查**：按 2.10 双方法检查 `tmp/{TreeID}_state.json` 是否存在——**存在 → 转入第六章 6.2 恢复流程执行断点续跑**（此时输出文件已确认不存在；从状态现场继续，不重复执行第三章的初始标记，不从头分析）；不存在 → 继续 2.6 及后续步骤进行全新分析——全新分析的状态文件初始化按 6.2 分支④，在开始追踪前（第三章标记完成后）执行。
```

- [ ] **Step 3: 第二章执行顺序线（第 26 行）**

Edit 旧文：`输出目录与预查重（2.5）`
新文：`输出目录、预查重与任务恢复检查（2.5）`
（该行其余内容不动。）

- [ ] **Step 4: §6.2 头部新增"入口说明"**

在 §6.2 导语行（`每次执行本 skill，初始化阶段按以下顺序判定（编号即分支，命中即执行对应流程）：`）之后、编号列表 `1. **① 校验输入**…` 之前，插入一段：

```
**入口**：本流程由第二章 2.5 的**任务恢复检查**进入——分支 ①/② 已在第二章先行完成（①＝2.3–2.4 输入校验，②＝2.5.3 预查重跳过）；检查命中"状态文件存在"→ 执行 ③（断点续跑/补做输出）；未命中 → 按 ④ 全新启动（初始化时机由 2.5.4 指明）。
```

- [ ] **Step 5: §9.1 思维陷阱新增 1 行**

在旧行（整行）之后插入新行：
- 旧行（定位用）：`| "状态文件等整条链完成再存" | 每个工作项处理完立即落盘（见 6.1 保存时机） |`
- 在其后插入：

```
| "有中间结果也从头上再跑一遍" | 执行开头必走 2.5 任务恢复检查：状态文件存在即转 6.2 断点续跑，不重复第三章标记；从头分析既浪费又可能覆盖现场（见 2.5、6.2） |
```

- [ ] **Step 6: 验证**

Run: `grep -n "2.5 输出目录、预查重与任务恢复检查\|任务恢复检查\|有中间结果也从头上再跑一遍\|入口**：本流程由第二章" taint-path-walker/SKILL.md`
Expected: 命中 4 处新增内容；且 `grep -c "### 2.5 输出目录与预查重"` = 0、`grep -c "输出目录与预查重（2.5）"` = 0（旧文已替换）。

- [ ] **Step 7: 提交**

```bash
git add taint-path-walker/SKILL.md
git commit -m "docs(taint-path-walker): front-load resume check into 2.5

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: 主设计文档三处同步

**Files:**
- Modify: `docs/superpowers/specs/2026-10-14-taint-path-walker-design.md`

- [ ] **Step 1: §3.3 追加 bullet**

在旧 bullet（`- 预查重：\`{TreeID}.json\` 已存在 → 打印"该入口已有分析结果：{路径}，跳过分析。"并结束。支持上游重复调度。`）之后追加一条：

```
- 任务恢复检查：`tmp/{TreeID}_state.json` 存在 → 转入恢复流程（§8.2）断点续跑，不从头分析。
```

- [ ] **Step 2: §4 大纲第二章行**

Edit 旧文：`输入解析与初始化（参数、entry json 校验、TreeID、输出目录、预查重、macro.json、codegraph 检测、路径规范、分析范围确定）`
新文：`输入解析与初始化（参数、entry json 校验、TreeID、输出目录、预查重、任务恢复检查、macro.json、codegraph 检测、路径规范、分析范围确定）`

- [ ] **Step 3: §8.2 头部入口说明**

在旧文（`### 8.2 恢复流程（入口四分支）` 之后紧跟的空行后、`1. 校验输入（第 5 节规则）。` 之前）插入：

```
入口：由第二章 2.5 的任务恢复检查进入；分支 ①/② 与 2.3–2.4 / 2.5.3 为同一判定。
```

- [ ] **Step 4: 验证 + 提交**

Run: `grep -n "任务恢复检查" docs/superpowers/specs/2026-10-14-taint-path-walker-design.md` → 命中 §3.3、§4、§8.2 三处。
```bash
git add docs/superpowers/specs/2026-10-14-taint-path-walker-design.md
git commit -m "docs(taint-path-walker): sync design with front-loaded resume check

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: TEST_PLAN.md 注释微调

**Files:**
- Modify: `test_fixtures/TEST_PLAN.md`（resume 用例实操段 B2 注释）

- [ ] **Step 1: B2 注释**

Edit 旧文：`# B2 完成态恢复：删输出、留状态，重跑应走恢复分支`
新文：`# B2 完成态恢复：删输出、留状态，重跑应走恢复路径（经 2.5 任务恢复检查转入 6.2 恢复分支）`

- [ ] **Step 2: 验证 + 提交**

Run: `grep -n "经 2.5 任务恢复检查转入 6.2" test_fixtures/TEST_PLAN.md` → 命中 1 处。
```bash
git add test_fixtures/TEST_PLAN.md
git commit -m "test(taint-path-walker): note resume entry path in TEST_PLAN

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: 验收复跑（全新路径 + B2/B3）

**Files:**
- 产物（运行生成）: `test_fixtures/project/.ethunter_out/taint-path-walker/{c01.json, tmp/c01_state.json}`

先 Read 全文 `taint-path-walker/SKILL.md`，以执行者自身为运行时主 agent（**不派发 sub-agent**）按新流程执行。

- [ ] **Step 1: 全新路径——直跑 c01**

```bash
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
rm test_fixtures/project/.ethunter_out/taint-path-walker/tmp/c01_state.json
```
按 skill 流程直跑 c01（entry=`test_fixtures/entries/c01_entry.json`）。验证：
1. `diff test_fixtures/expected/c01.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo FRESH_OK`
2. 状态文件检查：`tree_id=c01`；`taint_marks` 含 `cfg`；`phase=done`；`queue=[]`；`counters` 与实际条目数自洽（用 python3 复核）。

- [ ] **Step 2: B2——完成态恢复（应经 2.5.4 → 6.2③）**

```bash
cp test_fixtures/project/.ethunter_out/taint-path-walker/c01.json /tmp/c01_direct2.json
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
```
重跑 c01（state 存在、输出不存在 → 走恢复）。验证：
```bash
diff /tmp/c01_direct2.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo RESUME_OK
```

- [ ] **Step 3: B3——半成品回卷**

按 `test_fixtures/TEST_PLAN.md`「resume 用例实操」的回卷脚本（python，动态取 `terminal_paths` 末条、按入边 `call_sites.taint_args` 重建前沿、`phase="tracing"`、重算 counters）执行回卷，然后：

```bash
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
```
重跑 c01。验证：
```bash
diff /tmp/c01_direct2.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo RESUME_MID_OK
```

- [ ] **Step 4: 不干扰检查**

```bash
for id in c01 c02 c03 c04 cpp01 cpp02 cpp03 cpp04 cpp05 cpp06 cpp07 cpp08 cpp09 m01 m02 m03 m04 m05; do diff -q "test_fixtures/expected/$id.json" "test_fixtures/project/.ethunter_out/taint-path-walker/$id.json" >/dev/null && echo "$id OK" || echo "$id DIFF"; done
```
Expected: 18 个全部 OK（其余 17 个未被本次复跑触碰；c01 与基线一致）。

- [ ] **Step 5: 不通过则修订重跑**

任一验证失败：定位原因——若为提示词问题（新 §2.5 第 4 项/§6.2 入口说明措辞导致路由或行为偏差），最小修复 `taint-path-walker/SKILL.md` 并提交（`docs(...)` 风格）；若为运行失误（未按流程），按流程重跑对应步骤。**不得放宽验收**。

---

### Task 5: 终检 + 提交刷新 + 推送

- [ ] **Step 1: 一致性终检**

Run 以下检查（全部应通过）：
1. `grep -c "任务恢复检查" taint-path-walker/SKILL.md` ≥ 2（§2.5 标题+第 4 项；§6.2 入口说明）；`grep -c "2.5 任务恢复检查\|2.5.4" taint-path-walker/SKILL.md` ≥ 1。
2. 全文交叉引用抽查：`2.5`/`6.2`/`2.10` 引用处仍有效（编号零变化）；`grep -n "2\.5\.[34]" taint-path-walker/SKILL.md` 仅应命中 §6.2 入口说明（`2.5.3`、`2.5.4` 各 1 次）。
3. `git status --porcelain` 显示的 `.ethunter_out` 改动仅为 c01 相关（若有）；其余 17 个输出与 `expected/` 逐一对 `diff -q` 通过。

- [ ] **Step 2: 提交运行产物刷新（如与 HEAD 有差异）**

若 `test_fixtures/project/.ethunter_out/taint-path-walker/{c01.json,tmp/c01_state.json}` 相对 HEAD 有改动：先深比较确认语义等价（python `json.load` 相等）或与基线一致后：
```bash
git add test_fixtures/project/.ethunter_out
git commit -m "test(taint-path-walker): refresh c01 artifacts after resume-entry rerun

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```
（无差异则跳过本步。）

- [ ] **Step 3: 推送**

```bash
git push
```
Expected: `origin/master` 快进至最新提交。

- [ ] **Step 4: 报告**

汇报：4 处 SKILL.md 改动落位情况；FRESH_OK / RESUME_OK / RESUME_MID_OK 结果；18/18 基线一致；推送状态。

---

## 自检记录（计划编写时的覆盖核对）

- 规格 §3.1–§3.6 全部映射：§3.1/3.2/3.3/3.4 → Task 1；§3.5 → Task 2；§3.6 → Task 3；§4 验收 → Task 4；收尾 → Task 5。
- 无占位符：全部旧文/新文逐字给出；命令含期望输出。
- 命名一致性：§2.5 第 4 项被称为"2.5.4"（与规格 §3.3 措辞一致）；"任务恢复检查"用词全案统一。
