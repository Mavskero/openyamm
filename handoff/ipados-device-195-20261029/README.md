# 195 接收端真机结果（2026-10-29）

本记录针对 `ipados-ui-function-195-20261029` 的受影响检查。本轮可达短流程已完成，
普通旅行、中文 Save104 新槽保存、前后台恢复和独立进程重启读档均通过。
触摸入口、空槽名称和保存分页的真实阻断已修复并复验；下表未验项不计为通过。
完整自然通关、全部 UI、所有任务分支和 Windows/MMMerge 存档兼容性均未证明。

## 版本、设备和数据边界

| 项目 | 实际记录 |
| --- | --- |
| 发布源码 | `VERSION.json` 的 `18cd05fddcbbd0ca35b27c9cbecbeb7ca02b1bf1`，源码包 2250 个清单文件已核对 |
| 工作分支 | `codex/ipad-device-debug`，保留原有修改，从旧接收端提交快进到上述固定基线 |
| 修复提交 | `7aba6c0cc4bdf36e63e75c9f75da6733d9877c0c`，父提交为上述发布源码 |
| 发布 App 附件 | `OpenYAMM-195-iPadOS-unsigned.zip`，12,907,142 字节；SHA256 `54f1cdde25ddcbf47c1fb9fdec7aaa56a690a7043bf4d2bb85ab5a96bbe3cb3c` |
| 首次安装方式 | 接收 Mac 对发布附件重签，保留数据更新；之后对实际阻断进行源码修复和设备 Release 构建 |
| 最终修复 App | 20,783,264 字节；可执行文件 SHA256 `1965057da4b003c2738c80dd1a66e88ee460f43ced77aae4c4c7c7e81bd783db` |
| 修复源码快照 | `pagination-source.patch`；SHA256 `c188498792c80d7160729026712ce06c16a0f8865bda39487603870b7475aa9f` |
| 工具链 | 本机 Xcode 26.3 / 17C529、iPhoneOS SDK 26.2；新的 build/DerivedData，未复制旧缓存 |
| 设备 | iPad17,2（M5 iPad Pro 11），iPadOS 27.0 / 24A5408d |
| 签名 | Team `LB4Y94KCR7`；bundle `org.openyamm.ipados`；App ID `LB4Y94KCR7.org.openyamm.ipados` |
| 资源 | 同一 195 tag 的六个文件，合计 3,504,948,309 字节；本地 SHA256SUMS 和设备暂存回读均匹配 |
| 原生导入 | 选择整个新 assets 目录，六文件完整导入；ID `E921D559-AF24-4B5F-8D82-30865D6C27F3` |

原始证据根目录为接收 Mac 的 `local-data/device-validation/remote-195-20261029/`。
工作仓库绝对路径为 `/Users/mavskero/Documents/Codex/2026-10-28/openyamm-ipados-https-github-com-mavskero/work/openyamm`。
本目录仅存结论和摘要；原始游戏资源、用户存档、签名材料和凭据不上传。
六文件摘要及最终结论见 [RESULTS.json](RESULTS.json)，选定本机证据路径/大小/摘要见
[EVIDENCE_INDEX.json](EVIDENCE_INDEX.json)。索引不代表原始文件已经上传。
所有 world 均来自本次 tag，清单保持 `worlds/` 路径，未解压 ZIP 或混用旧 engine。
安装及保存前已备份 Documents/Library/tmp 共 1223 文件、12,054,854,313 字节。
AFC 不可读的系统托管 SystemData/元数据未冒充为已备份。
10 个旧存档原件在设备 `saves-preserved-pre195-20261029` 和本机备份中保留，工作目录使用副本。
最终修复版安装前后 11 个工作存档字节一致。写入槽 11 后再冷启动读取，12 个工作存档摘要不变，
10 个旧原件摘要全部不变。普通旅行更新了工作副本 autosave 为 Save104，保留的旧 autosave 未变。

未使用模拟器、卸载、清空容器、重置、传送、无敌或强制任务位；没有重打旧 Mac 战役。
原有 `assets_dev/engine/icons/winBG.PCX` 大小写冲突修改保留，不纳入本次提交。

## 逐项结果

下列路径均相对原始证据根目录。表内通过只对应已观察步骤，不扩展到相邻未验分支。
发布原版上的未受影响通过项沿用本轮证据。首个修复版本摘要为 `9bd1cecb…a66c5`（`fixed-app.json`），
最终 `1965057d…83db` 继续修复保存分页，仅复验分页、新槽保存、Home 和独立进程读档。

| 项目 | 当前结果 | 关键证据 |
| --- | --- | --- |
| 发布源码、App 和六文件资源对应 | 通过 | `release/VERSION.json`、`release/source-archive-verified.json`、`preparation.json`、`install-195.json` |
| 完整原生资源导入 | 通过 | `picker-assets-195.xcresult`、`after-import-195/GameImports/active.json`；原生报告的 `runtime_validated:false` 不代表全内容验证 |
| 建队背景、职业/技能、“无”、取消 | 通过 | `creation-enroth-195`、`creation-detail-cancel-195`；修复版取消见 `fixed-creation-cancel-load-195` |
| 建队技能详情、四项中文精通 | 修复后通过 | 用户实际双指操作，`creation-inspect-manual-expanded.png` 清晰显示普通/专家/大师/宗师；不是 XCTest 合成双指 |
| Enroth 片头 | 通过 | 正常新建单人牧师，`intro-current/second/third` 图及 `intro-finish-195`；自然播放结束进入 oute3 |
| 中文日期、月份、系统槽标签 | 通过 | `save-ui-195-attachments/load-chinese-dates.png` |
| 长中文名、软键盘、退格、取消 | 通过 | `ime-long-name-195`、`ime-edit-cancel-195`；14 字编辑名，按钮可见，取消后旧档摘要未变 |
| 空槽名称初始化 | 原版失败，修复后通过 | `empty-name-repro-195`；最终 `pagination-slot11-195-attachments/slot11-blank-edit-keyboard.png` 空白编辑且按钮可见 |
| 十个手动槽占满后新槽访问 | 原版失败，修复后通过 | `save-capacity-blocker-quit-195`；最终 `pagination-slot11-195` 翻到空槽 11 并保存成功 |
| 三页长卷阅读、前后翻页、关闭 | 通过 | `letter-read-195`、`letter-pages-195`，普通新角色持有的信件，未造物品 |
| 长 NPC 多页选项、问题中文输入 | 未验 | 现有可达酒馆选项可读；尚无对应长分页/问题场景，不以短菜单替代 |
| 技能列表、背包 | 列表/开关通过 | `letter-pages-195`、`checkpoint-character-inspect-195`；修复版可见检查入口 |
| 角色技能双指详情 | 用户确认；展开像素证据不足 | 用户回复“已展开”；`character-inspect-manual-expanded.png` 取到时已关闭，不冒充展开截图 |
| 背包物品详情展开 | 未验 | 背包/技能列表和信件的通过不能扩展到全部物品详情 |
| 法术入口及详情 | 正常新档中通过 | `checkpoint-spell-detail-195` 的治愈虚弱说明/四项精通完整；`checkpoint-return-current.png` 返回户外 |
| 目标卷轴、昏睡队员用药 | 未验 | 自然新档暂无对应物品/状态，不引入夹具 |
| 商店拖离取消、银行金额提交/取消 | 未验 | 本轮未进入对应交易/金额场景 |
| 键位设置前后页/返回 | 导航通过；重新绑定未验 | `key-settings-195`、`keys-pages-save11-open-195`，未改绑定 |
| Arcomage 轻点、上划弃牌、认输 | 通过 | `arcomage-tap-195`、`arcomage-swipe-195`、`arcomage-resign-195`；自然进入酒馆，无造牌/造任务位 |
| 休息天空、沙漏、六项中文、等五分钟、退出 | 通过 | `rest-open-195`；`save-checkpoint-open-195-attachments/rest-finished-clock.png` 为 11:31；日志只开始一次五分钟等待 |
| 普通旅行取消/确认、费用/时间一次结算 | 通过 | `carriage-open-195`、`carriage-travel-195`、`travel-verification.json`；见下节 |
| 旅行失败回滚 | 未验 | 未自然遇到加载失败，未删除正常资源制造失败 |
| 新中文 Save104 写入 | 通过 | 空槽 10“中文保存测试”；`save-chinese-checkpoint-195`、`after-save10-checkpoint-195` |
| 最终新槽保存、Home 恢复、正常退出、独立进程读档 | 最终版通过 | `save11-chinese-195`、`save11-home-quit-195`、`cold-load11-195`、`final-validation.json`；包含真实 App 读取 |
| 当前版音乐和音效 | 用户确认通过 | `audio-user-confirmation.json`，用户明确回复“音乐和音效都能听到” |
| 旧核心路线、潜艇、结局、三大陆战役 | 按范围跳过 | 历史证据见 `handoff/ipados-device-20261028/README.md`；186/Mac 潜艇和 187/Cocoa 不增加本次回放 |

槽 10 为 Save104，1,282,823 字节，SHA256
`25f347aafe6651930e26820f03280754162e7561aa28b3d11a41fc537927f47c`，
`oute3.odm` 单人牧师、11:31 附近。最终另选空槽 11“中文旅行”，Save104，2,071,311 字节，SHA256
`0755f609ca4377f0c6dfab2b444005d60fc4ccb64ce04f48952d801cbba6a3d2`，地图 `outd3.odm`。
旧 RC1/RC2/RC3 不能读取新写 Save104，回退使用保留的旧格式原件副本。

## 最小复现、修复与验证

1. 发布 App 建队/Standard 角色页缺少可见纯触摸检查入口，阻断按住检查加第二指操作。
   建队增加触摸后可见放大镜，沿用已有详情逻辑；显示/命中共用矩形。
   Standard 布局在 iOS 真实触摸后增加法术/检查入口，未强制 Android HUD，也未修改资源 ZIP。
2. 发布 App 双击空槽时把中文“空槽”填入编辑框。修复依据 populated 而非英文 `Empty` 判断真实占用；
   保存失败保留编辑缓冲，成功后才刷新。空槽空白编辑和保存已在设备复验；失败缓冲由主机检查覆盖。
3. save1…save10 都存在时，原版只生成十个候选槽且箭头无效，无法安全另存新槽。
   修复生成 100 个手动槽、十行显示、滚动边界及绝对选择索引；IME 定位到可见行；
   成功滚动取消未提交编辑并清除旧点击状态，避免隐式保存。设备验证新槽 11，边界由主机检查覆盖。

XCTest 驱动增加公开 API 的拖动后保持，用于正常步行，不改游戏状态。

`build-pagination-195.log` 最终设备 Release 构建通过，签名严格校验通过，保留数据更新通过。
`input-pagination-195.log` 有界主机输入检查通过，覆盖触摸释放/取消、显示矩形命中、
真实菜单保存请求、中文编辑缓冲及分页；不替代设备触摸验收。最终设备
`pagination-slot11-195`、`save11-chinese-195`、`save11-home-quit-195`、`cold-load11-195` 均通过。

## 普通旅行与独立进程读档

正常步行进入新索匹格驿站（house 470），报价 25 金币/2 天到铁拳城堡。
先退出取消，回到出发地仍为 200 金币/5 食物；重入后点击路线一次，到 `outd3.odm`，175 金币/5 食物。
日志只有一次 `horse_route` 请求，游戏时间 847.145444 → 3727.145508，差 2880.000064 分钟，
即两天加浮点舍入误差。路线点击就是该服务的确认，无额外确认框（`confirmation_required=false`）。
`travel-verification.json` 保存费用、时间、请求计数及截图路径。

槽 11 保存后 Home 五秒/返回转向通过。正常菜单退出至原生启动页后，首次 SIGTERM 返回成功，
但 PID 57305 仍在，**该次不计独立重启通过**。随后在已正常退出游戏的状态结束进程，
`process-cold-absent.json` 确认 OpenYAMM 不在进程表，重新启动得到 PID 57379。
选择“中文旅行”，`cold-load11-195-attachments/cold-save11-loaded.png` 显示“存档已读取”、175 金币/5 食物。
`app-console-cold-new-process-195.log` 记录 `load_game_started` 和 `load_game_applied`，
路径均为 `saves/save11.oysav`，地图 `outd3.odm`，游戏时间 3727.838013。
读后可转向，最终正常退回原生启动页（`cold-load11-195-attachments/final-normal-launcher.png`）。
`final-validation.json` 和 `final-after-cold-load11-195` 证明最终 12 个工作文件和 10 个旧原件摘要未变。

## 保留的失败和证据限制

- UI 自动化系统认证由用户在设备完成；早期超时是环境阻断，当前已解除。
- OCR 将 Enroth、中文“菜单”、11:31 误识别；原失败 xcresult 保留，依据实际像素和日志分别判断。
- 主菜单读档列表按文件顺序，不能照搬保存页第十行坐标。`fixed-load10-195` 实际误读旧 save8 工作副本，
  没有覆盖或保存；后续异常操作记录不作为本轮自然新档玩法证据。
- 旧副本法术页出现过点击位置与结果不符，原因未定。固定源码与运行包的布局/命中路径未发现分叉；
  在正常 save10 上同点返回、选法术和说明均正常，未为不可重复现象增加经验坐标偏移。
  `fixed-load-new-save-195` 实际加载 save10 成功，但 OCR 把“菜单”识别为“菜単”，保留其失败结论。
- 本轮在最终 195 修复版再次获得扬声器音乐/音效确认，没有沿用 RC2 试听结论。
- 计划参数错误两次在触控前被拒绝（超出等待上限、tapButton 错用 identifier）；修正计划后继续。
  一次“载入游戏”与实际“读取游戏进度”的断言不符和首次 SIGTERM 未终止进程均保留，不计通过。
- 部分文字仍为英文（BodyMagic、键位动作名），本轮不代表全部 UI 中文化。
- 旧首轮移动/转向、房屋影片等证据保留在 `handoff/ipados-device-20261028/README.md`；本轮不重打整套核心路线。

主代理独占构建、集成和设备，子代理按用户指定使用 gpt-6.1-sol/max 做有界源码定位。
完整自然通关、全内容资源审计、所有队伍/任务分支、长时间稳定性及 Windows/MMMerge 存档兼容性未证明。
角色技能展开像素缺口和表内未验项仍保留；后续只在自然场景已具备时补相应项。

Mac mini 接力获取 `codex/ipad-device-debug`，核对修复提交 `7aba6c0cc` 和本记录；已有工作先比较并合并，
保留未提交修改。资源使用固定 195 tag，不以继续更新的 `codex/ipados` HEAD 冒充基线。
使用新机器自己的 Team、build/DerivedData。原始证据仍在本接收 Mac，摘要索引不能替代原文件。
