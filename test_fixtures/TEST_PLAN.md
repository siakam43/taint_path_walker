# taint-path-walker 系统测试计划

## 测试项目结构

test_fixtures/project/
├── c/       c_base.{h,c}  c_channel.c  c_filter.c   # C 基础用例
├── cpp/     vtable.{h,cpp} overload.cpp templates.cpp ctor_conv.cpp smart_lambda.cpp mi.cpp
├── mech/    dedup.c limits.cpp cycle.c external.c   # 机制类用例
test_fixtures/entries/    # 各用例入口文件 {TreeID}_entry.json
test_fixtures/expected/   # 验收通过后录制的基线 {TreeID}.json

## 开发期运行方式

**运行方式（当前 skill 版本）**：Read 全文 `taint-path-walker/SKILL.md`，严格按其流程以
`project_dir = test_fixtures/project`、`entry_json = test_fixtures/entries/<用例>_entry.json` 执行一次完整分析。

**批量回归注意**：各用例输出文件名互不冲突（`{TreeID}.json`），同一轮回归内**不要**在用例间删除输出目录
——只在"重跑同一用例"前删除该用例的 `{TreeID}.json`（否则触发预查重跳过，见 SKILL.md 2.5 / 6.2 分支②）。
全量重跑前可整体删除 `.ethunter_out/taint-path-walker/`。

**验收核对**：核对链元素 `begin_line`/`calls.line` 均回到源码行号；状态文件 `phase=done`、`counters` 与实际条目数自洽。

## 基线说明

`test_fixtures/expected/` 为 2026-10-15 录制、18 个基线（c01–c04、cpp01–cpp09、m01–m05）全部验收通过版本的产物；用例表共 22 行（18 个正常用例 + resume 用例 + bad01–bad03 错误用例），resume 与错误用例不产出基线。追踪由 LLM 执行、
输出非严格字节级确定，因此**主验收依据为用例表的验收点**，基线 JSON 作比对参考（比对有差异时
逐项判定是否为 LLM 表述性差异；链集合与解析结论才是硬性标准）。

## 用例与验收点（主验收依据：必须成立的链集合与解析结论）

| 用例 | 入口 | 验收点 |
|------|------|--------|
| c01 | hal_apply_cfg | 链集合恰好 = { [hal_apply_cfg, hal_cfg_step1, hal_cfg_step2], [hal_apply_cfg, hal_cfg_step3], [hal_apply_cfg, hal_cfg_check] }；[E,hal_cfg_step2] 直调链被去重但其调用点并入链1（链1 的 E.calls 含 step1 与 step2 两条；[E,step3] 的 E.calls 为 step3；[E,check] 的 E.calls 为 check 且 taint_args 为空数组）；step2 内的校验不阻挡传播 |
| c02 | hal_poll_shared | 恰好 1 条链 [hal_poll_shared, hal_process_cmd]；不得出现 shm_read_block 节点；calls.taint_args = [p, n] |
| c03 | hal_dispatch | ctx（框架指针）未产生任何追踪；恰好 1 条链 [hal_dispatch, hal_process_cmd] |
| c04 | hal_cfg_apply2 | flags（常量类参数：仅与固定值位测试、未流入其他位置）被二次过滤不追踪；恰 1 条链 [hal_cfg_apply2, c04_sink]，taint_args [p, n] |
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
| m03 | entry_fanout | 20 条链 [entry_fanout, op_00..op_19]；state 截断记录含 op_20（验收以最终输出为准；中间终态可能含单元素 [entry_fanout]，被去重移除，非缺陷） |
| m04 | entry_cycle | 链 [entry_cycle, cyc_a, cyc_b]；state 环终止记录；不再延伸（cyc_a 锚定定义行 cycle.c:8，不得锚到第 1 行原型声明） |
| m05 | entry_ext | 单元素链 [entry_ext]；state untraceable 含 vendor_crypto_sign（reason=源码缺失，tried 非空） |
| resume | c01 | 直跑 / 完成态恢复 / 半成品回卷三条路径输出一致；照抄流程见下节「resume 用例实操」 |
| bad01 | 缺 form | 报错并结束；不生成 {TreeID}.json；错误信息包含缺失字段 |
| bad02 | 入口不存在 | 报错并结束；不生成 {TreeID}.json；错误信息包含函数名 |
| bad03 | para_index 越界 | 报错并结束；不生成 bad03.json；错误信息指出 para_index 越界（校验项 7，2 个形参对应合法索引 0/1） |

## resume 用例实操（照抄流程）

```bash
# B1 直跑参照
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
rm test_fixtures/project/.ethunter_out/taint-path-walker/tmp/c01_state.json
# → 按 skill 流程完整重跑 c01，然后：
cp test_fixtures/project/.ethunter_out/taint-path-walker/c01.json /tmp/c01_direct.json

# B2 完成态恢复：删输出、留状态，重跑应走恢复路径（经 2.5 任务恢复检查转入 6.2 恢复分支）
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
# → 重跑 c01
diff /tmp/c01_direct.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo RESUME_OK

# B3 半成品状态回卷：将最后一条 terminal_paths 移回 queue（按 §6.2 前沿重建填真实 tainted_vars）、
# phase="tracing"、删输出后重跑
```

回卷脚本（已实测通过）：

```python
import json
p = "test_fixtures/project/.ethunter_out/taint-path-walker/tmp/c01_state.json"
st = json.load(open(p))
last = st["terminal_paths"].pop()
fid = last["path"][-1]
e = [x for x in st["tree"]["edges"] if x["from"] == last["path"][-2] and x["to"] == fid][0]
tainted = e["call_sites"][0]["taint_args"]
st["queue"].append({"path": last["path"], "frontier": {"func_id": fid, "tainted_vars": tainted}, "status": "pending"})
st["phase"] = "tracing"
st["counters"]["queue_remaining"] = len([q for q in st["queue"] if q["status"] in ("pending", "in_progress")])
st["counters"]["terminal_paths"] = len(st["terminal_paths"])
json.dump(st, open(p, "w"), ensure_ascii=False, indent=4)
```

注释说明：`tainted_vars` 来自该工作项入边的 `call_sites.taint_args`（§6.2 前沿重建）。`terminal_paths` 为发现顺序、不排序，因此"最后一条"随运行实例而异，脚本动态取之即可；实测样例：`[1,5]`（frontier func_id=5、tainted_vars=[]）或 `[1,2,3]`（func_id=3、tainted_vars=["len"]）。关键验收 = 回卷重跑后输出与直跑一致（RESUME_MID_OK），不绑定具体回卷取值。然后：

```bash
# B3 续：删输出 → 重跑 → 核验回卷恢复输出
rm test_fixtures/project/.ethunter_out/taint-path-walker/c01.json
# → 重跑 c01
diff /tmp/c01_direct.json test_fixtures/project/.ethunter_out/taint-path-walker/c01.json && echo RESUME_MID_OK
```

## 错误用例运行与预期

错误用例与正常用例同方式运行（entry_json = `test_fixtures/entries/bad0X_entry.json`），预期在初始化校验阶段报错结束：

- **bad01**（entry json 缺 `form` 字段）→ 校验项 3 报错，错误信息包含缺失字段名；不产出 `bad01.json` 与状态文件。
- **bad02**（入口函数 `not_exist_fn` 不存在）→ 校验项 6 报错，错误信息包含函数名；不产出 `bad02.json`。
- **bad03**（`para_index "2"` 越界——2 个形参、合法索引 0/1）→ 校验项 7 报错，错误信息包含合法索引范围；不产出 `bad03.json`。

## 运行前注意

- 重跑同一用例前必须删除该用例的 `{TreeID}.json`（否则触发预查重跳过）；全量重跑前可整体删除
  `test_fixtures/project/.ethunter_out/taint-path-walker/`。同一轮回归内不要在用例间删除输出目录（见上文批量回归注意）。
- 若仓库目录被移动，刷新 entries 中的绝对路径（`$NEW` 为空时短路，防止 sed 破坏 entries）：
  OLD=/home/admin/cc/wksp/siakam_security_skills/taint_path_walker/test_fixtures/project
  NEW=$(cd test_fixtures/project && pwd) && [ -n "$NEW" ] && grep -rl "$OLD" test_fixtures/entries | xargs sed -i "s|$OLD|$NEW|g"
