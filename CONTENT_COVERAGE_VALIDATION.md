# 内容与证据覆盖清单（Checkpoint 123）

状态：2026-09-16 验收通过。本章建立可重生的全量清单，不把文件存在、导入成功或 headless
请求冒充为实际游玩通过。权威结果为
`../local-data/validation/content-coverage-123/result.json`，源哈希为同目录 `source-hashes.json`。

2026-09-19 后续状态：下列清单数字及“待127/128”描述是123验收时的快照，未重写已封存的清单。
127已完成正常大耳怪前哨任务；128已完成当前导入事件的正常清堡、奖励与独立读档，但发现原Merge
历史编号3与导出事件5不同，交Sol先修复。最新证据分别见127/result.json、128/result.json和
`MM7_CASTLE_CLEAR_VALIDATION.md`；不能把123的历史层级计数当作今天全部证据的统计。

## 可证明的分母

- 原始与运行时 `MapStats` 均有 207 个数字登记行：MM6 67、MM7 76、MM8 61、Merge/Breach 3。
- 119 完整包内有 201 张几何和 201 个对应主地图脚本；最终导入报告也记录 201 张地图加载检查。
- 运行包含 202 个主事件程序（全局 1 + 地图 201）和 8 个显式 overlay；静态索引出
  12630 个注册事件，其中 550 个显式 no-op。事件数不是任务数。
- `data_tables/english/quests.txt` 有 1719 个 QBit 行，604 行有非空任务文本，204 行的定位文本含中文。
  `english/` 是加载路径语义，不能根据目录名判定内容语言。
- QBit 行同时包含任务、状态位和内部标记，动态 MMExtension 关系不能全靠静态分析确定；
  因此本章不给出伪造的“任务通过率”。

## 证据分层

`maps.json` 为每张地图记录原始几何、运行包几何/脚本/overlay、出口引用、资源缺口及最高证据层。
当前保守索引为：9 张有已验收实际 Mac 证据，12 张的最高证据仅为121 headless
生产请求，180 张仅有导入/加载证据，6 张仅登记未导入。这 9 张是可追溯的保守子集，
不是“只有 9 张曾经运行”，更不是“9 张内容全通”。

每个世界各抽查一个已验收入口与一个未实测入口：MM6 `oute3.odm` / `6d01.blv`、
MM7 `7out01.odm` / `7d29.blv`、MM8 `out01.odm` / `d07.blv`。未实测项虽有几何和脚本，
仍保持 `import_load`，证明清单不会因“文件存在”自动升级为游玩通过。

## 已定位的缺口与后续入口

- 未导入 6 张：`mm6:zddb05.blv` 原包无几何；`mm6:6d09.blv` 涉及未支持指令 27/28；
  `mm6:znwc.blv` 涉及未支持指令 20；三张 Merge/Breach 地图尚未导入。
- 18 张已导入地图仍有导入报告中的引用缺口：17 个未解析装饰引用、`mm6:hive.blv`
  的 3 个未解析 encounter；当前报告没有缺失纹理、sprite 或 palette。
- MM6 大耳怪前哨：Q1107 由全局事件1316设置，1317检查物品2163，奖励 2000 XP/2000金和奖章95；
  `6d01.blv` 与脚本已导入，密码物品的场景/宝箱产生来源留待127实际核验。
- MM7 哈蒙代尔城堡：`7out02` 事件301设Q587并进入 `7d29`；城堡事件2在 group56 清空后设Q647，
  全局事件856给 5000 XP/奖章3并清Q587。基本旧事件链已导入，但原 MMExtension 地图 Lua
  并未通用应用，128需对正常流程和差异做实际验证。
- MM8 走私者黑函：全局事件29由NPC3给item742并设Q9；事件32在NPC4话题检查黑函，
  消耗item742、清Q9、设Q10/Q284；回NPC3的事件29再给12000 XP。本章只审计，129实际跑通。
- Merge 邀请信item770–772：原 `Quest_CrossContinents.lua` 具有经验门槛后的宝箱生成、拾取标记和
  Dimension Door 首次接触链。当前运行包只有 Verdant 介绍子集，这三段均尚未导入，明确留待130。

## 重生与校验

在项目根目录执行：

```sh
source external-storage/env.sh
export PATH="$PWD/.tools-venv/bin:$PATH"
python3 local-data/validation/content-coverage-123/generate_content_coverage.py
python3 local-data/validation/content-coverage-123/generate_content_coverage.py --check
```

两次都已返回0。本章没有修改产品或 native 测试源，因此没有重跑未受影响的 native/iOS
验证；已封存的导入/加载记录仅作分层索引。总体仍估 **55–60%，低置信度**：
本章降低了覆盖不确定性，但没有新增剧情通关、广泛地图游玩或当前设备验收。
