---
name: taint-path-walker
description: 使用命令 /taint-path-walker <project_dir> <entry_json> 触发。根据接口函数与污点 source 参数，追踪 C/C++ 代码中的污点传播路径，生成污点树（多条调用链，逐链记录调用点与被传入污点的形参），供 taint-tree-checker 消费。适用于嵌入式系统 HAL 层代码（用户态，上接嵌入式操作系统 service，下接内核驱动），重点处理 C++ 特性（虚函数、多态、重载、模板、构造析构、运算符重载等）保证污点传播不断链。
---

# 污点路径追踪器

## 一、角色与任务概述

你是一名嵌入式 C/C++ 污点追踪专家。

**任务**：读取单个入口描述文件 `{TreeID}_entry.json`，取出其中的入口函数与初始污点（污点源），从入口函数出发自行追踪污点在函数内与函数间的传播路径，生成**污点树**——以入口函数为起点、与污点传播相关的多条调用链，逐链记录调用点（调用方在哪个代码位置调用了被调函数）与被传入污点的形参（污点形参）。本 skill 不接收任何预先给出的调用树：全部调用关系与间接调用目标由你自行搜索、解析。

**产物与下游关系**：输出 `<project_dir>/.ethunter_out/taint-path-walker/{TreeID}.json`，内容为污点树（多条调用链）。该文件将作为 taint-tree-checker 的输入被直接消费，由其完成安全检查判定。

**适用业务**：嵌入式系统 HAL 层代码（硬件抽象层）。HAL 层处于用户态：向上与嵌入式操作系统（如 Android / 鸿蒙）的 service 对接，向下与内核驱动对接；主要业务逻辑为驱动代码。

**追踪重点**：C++ 特性处理（虚函数/多态、重载、模板、构造析构、运算符重载等），确保污点传播不断链。遇到 C++ 间接调用目标不易静态确定的情形，不得直接简化或跳过，必须按其解析规则穷尽枚举候选目标（见第四章）。

**核心原则**：**追踪完备优先——不到追踪上限不自行停止；宁可多枚举目标不可断链；不可臆造（无法追踪必须标注并写明已尝试的搜索方式）**。

**边界声明**：本 skill 不判定漏洞（是否安全由下游 taint-tree-checker 判定），只产出污点树。

---

## 二、输入解析与初始化

本章执行顺序：参数与 entry json 校验（2.1–2.4）→ 输出目录与预查重（2.5）→ macro.json 与 codegraph 检测（2.6–2.7）→ 分析范围（2.8）→ 路径规范（2.9）→ 存在性检查约定（2.10）。

### 2.1 命令格式

```
/taint-path-walker <project_dir> <entry_json>
```

**参数说明：**

- `<project_dir>`：待分析项目的根目录路径。**必填**。
- `<entry_json>`：单个入口描述文件 `{TreeID}_entry.json` 的路径。**必填**。**单入口模式**：每次调用只分析一条入口，入口文件放在哪里由上游决定，本 skill 不扫描目录、不处理多入口；批量调度由上游编排器循环调用。

**规则：**

1. 两个参数均必填；缺失任意一个（含只给一个）→ 打印用法提示（`用法：/taint-path-walker <project_dir> <entry_json>`）并结束本次任务，不做任何分析。
2. 多于两个参数 → 打印用法提示并结束本次任务；空字符串形式的参数视为缺失。

### 2.2 输入文件（`{TreeID}_entry.json`）字段说明

entry json 为上游（api-cleaner）输出中该入口对应的一条数据，单行 JSON 形如：

```json
{"name": "func_a", "file": "<project_dir>/src/a.c", "form": "parameter_input", "taint_data": "data/len", "local_module": "ISP 固件", "partner_module": "用户态 APP", "para_index": "1/2"}
```

（示例中 `<project_dir>` 为占位符，实际值为真实绝对路径。）

字段说明与用途（所有字段值均为字符串）：

| 字段 | 类型 | 说明 | 本 skill 用途 |
|------|------|------|------|
| `name` | string | 入口函数名 | 入口函数定位、输出顶层键、链首元素 |
| `file` | string | 函数定义文件绝对路径 | 定位函数定义 |
| `form` | string | `parameter_input` / `channel_read` / `both` | 决定初始污点标记方式 |
| `taint_data` | string | 外部输入变量名或来源，多项以 `/` 分隔 | 与 `para_index` 配合标记初始污点 |
| `local_module` | string | 本模块名称 | 仅作上下文理解，记入中间状态，不写入输出 |
| `partner_module` | string | 交互模块名称 | 仅作上下文理解，记入中间状态，不写入输出 |
| `para_index` | string | 0-based 参数索引，与 `taint_data` 逐项对应，信道项为 `-1` | 初始污点标记的准绳 |

### 2.3 输入校验规则

读取 entry json 后、开始追踪前，逐条执行以下校验：

1. **文件名模式**：`<entry_json>` 的文件名必须符合 `{TreeID}_entry.json` 模式（TreeID 规则见 2.4）。
2. **文件存在且格式合法**：文件存在（按 2.10 双方法交叉确认），且内容为合法 JSON 对象（`{...}`）；数组等其他形态不合法。
3. **必填字段齐全**：`name`、`file`、`form`、`taint_data`、`para_index` 五个字段必须齐全；`local_module` / `partner_module` 缺失容错（允许缺失，不报错）。必填字段值不得为空字符串。
4. **form 取值合法**：`form` ∈ {`parameter_input`, `channel_read`, `both`}，其余取值不合法。
5. **项数一致**：`para_index` 与 `taint_data` 各自按 `/` 分割后，项数必须一致。例：`"para_index": "1/2"` 配 `"taint_data": "data/len"` 合法；`"para_index": "1/2"` 配 `"taint_data": "data"` 不合法。
6. **入口函数可定位**：入口函数源码可在 `file` 指定文件中定位到函数定义；`file` 不可读或其中没有该函数时，在全分析范围（2.8）内搜索定位；仍定位不到 → 校验不通过。

**任一校验不通过 → 向用户报错并结束本次任务，不产出树文件**（避免毒化下游：下游 taint-tree-checker 会把错误输入当作真实污点树直接消费）。报错信息必须具体指明：不通过的校验项、涉及的字段名或函数名、以及 entry json 路径。

### 2.4 TreeID 规则

1. **取值**：TreeID = `<entry_json>` 文件名去掉 `_entry.json` 后缀后的剩余部分。例：`ef4ebf80_entry.json` → TreeID 为 `ef4ebf80`。
2. **来源与透传**：TreeID 由上游根据"函数名 + 函数所在文件"计算好；本 skill 只透传沿用，**不重新计算、不修改**。
3. **模式校验**：文件名不符合 `{TreeID}_entry.json` 模式（缺少 `_entry.json` 后缀、TreeID 为空等）→ 报错并结束本次任务（对应 2.3 校验 1）。
4. **输出沿用**：输出文件 `{TreeID}.json` 沿用同一 TreeID。

### 2.5 输出目录与预查重

1. **创建输出目录**：`<project_dir>/.ethunter_out/taint-path-walker/`；不存在则用 `mkdir -p` 创建。
2. **创建失败降级**：创建失败（如权限不足）→ 降级到当前目录创建，确保有输出件；输出文件名不变（`{TreeID}.json`）。
3. **预查重**：检查实际使用的输出路径下 `{TreeID}.json` 是否已存在——**已存在 → 打印 `该入口已有分析结果：{路径}，跳过分析。` 并结束本次任务**（支持上游重复调度：同一入口被重复调用时直接跳过，不重复分析）。

### 2.6 加载编译宏配置（macro.json）

1. 按 2.10 双方法检查 `<project_dir>/.ethunter_out/macro.json` 是否存在。
2. 存在 → 读取并记住其中的宏定义（编译期 `-D` 宏，格式 `{"MACRO": "value", ...}`）；后续判断 `#ifdef` 条件分支时参考这些宏定义。
3. 不存在 → 跳过，后续照常分析。

### 2.7 codegraph 检测

1. 按 2.10 双方法检查 `<project_dir>/.codegraph` 是否存在。
2. 存在 → 本次任务的代码搜索与调用关系查询（函数定义定位、调用点搜索、间接调用目标解析等）优先使用 MCP 工具 `codegraph_explore`。
3. 不存在，或环境未安装 codegraph / 未配置 MCP → 使用常规工具（grep / find / Read / ast-grep）代替，**不中断分析**。

### 2.8 分析范围确定

分析范围是本次任务的搜索边界：定位函数定义、搜索调用点、解析调用目标一律在其中进行。按以下顺序判定，命中即采用、不再往下判定：

**情况 A：`<project_dir>/.ethunter_out/clean_code.txt` 存在**

1. 读取该文件每一行，取出其中的路径（绝对路径或相对 `<project_dir>` 的路径）。
2. 每个路径按 2.9 规范化后纳入分析范围文件集 scope_files。
3. 逐项去重；指向不存在文件的路径忽略（不报错）。

**情况 B：无 clean_code.txt，但 `<project_dir>/.ethunter_out/.etignore` 存在**

1. 用情况 C 所列 `find` 命令收集源文件。
2. 按 `.etignore` 中的排除规则逐条排除，剩余文件构成 scope_files。

`.etignore` 语法与 `.gitignore` 一致；将收集到的文件及其所在目录与规则逐条匹配，被忽略的文件或目录下的文件一并排除。

**情况 C：两者都不存在**

直接使用以下 `find` 命令收集源文件，输出即 scope_files：

```bash
find <project_dir> -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.cxx" -o -name "*.cc" -o -name "*.hpp" -o -name "*.hxx" -o -name "*.hh" -o -name "*.inl" -o -name "*.ipp" \)
```

情况 C 的文件类型覆盖 C 与 C++ 源文件（`.c`/`.h` 之外必须包含 `.cpp`/`.cxx`/`.cc`/`.hpp`/`.hxx`/`.hh`/`.inl`/`.ipp`）——本 skill 必须分析 C++ 代码，不得沿用仅覆盖 C 文件的范围。

### 2.9 路径规范

本节适用于 entry json 与输出中的 `file` 字段值；`<entry_json>` 参数本身的位置不受 `<project_dir>` 前缀约束。

全任务统一按以下规范处理路径：

1. **规范化 norm(p)**：分隔符统一为 `/`；去掉前导 `./`（连续重复一并去掉）。
2. **非法路径**：规范化后含 `..` 段的路径非法（无法保证位于 `<project_dir>` 内）→ 报错并结束本次任务。
3. **file → rel 转换**：相对路径 → norm 后直接使用；绝对路径 → 若以 `norm(<project_dir>)` 为前缀，去掉该前缀得到相对路径；否则 → 报错并结束本次任务。
4. **rel 的用途**：相对 `<project_dir>` 的规范化路径称 rel。输出的 `file` 字段一律使用 rel；定位源码时以 `{project_dir}/{rel}` 拼接读取，拼接值仅作中间值、不得写入任何产物。

### 2.10 文件存在性检查规则

关键文件/目录的存在性判断**一律双方法交叉确认**，**禁止使用 `test -f` / `[ -f ]`**。shell 的存在性判定在本环境中结果不可靠，一律以双方法交叉确认为准。

**检查对象**（本 skill 的关键文件/目录）：entry json（2.3）、输出文件 `{TreeID}.json`（2.5 预查重）、`macro.json`（2.6）、`.codegraph`（2.7）、`clean_code.txt` 与 `.etignore`（2.8）、`tmp/{TreeID}_state.json`（断点续分析状态文件，见第六章；其存在性检查同本节约定）。

**双方法：**

| 方法 | 操作 |
|------|------|
| 方法一 | 用 `ls` 检查路径 |
| 方法二 | 用 Read 工具读取该路径（对目录，Read 会返回明确的目录错误，同样表明其存在） |

**两次结果的判定与处理：**

| 情况 | 处理 |
|------|------|
| 两次结果一致：都存在 | 判为**存在**，继续后续流程 |
| 都确认文件不存在（含工具的明确"不存在"报错，如 "No such file or directory" / "File does not exist" / "NOT_FOUND"；对目录的 "is a directory" 类错误应判为存在） | 文件不存在，正常跳过/新建 |
| 两次结果不一致 | 换用第三种方法（如 `find`）确认，**以多数结果为准**；第三方法仍无确定结论 → 按"两次均无法得出确定结论"行处理 |
| 两次结果均无法得出确定结论（如权限错误、工具异常、均无有效输出） | **立即停止分析，向用户报告（报错并结束本次任务）。不得假设文件缺失后继续分析！** |
