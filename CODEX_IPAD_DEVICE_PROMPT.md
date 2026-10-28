# 在另一台 Mac 的 Codex 中粘贴以下提示词

接手 OpenYAMM iPadOS 项目，仓库 https://github.com/Mavskero/openyamm ，交付分支 `codex/ipados`。
先读取 `AGENTS.md`、`IPADOS_HANDOFF.md` 及 `handoff/ipados-rc2-20261028/README.md`，
确认当前 Git 提交和工作区改动；未克隆时按交接文档克隆。以 RC2 / Save103 为基线，
创建 `codex/ipad-device-debug` 分支（若已存在则检查并继续，不重置）。

本次目标：**尽快在这台 Mac 连接的目标 iPad 上完成签名、安装、完整资源导入、正常游玩、
中文保存、前后台恢复和重启读档，发现实际阻断就修复，直到首轮真机短流程通过。**
先运行应用，少做回测、少加测试用例，不重跑已通过的 Mac 战役，不把构建成功当成真机通过。

**我明确授权本接手会话操作我提供/连接的目标 iPad**：识别设备，使用本机已有/我配置的
开发者账户签名，安装或保留数据地更新 OpenYAMM，启动与调试、读取应用日志、截图，
导入我合法拥有的游戏资源，以及进行必要的触控、存读档、前后台、音画与短时性能检查。
此授权覆盖历史文档中的“No device or simulator operations”的真机部分，仅适用于本接手任务；
仍不使用模拟器。不卸载已有 App、不清空容器、不覆盖或删除旧存档、不重置设备。
需要覆盖安装前先确认旧数据可保留/备份；不上传证书、私钥、密码、令牌或原始游戏资源。

新电脑使用实际工作目录和新的构建目录，不复制旧机器的 CMakeCache、DerivedData、
NVMe 路径别名或 external-storage/env.sh。检查完整 Xcode、iPhoneOS SDK、CMake/Ninja、
目标设备和本机签名 Team；每次 xcodebuild 明确 scheme 和项目内 derivedDataPath。
先按 `IPADOS_HANDOFF.md` 的最短构建路径准备 shaderc 和 iOS 工程，再构建设备 Release。

完整资源已作为 Release 提供：
https://github.com/Mavskero/openyamm/releases/tag/ipados-rc2-runtime-20261028 。
按 IPADOS_HANDOFF.md 下载并组装 assets/：conversion.json、engine.zip，以及 worlds 下
mm6/mm7/mm8/mmmerge 四个 ZIP；不要解压 ZIP。核对 SHA256SUMS，再从 iPad 启动页
选择整个资源文件夹。也可使用原电脑 RC2 assets 的完整副本。
资源下载期间继续不依赖资源的构建/签名准备；网络或权限失败时说明实际问题，不虚构资源已就绪。

第一轮按“启动/导入 → 正常进入游戏 → 触摸移动转向及一次 UI 交互 → 中文保存 →
Home/返回继续 → 退出重启读取”推进，再补必要音画和短运行观察。旧存档只用副本。
出现失败先查原因，区分产品缺陷、设备环境和自动操作问题，修完仅复验受影响步骤。
不要使用无敌、强制任务位或传送掩盖问题；受控夹具须明确标记。

主代理和子代理均希望使用 `gpt-6-astra / xhigh`；按接手端实际能力配置，
不能切换时如实说明。可使用多个子代理做有界源码定位，各自独占文件；
主代理独占共享构建目录、集成和设备，避免并发操作。不要恢复旧代理任务或沿用旧 blocked 状态。

保留原始资源、旧存档、所有未提交改动和失败记录；不 reset/clean。
将本次设备日志、截图、真实存档及结论保存到新的 `local-data/device-validation/` 子目录，
在修复分支提交源码和交接更新；不要自行公开发布或上传游戏资源。
遇到必须由我登录账户、信任/解锁设备或提供资源时，说明具体所缺信息；
其余已授权工作持续推进，不反复请求同一许可。

完成后给出：实际提交、设备和系统版本、签名/App ID、成功短流程、修复项、剩余问题及
关键证据路径。完整自然通关仍未证明时明确保留这一限制，不夸大首轮真机结果。
