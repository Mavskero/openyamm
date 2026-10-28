# iPadOS RC2 接手与真机调试

更新：2026-10-29。仓库：<https://github.com/Mavskero/openyamm>。
原交付分支：`codex/ipados` / `ec5a8435`；真机修复分支：`codex/ipad-device-debug`。
可直接交给另一台 Mac 的 Codex 的任务见 [CODEX_IPAD_DEVICE_PROMPT.md](CODEX_IPAD_DEVICE_PROMPT.md)。

## 当前版本与验收边界

源码基线对应 **0.1.0-rc2-20261028 / Save103 / checkpoint182**，完整开发资源基线是
`merge-source-polish-181/runtime-001`。原交付提交汇总 RC2 源码及接手材料，
并修正新机配置脚本在 macOS Bash 3.2 中展开空依赖参数数组的错误。
本轮在此基础上修复 iOS 场景交互入口和触摸释放处理，完成真机短流程及受影响步骤复验。

| 项目 | 当前依据 |
| --- | --- |
| Mac 正常游玩与保存 | RC1 候选实际启动、正常 MM6 读档、移动、背包、保存、独立重启读取通过；RC2 新启动器菜单启动/正常退出通过 |
| MM6/MM7/MM8/Merge 必要通关路径 | 原源、实现和分段实际证据；MM7 光明与黑暗终局、Merge 归队/结束卡及续玩均保留各自受控范围 |
| 最近实现 | 主线取得/重入、瓶中神殿返回、Inn 永久遣散与槽回收、BrAlvar 普通交互/角色关系/AI、MM7 后期控制方块刷新、迷宫反馈与脚步映射 |
| iPadOS 构建 | 新 Mac 的 Xcode 26.3 / iPhoneOS SDK 26.2 已完成 arm64 Release 自动签名、保留数据安装及实际运行 |
| 完整自然战役、所有支线/队伍组合 | 未证明；不要求接手端先重打所有战役 |
| 当前版本真机 | `ec5a8435` 完成核心短流程；本分支修复版在 iPad17,2 / iPadOS 27.0 复验中文档加载、触控移动与房屋交互、房屋视频、Home 恢复、背包、菜单和正常退出。原 8 档全部哈希不变。音乐/音效由用户试听确认；片头及长期性能仍未验收 |

2026-10-28 至 29 日新 Mac 首轮设备记录见
[handoff/ipados-device-20261028/README.md](handoff/ipados-device-20261028/README.md)。
真实阻断是旧 iOS 配置未启用场景交互按钮、触摸释放未完整进入既有 HUD 路径；
采用一次性设置迁移和共享输入路径修复，保留 Standard 布局、旧存档及原资源。未重打 Mac 战役。
实际 RC2 包缺少建队背景 `makeme` 和部分移动界面图标，Enroth 新游戏片头未触发，已单独记录；
资源六文件全部导入成功不代表内容无缺口，短流程通过不等于完整自然通关。

checkpoint 183 已合并真机修复，并补齐建队背景和 72 张自有按钮图片，恢复主菜单 Enroth 建队后的
原版片头。普通 Mac 新建/片头/Escape/续玩/退出、UI 导入及通用 iOS 构建通过；本补丁没有新真机验收。
详细范围见 [183 交接](handoff/device-feedback-183/README.md)。新运行包使用原 RC2 四个 world ZIP，
再从 [183 资源补丁](https://github.com/Mavskero/openyamm/releases/tag/ipados-device-feedback-183-20261029)
取得新的 `engine.zip`、`conversion.json` 和校验清单。App 也须重新构建更新。

2026-10-29 用户依据新真机反馈恢复持续开发 goal，当前为 active，**不是游戏已经完成**。
原电脑继续无设备操作的集成开发；真机操作授权仅适用于收到用户授权的接手会话。
历史快照中的禁用设备、旧机器路径、过时 next 指令和百分比不是接手任务的新约束。

## GitHub 中有什么

本仓库保留原有项目树和历史，并提交当前所有修改及新增的生产源码、导入器、诊断工具、
CMake 配置和 UI。`ios/`、`scripts/`、`cmake/patches/` 已在版本管理中。
源码提交不嵌入运行 ZIP；按用户追加的测试要求，完整 RC2 运行资源另放在下述 GitHub Release。
签名凭据、存档、构建缓存、二进制程序或原始截图没有上传。
真机分支另外提交输入/设置修复、对应单测和交接记录；接手电脑的 `winBG.PCX`
大小写冲突改动保留在该电脑工作区，未纳入提交。

[handoff/ipados-rc2-20261028/](handoff/ipados-rc2-20261028/) 包含：

- RC2 版本、已知限制、1909 文件源码清单及资源 SHA256 清单。
- 当前需求和接受记录的 JSON 副本；证据索引列出原路径、复制路径和 SHA256。
- 原工作区交接文档、阶段记录与启动器快照，全部明确为历史材料。

历史完整日志、截图、受控与正常存档仍在原工作区，不在公开仓库。历史 JSON 中提到的这些路径
表示证据来源，不保证新电脑存在；不要把缺少历史文件改写成一次新验证通过。

## 另一台 Mac 克隆

优先 Apple Silicon Mac、完整 Xcode（含 iPhoneOS SDK）、Git、CMake 3.24 或更新版本、
Ninja、make 和 Python 3。CMake 自动下载固定版本的 SDL3、bgfx、FFmpeg、Lua 等依赖，
不要用另一台电脑的系统库替换已固定的版本。首次配置需要网络。

以下稀疏检出覆盖当前 Apple 构建源码，避免首次下载上游历史中的大量旧资产；
仓库里的其他目录仍保留在 Git 中，需要时再检出。

```sh
git clone --filter=blob:none --sparse --branch codex/ipados \
  https://github.com/Mavskero/openyamm.git
cd openyamm
git sparse-checkout set .github android cmake engine game ios packaging scripts \
  tests tools util third_party/doctest handoff \
  assets_dev/engine/rendering assets_dev/engine/scripts/common assets_dev/engine/ui
git sparse-checkout reapply --no-cone
sed 's#^#/assets_dev/engine/icons/#' assets_dev/engine/ui/gameplay/owned_images.txt \
  | git sparse-checkout add --stdin
git log -1 --oneline
```

在这台电脑选定实际工作目录。不要复制旧 `CMakeCache.txt`、build 目录、`_deps` 缓存、
DerivedData、`external-storage/env.sh` 或旧机器的绝对路径/符号链接。
原电脑的 NVMe 挂载和路径别名要求只保护原电脑现有缓存，不要求新电脑创建相同目录。
若新电脑也用外置盘，先确认实际挂载，再把构建、缓存和结果放在该盘项目目录。

## 最短构建与签名路径

先用 `xcode-select -p`、`xcrun --sdk iphoneos --show-sdk-path` 和工具版本确认环境。
如 Xcode 未选择正确，可在当前终端设置 `DEVELOPER_DIR` 指向实际 Xcode 的
`Contents/Developer`；不要假设旧电脑的 Xcode 路径和签名账户存在。

在新仓库根目录只准备依赖和主机 shaderc，随后直接生成 iOS Xcode 工程：

```sh
cmake -S . -B build-apple -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_FETCH_DEPENDENCIES_ONLY=ON \
  -DOPENYAMM_BUILD_TESTS=OFF
BX_DIR="$PWD/build-apple/_deps/bx-src" \
  BIMG_DIR="$PWD/build-apple/_deps/bimg-src" \
  make -C build-apple/_deps/bgfx-src shaderc -j3

export OPENYAMM_DEVELOPMENT_TEAM='YOUR_ACTUAL_TEAM_ID'
scripts/configure_ipados_xcode.sh
```

这复用 `scripts/build_apple.sh` 的依赖/shaderc 准备步骤，避免先构建全部桌面诊断目标。
配置脚本在缺少 `build-ios/_deps` 时会自行下载 Xcode 构建所需的固定依赖。
这是由现有脚本整理的接手命令；新 Mac 首轮设备记录已补充实际构建与运行证据。
以后若需要 Mac 目标，应重新配置 `OPENYAMM_FETCH_DEPENDENCIES_ONLY=OFF` 并传入
`OPENYAMM_HOST_SHADERC`，或使用 `scripts/build_apple.sh macos`。

默认 bundle ID 是 `org.openyamm.ipados`。若新 Team 无法使用它，先检查设备上已有 App
及其数据，再选择可用 ID，并通过 CMake 设置，不能只改生成的 Xcode 工程：

```sh
cmake -S . -B build-ios-xcode \
  -DOPENYAMM_BUNDLE_IDENTIFIER=YOUR_AVAILABLE_BUNDLE_ID
```

`OPENYAMM_BUNDLE_IDENTIFIER` 是 CMake 缓存参数，不是配置脚本自动读取的环境变量。
改变 bundle ID 会产生另一个 App 数据容器。不要卸载已有 App 来绕过签名或数据问题。

选择用户提供/连接的目标 iPad，确认开发者模式、信任、解锁和签名账户后构建：

```sh
export OPENYAMM_DEVICE_ID='ACTUAL_DEVICE_IDENTIFIER'
export OPENYAMM_DEVICE_DERIVED_DATA="$PWD/build-device-deriveddata"
xcodebuild -project build-ios-xcode/OpenYAMM.xcodeproj \
  -scheme openyamm_ipados -configuration Release \
  -destination "id=$OPENYAMM_DEVICE_ID" \
  -derivedDataPath "$OPENYAMM_DEVICE_DERIVED_DATA" -jobs 3 \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
  DEVELOPMENT_TEAM="$OPENYAMM_DEVELOPMENT_TEAM" build
```

不要在这条设备构建命令加入 `CODE_SIGNING_ALLOWED=NO`；该参数只用于历史未签名检查。
工程对依赖全局禁用签名，App target 自己启用 Automatic 签名。
从本次构建输出或 `TARGET_BUILD_DIR`/`FULL_PRODUCT_NAME` 确认真正 `.app` 路径再安装，
不要照抄历史 `build-ios-xcode/ios/Release-iphoneos` 路径。
可用 Xcode Run 或已安装 Xcode 的 `devicectl` 安装/启动；先查本机命令帮助，保留旧 App 数据。
不在交接文档、日志或 Git 中保存账户密码、私钥、证书密码、令牌或 provisioning 文件。

## 游戏资源下载与传输

完整资源可从 [RC2 运行资源 Release](https://github.com/Mavskero/openyamm/releases/tag/ipados-rc2-runtime-20261028)
下载。附件是六个原样文件，另有 `SHA256SUMS` 和目录说明；不要用页面自动生成的
Source code ZIP 代替运行资源，也不要解压五个运行 ZIP。

安装 GitHub CLI 后，在新的可写目录执行：

```sh
mkdir -p assets/worlds
gh release download ipados-rc2-runtime-20261028 --repo Mavskero/openyamm \
  --dir assets --pattern engine.zip --pattern conversion.json --pattern SHA256SUMS
gh release download ipados-rc2-runtime-20261028 --repo Mavskero/openyamm \
  --dir assets/worlds --pattern mm6.zip --pattern mm7.zip --pattern mm8.zip --pattern mmmerge.zip
(cd assets && shasum -a 256 -c SHA256SUMS)
```

已有同名文件时先检查，不添加覆盖参数。也可从网页逐个下载并按下方目录摆放。

现有完整 RC2 资源在原电脑：

```text
/Volumes/NVMeSSD/Projects/openyamm-Ⅰpados/releases/first-playable-20261028-rc2/assets/
  conversion.json
  engine.zip
  worlds/mm6.zip
  worlds/mm7.zip
  worlds/mm8.zip
  worlds/mmmerge.zip
```

五个 ZIP 总计 **3,494,489,946 字节，约 3.25 GiB**。也可通过磁盘/AirDrop
复制整个本地 `assets` 目录。仓库内对应校验清单在
[runtime-SHA256SUMS](handoff/ipados-rc2-20261028/runtime-SHA256SUMS)。
把清单路径换成克隆仓库实际路径后，可在资源目录执行 `shasum -a 256 -c /path/to/runtime-SHA256SUMS`。
不要把上游零散 `assets_dev`、事件补丁 ZIP 或 `import_source.py` 的初步提取输出当成完整运行包。

把资源目录放到 iPad“文件”可访问的位置，启动 App，选“选择游戏资源目录”，选择整个
`assets` 文件夹，复制成功后“开始游戏”。不要只选单个 ZIP。
导入复制到 App 自己的 `Application Support/GameImports/<id>`，不自动导入存档。

如需续用旧 OpenYAMM `.oysav`，先复制并保留原件，再通过开发设备容器确认 App 的
`Library/Application Support/saves` 目录。菜单使用 `save1.oysav` 等槽位文件，先检查已有槽位，
只写未占用槽位。普通 Documents 文件共享不会自动放入游戏读取目录。
RC2 写 Save103，旧档加载后保存会升级；
不要用 RC1 读取新保存，不承诺原 Windows/MMMerge 存档兼容。

## 第一轮真机调试

先让真实程序跑起来，按以下短流程推进；只有失败的步骤才做定位与局部重验：

1. 确认安装/启动，完成完整资源目录导入，普通新建或复制的正常存档进入游戏。
2. 触摸移动、转向、停止，以及一次菜单/背包/交互，确认画面和输入对应。
3. 新名字保存；检查中文输入、键盘关闭和画面恢复。保留设备上已有存档。
4. Home 后返回，继续操作；正常退出后独立重启，读取刚保存的进度。
5. 实际试听声音和一段必要过场；观察一段短正常运行的崩溃、内存与卡顿。
   锁屏、耳机或键鼠仅在目标设备具备条件时补对应检查。

出现失败先区分产品、签名/资源环境、自动操作脚本和玩家操作问题。
不要直接改任务位、无敌、传送或清空敌人使流程通过；必须使用夹具时明确标注。
不重打已经通过的 Mac 路线，不预设所有战役、所有职业或长期刷新矩阵。

现有 `ios/UITests/` 可用于必要的短 UI 操作，不是每一步的前置条件。
其目标 bundle ID 硬编码为 `org.openyamm.ipados`；若改 App ID，先改为一致再运行。
真机计划结果放入新的 `local-data/device-validation/<run>/`，保存日志、截图、真实存档及结论。
操作/安装成功不等于游戏行为通过，源码核对不等于真机验收，受控片段不等于自然通关。

## 开发约定与继续提交

主代理负责架构、集成、共享构建目录和设备。需要子代理时，按用户指定使用
`gpt-6.1-sol / max`，先做有界只读定位，再分配独立文件；不允许并行构建或操作同一设备。
最新用户指定主、子代理均为 `gpt-6.1-sol / max`；实际模型以接手端可配置设置为准，不冒称已切换。
保留未提交改动、失败记录和原始资源。修复共享机制；导出的 Lua 在导入器或原源 overlay 修，
不要改生成物，也不要复制参考引擎代码。具体规则见 [AGENTS.md](AGENTS.md)。

从此交付点创建 `codex/ipad-device-debug` 分支进行真机修复，提交最小必要改动与新的交接结论。
签名账户登录、设备信任/解锁、缺失资源位置等必须由人完成时，清楚说明具体缺什么；
已有明确授权的操作不反复询问。
