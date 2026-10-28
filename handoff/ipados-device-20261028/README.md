# iPadOS 首轮真机交接：2026-10-28 至 29 日

**首轮正常短流程通过，场景触摸交互阻断已修复并局部复验；完整自然通关未证明。**
从 `ec5a8435f32b1f75e71370c045f9bd5f3a604e08` 创建并保留
`codex/ipad-device-debug`。本目录随修复提交保存，实际提交号可用 `git log -1` 获取。
没有重打 Mac 战役，没有模拟器、卸载、清空容器、覆盖旧档、修改任务位或传送。

## 环境与可追溯构建

| 项目 | 实际值 |
| --- | --- |
| 新主机 | Apple M3 Max，macOS 15.6.1 / 24G90 |
| 工具链 | Xcode 26.3 / 17C529，iPhoneOS SDK 26.2，CMake 4.4.3，Ninja 1.13.2 |
| 目标真机 | iPad17,2（M5 iPad Pro 11），iPadOS 27.0 / 24A5408d；已配对、开发者模式、DDI 可用 |
| 签名 | 本机 Apple Development，Team `LB4Y94KCR7`，Automatic；App ID `LB4Y94KCR7.org.openyamm.ipados` |
| 安装 | Release，bundle ID `org.openyamm.ipados`，保留数据更新；最终签名严格校验通过 |
| 最终二进制 | 20,310,336 字节，SHA256 `c9e926bb8ae151c504f156e33d875af05f1d5dcaf7fb74ea590394810bfe8ede` |
| 资源 | 原 RC2 六文件，3,494,497,338 字节；本地和 iPad 暂存回读均匹配原交付 SHA256 |

这台 Mac 使用自己的工作区、Xcode 和全新 `build-apple`、`build-ios-xcode`、
`build-device-deriveddata`、`build-ios-uitests`。旧 NVMe 路径仅作为 `ssh macmini` 的资源读取来源。
Apple 登录、协议接受及管理员认证由用户完成。未提交证书、私钥、profile 或账户凭据。

## 已通过的流程及边界

下表证据路径以 `local-data/device-validation/20261028-local-001/` 为根；原件只在本机，
不纳入 Git。文件大小和 SHA256 见 [EVIDENCE_INDEX.json](EVIDENCE_INDEX.json)。

| 步骤 | 结果及证据 |
| --- | --- |
| 安装、启动 | 基线和最终修复版均实际运行。`device-release-build-03.log`、`device-context-fix-build.log`、`install-context-fix.json`；最终进程 50443 的 AX 记录在 `context-fix-final-normal-quit-attachments/` |
| 完整资源导入 | 在原生文件夹选择器导入全部六文件，启动页显示“已导入 6 个文件”。`runtime-sha256-verified.json`、`device-staged-runtime.json`、`import-rc2.xcresult`；导入 ID `1880A14C-2C7B-45D5-8BB9-19B0799BFB48`，原有导入保留 |
| 正常新建 | 普通 Enroth 建队、单人男性骑士 Lv1、15 点正常分配，进入 `oute3.odm`。`normal-game-world-attachments/` 和 `app-console.log` 的 `new_game_started`；未用预置队伍、无敌或坐标作弊 |
| 触摸移动、转向和停止 | 左侧拖动移动、右侧拖动转向，画面与坐标变化对应；松手观察无漂移。`touch-move-turn.xcresult`、`touch-idle-inventory.xcresult`；最终版继续正常步行至酒馆 |
| 中文新存档 | 明确选空槽 9，通过屏幕拼音键盘及候选词输入“真机首测”，关闭键盘、保存并恢复画面。`save9-ime-commit-attachments/`、`save9-return-save-attachments/`、`save9-first-write/verification.json` |
| Home / 返回 | 基线在背包中 Home 5 秒、返回后继续操作。最终版在房屋中 Home / 返回后仍可退出房屋、开背包。`background-return.xcresult`、`context-fix-house-and-ui.xcresult`、`context-fix-exit-house-inventory.xcresult`；后两个名称不应误解为在背包中后台恢复 |
| 正常退出、独立重启读档 | 基线从菜单正常退出至原生启动页，再独立进程启动，PID 49733 → 50076；正常读取槽 9，时间、坐标、朝向一致。`normal-quit.xcresult`、`cold-launcher-start.xcresult`、`app-console-relaunch.log` 的 `load_game_applied`。最终版再次从新进程读取同一档：`context-fix-load-save9.xcresult` |
| 房屋交互和视频 | 最终版正常步行至门前，出现“孤独骑士”按钮；真实触摸进入 house 260 / event 11。酒馆人物举杯动作的不同帧已目视确认，随后正常退出。`context-fix-near-door.xcresult`、`context-fix-enter-tavern.xcresult`、`context-fix-house-and-ui.xcresult`；这证明房屋动态视频，不代表缺失的 Enroth 片头通过 |
| 背包、菜单、退出 | 最终版退出房屋后开关背包、打开菜单、正常退出，原生页显示“已返回启动页”。`context-fix-exit-house-inventory.xcresult`、`context-fix-final-normal-quit.xcresult`；结束时引擎已停止，原生 App 保留在启动页 |
| 扬声器声音 | 用户直接反馈“能听到音乐和音效”，见 `audio-user-confirmation.json`；另有音乐与房屋音效日志，未仅凭日志认定出声 |
| 短运行观察 | 基线读档后户外静置 71.504 秒，72 个 FPS 日志样本，最低 118.89、中位 119.988、最高 120.096，无低于 60 样本及该段崩溃标记。`short-observation.json`；诊断日志开启，非帧耗时基准、长时稳定性或内存泄漏测试 |

核心流程先在交付基线上完成，修复后只复验受影响的读档、触控、交互、恢复和退出步骤。
最终版没有重新导入资源或重新创建中文档；不将这种局部复验写成全流程重新执行。

## 修复与验证

旧 iOS 配置继承 `context_action_popup=false`，相机触摸区域不会直接激活场景，导致无可用
入口进入房屋。最终修复将 iOS 新设置默认启用场景交互，并对旧配置执行一次 `ios / 1` 迁移。
已迁移后用户手动关闭时不再次强制打开；更新版本的 profile 也保持不变。
`GameplayScreenController` 的两处原 Android 触摸保持/释放路径同时适用于 iOS，覆盖头像和 HUD 点击。

生产改动仅在 `GameSettings.h/.cpp`、`GameApplication.cpp`、`GameplayScreenController.cpp`。
保留原 Standard HUD、声音、世界、诊断及玩法设置；实际回读 `final-settings-diff.json` 证明
仅 profile 和 context 开关改变，资源根路径另外因系统安装更新而更换容器 UUID。
`IosSettingsMigrationTests.cpp` 两项单测、21 个断言通过，覆盖迁移保留字段、幂等及新版本 profile。
最终 Release 构建成功、签名校验成功、保留数据更新成功，并完成上表实际设备复验。

曾尝试移动布局，但 RC2 包缺少其按钮图片，出现不可见入口；实验改动已撤回。
未保留 `GameplayUiController.cpp` 改动或强制 Widescreen。实验前后的设置和回读存于
`settings-experiment-restoration.json`、`settings-restored-before-final-migration.ini`。
最终迁移在恢复原 Standard 配置后实际执行。原有 `winBG.PCX` 大小写冲突改动未重置、未提交。

## 存档保护

安装前备份 8 个旧存档；安装后、中文新档后、重启读档后和所有更新结束后分别回读校验。
最终 `saves-final.json` 的 8 个 `unchanged` 均为 true，目录只新增槽 9。
没有读取并改写任一旧档，也未用新档覆盖旧槽。保存菜单默认选中已有槽，后续继续操作必须显式选空槽。

新档 `save9.oysav`：名字“真机首测”，Save103，户外 `oute3.odm`，1 人队伍，1,257,751 字节，
SHA256 `a1cbc3fbec10effdf9228fb2632c0a105880e0268a2b0494409101797a0615e5`。
首次保存、冷启动读档后、最终退出后哈希相同。名字和相邻 BMP 结构已校验；完整语义由实际加载
证明，不把头部解析当作完整解析器。房屋复验期间未再保存，设备中的槽 9 仍是首次保存位置。

## 未完成项与失败分类

- RC2 建队背景 `makeme` 缺失，建队背景黑但流程可用。只读定位指向原始 `.pcx` 名称解析/资源组装；
  应修导入器的权威解析再重导对应资源，不直接改游戏原档或生成事件。
- Enroth `oute3_intro.lua` 触发 overlay 未打入运行包；`6intro.bik` 视频存在。
  正常新游戏片头未播放，不能以酒馆视频通过替代。后续应显式注册 overlay，再仅复验片头及读档不重复触发。
- RC2 缺少部分移动 HUD 的 pause/inventory/attack/cast/quick_cast/resume、升降、摇杆 PNG。
  最终保留可用 Standard 布局。场景按钮的图标也缺失，但程序绘制的边框和“孤独骑士”文字可见并能操作。
  后续用明确资源清单补入自有 UI 图片，避免把运行时生成的纹理误判为缺图。
- 部分标签仍混用英文和中文；本轮未扩展到完整本地化。
- 原生资源报告仍为 `runtime_validated:false`，它没有证明所有内容完整。六文件导入成功和核心可玩
  不消除上述内容缺口；本轮没有重打完整战役、所有支线/队伍组合、长时温控/内存、锁屏、耳机或外设矩阵。
- `cold-load-save9.xcresult` 原始结论为失败：读档已成功，但装饰性 Menu 的 OCR 断言漏检；
  实际截图、`load_game_applied`、坐标及文件哈希证明读档。保留原失败结果，不将它改成绿色。
- 早期文件夹滚动、文字识别及一次 drag 参数错误属于自动操作失败，修正后继续；
  缺平台组件和手动签名拒绝属于环境失败，最终 Automatic 构建通过。显式进程重启的 signal 9 不计为崩溃。

本机原始日志、`.xcresult`、计划 JSON、截图、视频、真实存档和设置快照均保留在上述 run 目录。
本目录只提交结论与哈希索引；本地 `outputs/` 另存可直接阅读的报告和关键证据副本。
主代理独占构建/集成/设备；只读子代理按用户指定使用 Astra/xhigh。主代理模型未通过工具切换，
不声称已经切换成功。此次没有推送、创建 PR 或公开发布资源。
