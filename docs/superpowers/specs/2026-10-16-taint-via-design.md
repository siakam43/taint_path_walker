# taint-path-walker 空数组调用点通道描述（taint_via）设计

- **日期**：2026-10-16
- **状态**：已与用户逐点确认（方案 A：字符串字段 `taint_via`，英文短描述模板）
- **涉及文件**：`taint-path-walker/SKILL.md`、`docs/superpowers/specs/2026-10-14-taint-path-walker-design.md`、`test_fixtures/TEST_PLAN.md`、`test_fixtures/expected/{c01,cpp04,cpp08}.json`、`test_fixtures/project/.ethunter_out/`（三用例产物重录）
- **参考**：主设计文档 `2026-10-14-taint-path-walker-design.md`（其 §6 为输出契约、§8 为状态文件权威）；`test_fixtures/TEST_PLAN.md`（验收约定）

---

## 1. 背景与问题

`taint_args` 为空数组是合法值（SKILL.md 4.2：①污点经全局/静态变量进入；②经接收对象成员态进入且被调函数无污点形参；③经 lambda 捕获进入）。语义本身合理——调用点上确实没有脏实参，污点经"非参数通道"抵达被调函数。

但读结果文件时只见 `[]`：**通道的细节（哪个全局变量 / 哪个成员 / 哪个捕获项）被折叠**，无法了解污点具体如何进入被调函数。仓库中实际受影响条目仅 3 处：

- c01：`{"callee": "hal_cfg_check", "line": "39", "taint_args": []}`——情形①，全局 `g_cfg_mode`；
- cpp04：`{"callee": "isp::util::RingBuf::get", "line": "20", "taint_args": []}`——情形②，接收对象成员 `m_v`；
- cpp08：`{"callee": "<lambda@cpp/smart_lambda.cpp:46>", "line": "49", "taint_args": []}`——情形③，捕获 `data, len`。

约束前提：`taint_args` 语义与三情形合法性不变、验收点不推翻、信息以"新增"方式附加；下游 taint-tree-checker 的运行时解析不读 `calls`/`callee`/`taint_args`（其 SKILL.md 输入契约仅 `func`/`file`/`begin_line`），附加字段对管线契约风险为零。

## 2. 目标与范围

1. 为空数组调用点条目新增 `taint_via` 字符串字段（英文短描述），记录污点进入被调函数的通道与载体；
2. 输出文件与状态文件同步记录（状态是唯一现场，输出组装时从状态带入）；
3. `taint_args` 语义、去重/排序规则、既有验收格局不变。

**非目标 / 已知边界**：

- 仅空数组条目携带（⇔ 三情形之一）；"脏形参 + 非形参通道"并存的双通道条目不覆盖（数组已非空，不属于本次痛点）；
- 不记录载体的写入点（如"`g_cfg_mode` 由 `hal_cfg_step1:24` 写入"）——读者凭载体名回源码定位；后续如需扩展为键值结构属纯增量；
- 不引入任何新判定逻辑：通道类型在建边时（4.2 步骤 3）已经判定，`taint_via` 只把判定结果写下来；
- 不改 fixtures、不新增测试用例（三个既有用例已覆盖三情形）。

## 3. 字段定义与格式

`taint_via`：字符串，与 `taint_args` 同级，挂在 calls 条目（输出）与 `call_sites` 条目（状态）上。

**携带条件**：仅当该调用点 `taint_args` 为空数组时携带；非空数组条目不得携带。

**模板** `{关键词} {载体}`，关键词仅三个（全小写，固定拼写）：

| 关键词 | 载体取值 | 例 |
|--------|----------|-----|
| `global` | 全局/静态变量名；类 static 成员写作 `Class::member` | `global g_cfg_mode` |
| `receiver member` | 经 this 所指对象的成员路径 | `receiver member m_v` |
| `lambda capture` | 实际承载污点的捕获项名（不脏的捕获项不列） | `lambda capture data, len` |

**分隔符**：同一调用点多个载体以 `, `（逗号+空格）连接；多种通道并存（罕见）以 `; `（分号+空格）连接，如 `global g_flag; lambda capture ctx`。

三条实例（与仓库三处受影响条目一一对应）：

```json
{"callee": "hal_cfg_check", "line": "39", "taint_args": [], "taint_via": "global g_cfg_mode"}
{"callee": "isp::util::RingBuf::get", "line": "20", "taint_args": [], "taint_via": "receiver member m_v"}
{"callee": "<lambda@cpp/smart_lambda.cpp:46>", "line": "49", "taint_args": [], "taint_via": "lambda capture data, len"}
```

## 4. 数据流与状态同步

1. **建边时写入**：4.2 步骤 3 建调用边时通道类型已判定（三情形判定沿用 3.4/3.5 与 4.2 既有规则），`taint_via` 与 `taint_args` 同时写入状态文件 `tree.edges[].call_sites[]`。
2. **组装时带入**：5.3 步骤 4 组装输出时从状态读出该字段带入 calls 条目，不做二次判定。
3. **恢复兼容**：6.1 逐工作项落盘、6.2 恢复流程随 `call_sites` 自然持久化；TEST_PLAN 的 B3 回卷脚本读 `taint_args` 重建前沿，不受附加字段影响。
4. **去重/排序不变**：`taint_via` 不参与排序键；"完全相同的条目去重"的相等性含该字段——同一点位、同一通道的取值必然一致，不产生歧义；被吞并链的调用点合并时该字段随条目一并合并。

## 5. 改动清单（含目标措辞）

### 5.1 SKILL.md（5 处）

1. **§4.2 三情形段（现第 305 行）——权威定义处**。在原"`taint_args` 为空数组是合法值，仅限三种情形：①…②…③…"之后补：

   > 空数组情形须同步记录通道描述 `taint_via`（字符串，与 `taint_args` 同级；非空数组条目不得携带）。模板 `{关键词} {载体}`，关键词仅三个：`global <变量名>`（全局/静态变量；类 static 成员写 `Class::member`）、`receiver member <成员路径>`（污点经 this 所指对象的成员态进入，如 `m_v`、`s.inner`）、`lambda capture <捕获项名>`（实际承载污点的捕获项，不脏的捕获项不列）。同一调用点多个载体以 `, ` 连接；多种通道并存以 `; ` 连接（如 `global g_flag; lambda capture ctx`）。

2. **§5.2 规则 4（现第 406 行）**。`taint_args` 子项之后新增一行：

   > - `taint_via`：仅当 `taint_args` 为空数组时携带的通道描述（格式与关键词见 4.2）；先记入状态文件 `call_sites`，按 5.3 随条目带入输出。

3. **§6.1 字段表 `tree.edges` 行（现第 464 行）**。`call_sites` 说明补为：`line` 行号、`taint_args` 污点形参名数组、`taint_via`（空数组调用点的通道描述，见 4.2）。

4. **§7 检查清单第 5 条（现第 513 行）**。括号内扩为：`taint_args` 与空数组条目的 `taint_via` 通道描述准确；对应 4.5 自检项 2。

5. **§9.1 陷阱行（现第 554 行）**。应对措施中"`taint_args` 记空数组（4.2 情形②）"改为"`taint_args` 记空数组并同步记录 `taint_via` 通道描述（4.2 情形②）"；该行其余文字不动。

### 5.2 主设计文档（2026-10-14）同步 2 处

1. §6 输出契约 `taint_args` 三情形行（现第 164 行）：句末补"空数组条目须同步记录通道描述 `taint_via`（`global <名>` / `receiver member <成员路径>` / `lambda capture <捕获项>`；格式与分隔符规则见 SKILL 4.2）。"
2. §8 状态文件 `call_sites` 示例/说明处（现第 256 行）：在示例的 `call_sites` 数组中**追加**一条空数组条目 `{"line": "39", "taint_args": [], "taint_via": "global g_cfg_mode"}`（原非空条目保留），并注明"`taint_via` 仅空数组调用点携带"。

### 5.3 TEST_PLAN.md 与基线

1. **三行验收点追加断言**：
   - c01：`[E,check]` 的 `calls` 为 check、`taint_args` 为空数组**、`taint_via` 为 `"global g_cfg_mode"`**；
   - cpp04：`RingBuf::get` 条目 `[]`**、`taint_via` 为 `"receiver member m_v"`**；
   - cpp08：lambda 条目 `calls.taint_args` 为空数组**、`taint_via` 为 `"lambda capture data, len"`**。
2. **「基线说明」段**补一句：2026-10-16 起 c01/cpp04/cpp08 三个基线含 `taint_via` 字段；其余 15 个基线不含该字段（空数组条目之外的条目不得携带）。
3. **三基线刷新**：`expected/{c01,cpp04,cpp08}.json` 中对应条目追加 `taint_via`（字段序在 `taint_args` 之后，如 §3 实例）。
4. **产物重录**：`test_fixtures/project/.ethunter_out/taint-path-walker/` 下 c01/cpp04/cpp08 的输出与 state 文件随重跑刷新。

## 6. 验收方式

1. **重录三基线**：删除 c01/cpp04/cpp08 的输出与状态文件 → 按 skill 流程重跑三用例 → 输出与刷新后的 expected 一致（三处 `taint_via` 取值准确、`taint_args` 仍为 `[]`）→ 状态文件 `call_sites` 同步含 `taint_via`。
2. **回归不溢出（强断言）**：全量 18 用例重跑，除 c01/cpp04/cpp08 外其余 15 个输出与既有基线逐字节一致（比对有差异时按 TEST_PLAN「基线说明」逐项判定；硬性标准 = 链集合不变、且无 taint_via 泄漏到空数组之外的条目）。
3. **resume 路径复跑**：c01 按 TEST_PLAN「resume 用例实操」执行 B1/B2/B3；B2/B3 输出与直跑一致（RESUME_OK / RESUME_MID_OK），回卷脚本无需修改。
4. **交叉引用复核**：SKILL.md 新增引用无悬空；9.1 陷阱行与 4.2 定义措辞一致；全文 `taint_via` 拼写统一。

## 7. 兼容性与非目标

- 输出 schema 纯增量（新字段仅出现在空数组条目）；下游 taint-tree-checker 不解析 `calls`/`taint_args`（其输入契约仅 `func`/`file`/`begin_line`），无兼容风险；
- `taint_args` 语义、三情形合法性、去重/排序规则、验收点格局（除 3 行细化）全部不变；
- 状态文件 schema 纯增量；恢复流程四分支与 B3 回卷脚本不改；
- 非目标：不覆盖双通道（非空数组）条目；不记录写入点；不新增 fixture / 测试用例；不改变任何追踪判定逻辑。
