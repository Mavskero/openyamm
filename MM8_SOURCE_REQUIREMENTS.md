# MMMerge 原始资源核查

## 当前状态：075，已收到并提取 MM8 原版

用户提供的 GOG 英文版 1.0（22833）已成功提取到
`../external-storage/original-game/MM8_GOG_1.0_22833/`，175 个文件全部与安装包校验相符。
三份基础 LOD、八张宝箱背景和两份动画已取得；`EnglishD.lod` 包含 3,250 个 WAV 条目。
**更正：这份 GOG 原版本来就不含独立 `Sounds/` 目录，音效应从 `EnglishD.lod` 读取。**
尚未组合现有 Merge／汉化或重建 063 运行包。详见 `MM8_BASE_EXTRACTION_VALIDATION.md`。

## 以下为收到原版前的历史核查

2026-09-11，核对用户提供的 `MM678_Merged_05.11.2023_gdrv`。证据在
`../local-data/validation/mm8-source-audit-20260911/result.json`。

## 结论

当前目录包含 MM6、MM7 的主要地图、贴图、图标、精灵和对应动画资源，可以继续现有移植工作。
完整 MM8 内容仍需要原版 MM8 基础资源。暂时不要求另行提供 MM6、MM7 安装目录；这是对当前
导入需要的判断，不是全部三代素材和玩法均已验收的承诺。

用户给出的 [Celestial Heavens 主题](https://www.celestialheavens.com/forum/topic/16657)
直接访问返回 HTTP 403，未直接读到首帖。
另行核对了 MMMerge Revamp 维护者仓库的
[安装说明](https://gitlab.com/cthscr/mmmerge/-/wikis/Manual/Install.md)：
先安装 GOG 英文版 MM8，再将 MMMerge 包覆盖到 MM8 目录；文档明确关联 2023-11-05 Base Merge 包。
只采用这部分基础安装依赖证据，本项目没有切换到 Revamp 分支，也没有更新用户现有补丁。

## 实际目录

| 文件或目录 | 核查结果 |
| --- | --- |
| `Data/bitmaps.lod` | 缺失 |
| `Data/sprites.lod` | 缺失 |
| `Data/icons.lod` | 缺失 |
| `Data/mm8.bitmaps.lod` | 14,064 字节，仅 1 个 LOD 条目 `voltylX` |
| `Data/mm8.icons.lod` | 3,491,865 字节，650 个 LOD 条目 |
| `Data/mm8.games.lod` | 12,600,415 字节，122 个 LOD 条目；条目数不等于地图数 |
| `Sounds/` | 缺失 |
| `Anims/mightdod.vid`、`Anims/magicdod.vid` | 缺失；另有带 mm6/mm7/mmmerge 前缀的动画包 |

以上是已确认的缺项，不是完整的 MM8 文件需求清单。`mm8.*.lod` 包是独立资源层，不能仅靠改名
替代缺失的原版包。共享场景导入器本来就分别查找 `bitmaps.lod` 与各带前缀的覆盖包。

## 补充后处理

接收一份单独存放在 NVMeSSD 的干净 MM8 完整安装目录，优先使用安装说明要求的 GOG 英文版。
先核对档案结构与条目，再组合原版基础层、现有 Merge 和简体中文覆盖层，保留用户原始目录。
完整目录也便于查缺原始音效和动画；不将补齐三个 LOD 文件直接视为完整资源通过。

随后重新生成资源并验证 MM8 场景、共享素材和中文覆盖的实际效果。补齐文件不自动补齐尚未移植的
MMExtension 行为、任务和跨大陆内容。现有 MM6/MM7 开发成果与存档无需因此推倒重做。

## 072 实际宝箱画面补充

神庙正常开箱、拾取和存读档流程已得到逻辑验证，但实际宝箱区域显示为空黑。现有源包缺少
`chest01`–`chest08` 八张无前缀 MM8 宝箱背景，服务导入报告已记录此缺项。待完整 MM8 基础层
补入后重新生成资源，并检查背景、物品显示与鼠标／触屏拾取；当前不计为可视宝箱验收通过。
证据和限制见 `TEMPLE_CHEST_VALIDATION.md`。

## 073 导入提示补充

基础档案与宝箱背景的已知缺项现在随转换报告、导入记录及转换目录的再次导入保留，启动页持续显示。
旧目录没有可用记录时提示尚未核查。此记录只有有限文件存在性检查，不能据此认定全部图像、音效或
玩法完整；真实完整 MM8 原版仍待提供。详见 `IMPORT_RESOURCE_STATUS_VALIDATION.md`。
