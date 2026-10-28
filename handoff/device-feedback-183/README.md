# 首轮真机反馈修复：checkpoint 183

已合并真机分支 `73205957` 的场景触控和一次性 iOS 设置迁移；原真机报告与证据索引保留在
`handoff/ipados-device-20261028/`。本轮在原电脑完成源码修复、资源导入、Mac 短流程及通用 iOS
构建，没有操作真机或模拟器，也没有重打既有战役。

## 本轮改变

- 建队背景：UI 导入器为无扩展名的图片引用解析原 LOD 的 `.pcx` 成员，正常生成 `makeme.pcx`。
- 移动界面：`assets_dev/engine/ui/gameplay/owned_images.txt` 列出 72 张仓库已有 PNG，覆盖移动按钮、
  摇杆、暂停/继续及场景交互图标。导入器输出至 `engine/icons`；iOS 同时携带清单和原图，支持设备端
  从原始资源转换。不强制切换已验证的 Standard 布局。
- Enroth 片头：正常主菜单建队成功后播放已有 `6intro.bik`。原 Merge 的触发依据是
  `Scripts/General/MenuChooseContinent.lua` 的主菜单大陆选择，不是地图 OnLoad。
  本次在建队后播放，较原源时机稍晚；读档、重返地图、游戏内新建均不走此入口。
  没有添加地图 overlay、修改任务位或升级 Save103。

## 已实际确认

原始 UI 重新导入成功，73 个新增资源为建队背景和 72 张 PNG；其余本轮导入资源与 181 基线相同，
四个 world ZIP 沿用原件。全部 PNG 在导入时实际解码，iOS bundle 中的 72 张与源码逐字节一致。
Mac 与未签名通用 iOS Release 构建通过。

一次普通 Mac 新建：主菜单选择 Enroth，男性骑士，正常分配 15 点并选择 Bow/Armsmaster。
建队背景可见；原片头实际播放；Escape 后进入正常户外画面，菜单及正常退出通过。
没有无敌、传送、预置队伍或任务位夹具。没有新增测试用例或运行测试矩阵。

F9 曾通过自动操作发送，但目录未出现存档；本轮不声称保存/冷读通过，也未将未定位的按键投递
问题认定为存档缺陷。此前真机正常中文保存/独立重启读取结果仍保留其原有范围。
此次新增片头的冷读不重播依据是调用路径隔离，没有另跑冷读。

本轮完整本地记录为 `local-data/validation/device-feedback-183/scoped-result.json`，截图、日志和
独立运行目录保留在原 NVMe 工作区。新完整开发运行目录为 `device-feedback-183/runtime-001`；
历史 RC2 原件不变。新补丁的目标 iPad 显示/触控仍需接手端局部确认；完整自然通关仍未证明。

## 下一台电脑接手

拉取最新 `codex/ipados`，按 `IPADOS_HANDOFF.md` 展开清单所列 PNG 后重新配置/构建。
代码修复需要更新 App；背景和按钮资源修复还需要更新 `engine.zip` 与 `conversion.json`。
四个 world ZIP 无变化。只复验本轮改变的背景、片头和必要按钮，不重复原首轮全流程。

主代理与新子代理按最新用户要求使用 `gpt-6.1-sol / max`；主代理独占集成、构建和应用操作。
