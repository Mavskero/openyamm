# RC2 接手材料索引

当前入口：[iPadOS 交接](../../IPADOS_HANDOFF.md) · [新电脑 Codex 提示词](../../CODEX_IPAD_DEVICE_PROMPT.md)。

本目录保存 2026-10-28 交接时的项目记录。游戏逻辑已在 RC2 对应源码中，接手者不需要
再次应用补丁或解压旧源码覆盖当前 Git 工作区。首次真机调试从短正常流程开始。

| 路径 | 用途 |
| --- | --- |
| `release/VERSION.json` | RC2 版本、Save103、资源与可执行文件校验值 |
| `release/KNOWN_LIMITS.md` | 各修复实际验证范围及未验收项 |
| `release/RC2_SOURCE_MANIFEST.json` | RC2 当时 1909 个已存在源码文件的内容清单；本次 Git 另增加接手文档并更新入口说明 |
| `RESOURCE_TRANSFER.json`、`runtime-SHA256SUMS` | 私下传输的六个资源文件路径、大小、校验值 |
| `evidence/rc2/requirements-182.json` | 完整目标各项要求与证据边界 |
| `evidence/accepted/` | 工作状态所列接受模块的原始 JSON 副本 |
| `evidence/rc2/` | RC2 随包记录，包括构建、封装、源码恢复及181统计更正 |
| `evidence/followup/` | 交付后源码核对、未获真机授权时的阻断记录 |
| `historical-workspace/` | 原工作区计划、进度、设计、旧测试操作、指令和状态快照 |
| `EVIDENCE_INDEX.json` | 上述复制文件的原路径、仓库内路径、字节数与 SHA256 |

历史记录按原样复制，保持旧结论的时间点和限制。其绝对路径、旧 next、旧暂停/blocked 状态、
旧设备禁令及旧百分比不能直接当成新会话任务。读当前交接入口及用户最新指令。
`source-workspace-instructions.snapshot.txt` 是存档材料，不是本目录的 AGENTS.md。

证据 JSON 可能引用原工作区保留的日志、截图、存档和运行目录；这些大文件没有公开上传，
也没有为本次 Git 交接重新生成。源码核对和已有接受记录不等于第二台 Mac 或目标 iPad 已实测。
原游戏资源须单独传输，不把这个目录中的校验清单误认为资源本体。

当前代码与 RC2 之间的接手改动仅为说明和历史记录；没有再次修改生产逻辑，
也没有新增游戏回测或本机设备操作。后续真机调试结果应另建记录，不覆盖这里的历史文件。
