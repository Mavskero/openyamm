# OpenYAMM 开发状态与RC3交接

更新：2026-10-29。仓库：<https://github.com/Mavskero/openyamm>。
交付分支：`codex/ipados`；原 RC2 交付提交：`ec5a8435`；真机修复分支：`codex/ipad-device-debug`。
可直接交给另一台 Mac 的 Codex 的任务见 [CODEX_IPAD_DEVICE_PROMPT.md](CODEX_IPAD_DEVICE_PROMPT.md)。

## 当前开发：设备无关的集中修改

最新[193集中修复](handoff/ui-function-completion-193/README.md)已完成：读档/建队显示与键位、卷轴目标和HUD、昏睡同伴用药、解雇、室内旅馆、转场确认及回合制恢复。最终原生与未签名通用iOS Release通过；1个已有用例58断言，0新增用例。两段短检查累计9图已查看，首段夹具失败保留；实际范围见记录。继续修复旅行失败回滚和休息资源，不在此设置设备检查点。资源仍为192/runtime-003，Save103不变。

最新[192集中修复](handoff/ui-function-completion-192/README.md)已整合：失败读档/新建恢复原进度，
卷轴施法上下文、商店与银行操作、键位导航、窗口缩放、长存档名以及通关证书文字/背景。
15.408秒组合Mac检查通过，随后只修复截图暴露的显示问题；三图11.780秒及单证书11.629秒复查通过。
9张相关实际图已查看；最终原生006和未签名通用iOS Release002通过。0新增用例，只跑1个已有偷取用例10断言。
没有重打战役、冷启动矩阵、设备操作或新设备检查点。源码/构建限定覆盖与夹具范围见192记录。

当前完整本地资源是`ui-function-completion-192/runtime-003`：继承191/runtime-005，扩展39项文字表、
补充按钮字体布局及原版`winbg.pcx`；世界资源与Save103保持不变。尚未上传为新Release。
以下191和RC3信息保留其历史范围，不代表最新开发资源。整体仍约80–85%的低置信度工程估算，
完整自然战役及新版目标iPad验收没有被这些局部检查替代。

本分支已完成[191基础UI集中修复](handoff/ui-function-completion-191/README.md)：长卷分页、
中文对话排版、短按和取消处理、银行输入、日志翻页、原子保存及错误反馈、制作人员页面。
最终合并Mac短流程通过，9张实际图像已查看；其他输入分支按源码与构建限定接受，Save103不变。
后续补齐菜单、空存档槽、问题输入和道标的中文标签，并修复道标字体漏导入；4张受影响画面已查看，
原生与最终未签名通用iOS Release构建通过。两次检查的明确范围和首次缺字证据见191交接。
继续补齐了Arcomage触摸出牌/上划弃牌/可见认输、小窗口三类详情适配和原版中文属性标签。
卡牌及320×240详情的两段短检查首次通过；5张新增图已查看。最终新增属性标签仅作源码/导入/构建验收。
前一批[190集中开发](handoff/device-independent-completion-190/README.md)补齐任务跟随与交付、
计时、城堡入侵、物品处理、竞技场及海底操作限制、地图规则等源行为。

用户要求先集中完成不依赖真机的工作，本批没有发布RC4、更新设备检查清单或进行设备操作。
191当时完整本地开发资源为`ui-function-completion-191/runtime-005`，继承190，增加原版Credits文本、
26项界面文本表和原版道标字体；
它尚未上传为新Release。下文RC3附件及其188资源是历史版本，不包含190/191/192新增内容。
完整自然战役与全部UI完备尚未证明；具体构建/运行范围以191结果文件为准。

## 已发布RC3与验收边界

本轮交付目标为 **0.1.0-rc3-20261029 / Save103 / checkpoint189**。生产源码基线是
`8f91f2b9276b649c3529fae049beaf91a5668996`，完整资源基线为
`local-data/validation/loadscreen-parchment-188/runtime-001`。RC3 的最终源码归档提交、
文件清单、二进制校验值及候选包实际检查状态，以随包 `VERSION.json` 和 `evidence/` 为准。
现有构建依据是 checkpoint187 的 Mac 程序及 checkpoint188 的未签名通用 iOS Release。
RC3已完成一次独立 `Start-Mac.command` 启动、相对 `../assets` 资源读取、1280×800菜单及
Cmd+Q退出检查，27.763秒、退出码0；没有新建、读档或战役回放。详细状态以随包记录为准，RC2原包保留。

| 项目 | 当前依据 |
| --- | --- |
| Mac 正常游玩与保存 | RC1 正常 MM6 读档、移动、背包、保存及独立重启读取通过；183–188 的受影响短流程见下文。RC3新启动器/相对资源/窗口菜单/退出通过，没有新增游戏内回放；详情见其 VERSION/evidence |
| MM6/MM7/MM8/Merge 必要通关路径 | 原源、实现和分段实际证据；MM7 光明与黑暗终局、Merge 归队/结束卡及续玩均保留各自受控范围 |
| 最近实现 | 已合并真机触控修复，补齐建队背景/移动按钮/片头、卷宗阅读和问题输入，修复 Mac 全屏释放坐标及 UI 皮肤资源导入；Save103 不变 |
| iPadOS 构建 | 原电脑 checkpoint188 未签名通用 arm64 Release 构建通过；接收 Mac 另有早期签名安装报告，两者不等同于 RC3 目标 iPad 验收 |
| 完整自然战役、所有支线/队伍组合 | 未证明；不要求接手端先重打所有战役 |
| 接收 Mac 历史真机反馈 | 接收端报告 `ec5a8435` 及其触控修复分支完成核心短流程；iPad17,2 / iPadOS 27.0 的中文档加载、移动/房屋交互、房屋视频、Home 恢复、背包、菜单、退出及原 8 档哈希均有记录，音乐/音效由接收端用户试听确认。183–188 补丁没有新增目标 iPad 验收 |

2026-10-28 至 29 日新 Mac 首轮设备记录见
[handoff/ipados-device-20261028/README.md](handoff/ipados-device-20261028/README.md)。
原始真机日志、截图和存档仍在接收 Mac；原电脑读取了交接记录及校验索引，没有独立查看这些真机像素。
真实阻断是旧 iOS 配置未启用场景交互按钮、触摸释放未完整进入既有 HUD 路径；
采用一次性设置迁移和共享输入路径修复，保留 Standard 布局、旧存档及原资源。未重打 Mac 战役。
实际 RC2 包缺少建队背景 `makeme` 和部分移动界面图标，Enroth 新游戏片头未触发，已单独记录；
资源六文件全部导入成功不代表内容无缺口，短流程通过不等于完整自然通关。

checkpoint 183 已合并真机修复，并补齐建队背景和 72 张自有按钮图片，恢复主菜单 Enroth 建队后的
原版片头。普通 Mac 新建/片头/Escape/续玩/退出、UI 导入及通用 iOS 构建通过；本补丁没有新真机验收。
详细范围见 [183 交接](handoff/device-feedback-183/README.md)。历史
[183 资源补丁](https://github.com/Mavskero/openyamm/releases/tag/ipados-device-feedback-183-20261029)
保留；当前使用下文的188资源包，它已包含183修复。App 也须更新。

checkpoint 184 修复消息卷宗的头像阅读层被同帧清除、检查手势提前锁存及背包阅读冲突。
原始 VARN 宝箱取卷、阅读、真实机关输入密码、续玩和保存的受控 Mac 短流程通过，通用 iOS
Release通过；见 [184交接](handoff/mainline-playability-184/README.md)。该阶段沿用183资源，
Save103不变；当前交付统一用188。真机只补受影响的卷宗操作，不重做已通过战役。

checkpoint 185 补齐密码/谜题的点击提交、取消、UTF-8退格和键盘避让区域；Mac受影响短流程
与通用iOS Release通过，见 [185交接](handoff/event-question-input-185/README.md)。该阶段没有资源修改；
当前交付统一用188，目标iPad仅补问题输入的触控、中文编辑及键盘遮挡确认。

checkpoint 186 首次受控 Mac 潜艇往返雷格那、原影片/Escape和保存通过；无生产或资源修改，
无新增真机任务，见 [186交接](handoff/mm8-submarine-journey-186/README.md)。

checkpoint 187 修复 Mac 全屏菜单释放坐标错误，一次实际全屏读档菜单/取消/退出通过；仅改Cocoa，
无需新的iPad验证或资源包，见 [187交接](handoff/fullscreen-input-187/README.md)。

checkpoint 188 补齐原版侧栏纹理和所选UI皮肤伴随包顺序，Mac读档页告警消失，通用iOS构建通过。
新[188资源包](https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-resources-188-20261029)
包含183所有资源修复；四个world ZIP仍复用RC2。见 [188交接](handoff/loadscreen-parchment-188/README.md)。

2026-10-29 用户要求继续完成不依赖设备的开发，**不是游戏已经完成**。
历史goal工具仍记录blocked；本批按新指令继续实现，不能把缺少真机数据作为源码开发的停止条件。
原电脑继续无设备操作的集成开发；真机操作授权仅适用于收到用户授权的接手会话。
历史快照中的禁用设备、旧机器路径、过时 next 指令和百分比不是接手任务的新约束。

## GitHub 中有什么

本仓库保留原有项目树和历史，并提交当前所有修改及新增的生产源码、导入器、诊断工具、
CMake 配置和 UI。`ios/`、`scripts/`、`cmake/patches/` 已在版本管理中。
源码提交不嵌入运行 ZIP；当前资源由下述188补丁与RC2四个world附件组合。
RC3 的 Mac 启动包、未签名 iOS 包和固定源码归档分别交付，是否已发布及实际文件名以交付记录为准。
签名凭据、存档、构建缓存和原始截图不纳入这些附件。
真机分支另外提交输入/设置修复、对应单测和交接记录；接手电脑的 `winBG.PCX`
大小写冲突改动保留在该电脑工作区，未纳入提交。

[handoff/ipados-rc2-20261028/](handoff/ipados-rc2-20261028/) 包含：

- RC2 版本、已知限制、1909 文件源码清单及资源 SHA256 清单。
- 当前需求和接受记录的 JSON 副本；证据索引列出原路径、复制路径和 SHA256。
- 原工作区交接文档、阶段记录与启动器快照，全部明确为历史材料。

历史完整日志、截图、受控与正常存档仍在原工作区，不在公开仓库。历史 JSON 中提到的这些路径
表示证据来源，不保证新电脑存在；不要把缺少历史文件改写成一次新验证通过。

## RC3 目录与附件

[RC3 Release已发布](https://github.com/Mavskero/openyamm/releases/tag/ipados-rc3-20261029)，提供Mac启动包、未签名iPadOS包、固定源码及校验/恢复说明。具体限定结果见 [189交付记录](handoff/third-playable-candidate-189/README.md)。固定源码提交为 `09ae801f432c8772230b37ae512392f3c2f94346`，后续交付记录提交只补说明和验收元数据。

本地完整候选目录为 `releases/first-playable-20261029-rc3/`，包含 `Start-Mac.command`、
`macOS/OpenYAMM-RC3.app`、完整 `assets/`、空的 `userdata/saves/`、未签名 iOS App及源码恢复材料。
Mac预编译包要求 **Apple Silicon及macOS 26或更高**，默认1280×800窗口启动。
当前二进制部署目标为macOS 26.0，构建SDK为27.0；旧系统需从源码调整部署目标后自行构建，
尚未验证该路径。保持整个目录的相对结构；单独移动 `.app` 不包含全部运行资源。

GitHub 的 Mac 启动 ZIP 不含 `assets/`。解压后按下文下载资源，或手动复制一份已校验的188
完整 `assets/` 到 `Start-Mac.command` 同级，再启动。未签名 iOS ZIP需接收 Mac使用自己的有效签名
安装；交付副本排除旧 `_CodeSignature` 和 `embedded.mobileprovision`，不能直接安装。
旧 App和存档应保留；设置与存档不能从开发目录混入交付副本。

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

当前完整资源由两处附件组合：从
[188 UI 资源 Release](https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-resources-188-20261029)
下载 `engine.zip`、`conversion.json`、`SHA256SUMS`，从
[RC2 运行资源 Release](https://github.com/Mavskero/openyamm/releases/tag/ipados-rc2-runtime-20261028)
只下载四个world ZIP。188已包含183的建队背景和移动按钮修复，无需另下183补丁或RC2旧engine。
不要用页面自动生成的 Source code ZIP 代替运行资源，也不要解压五个运行 ZIP。

安装 GitHub CLI 后，在新的可写目录或未含assets的RC3 Mac启动包根目录执行：

```sh
mkdir -p assets/worlds
gh release download ipados-ui-resources-188-20261029 --repo Mavskero/openyamm \
  --dir assets --pattern engine.zip --pattern conversion.json --pattern SHA256SUMS
gh release download ipados-rc2-runtime-20261028 --repo Mavskero/openyamm \
  --dir assets/worlds --pattern mm6.zip --pattern mm7.zip --pattern mm8.zip --pattern mmmerge.zip
(cd assets && shasum -a 256 -c SHA256SUMS)
```

188的 `SHA256SUMS` 使用 `worlds/mm6.zip` 等相对路径，因此四个world必须放在 `assets/worlds/`，
校验命令必须在 `assets/` 中执行。已有同名文件时先检查，不添加覆盖参数。
本地完整候选已含资源时只需校验；也可从网页逐个下载并按下方目录摆放。

RC3完整资源目录结构：

```text
assets/
  conversion.json
  engine.zip
  worlds/mm6.zip
  worlds/mm7.zip
  worlds/mm8.zip
  worlds/mmmerge.zip
  SHA256SUMS
```

五个 ZIP 总计 **3,496,520,110 字节，约 3.26 GiB**。也可通过磁盘/AirDrop复制本地完整候选的
`assets` 目录，或把已下载的188基础资源与RC2四个world组装到新目录，再用188的清单校验。
完整六文件的大小和校验值还见 [188资源清单](handoff/loadscreen-parchment-188/runtime-manifest.json)。
RC2历史清单对应旧engine，不能用于确认188完整包。
不要把上游零散 `assets_dev`、事件补丁 ZIP 或 `import_source.py` 的初步提取输出当成完整运行包。

把资源目录放到 iPad“文件”可访问的位置，启动 App，选“选择游戏资源目录”，选择整个
`assets` 文件夹，复制成功后“开始游戏”。不要只选单个 ZIP。
导入复制到 App 自己的 `Application Support/GameImports/<id>`，不自动导入存档。

如需续用旧 OpenYAMM `.oysav`，先复制并保留原件，再通过开发设备容器确认 App 的
`Library/Application Support/saves` 目录。菜单使用 `save1.oysav` 等槽位文件，先检查已有槽位，
只写未占用槽位。普通 Documents 文件共享不会自动放入游戏读取目录。
RC3与RC2均写 Save103，较早旧档加载后保存会升级；
不要用 RC1 读取新保存，不承诺原 Windows/MMMerge 存档兼容。

## 第一轮真机调试

先读已有接收端记录。此前短流程通过时，只补183–185、188受影响的背景/片头/必要按钮、
卷宗阅读、问题输入及侧栏显示；187只改Mac，无新增iPad任务。不重做已通过步骤。
没有目标设备短流程依据时，先让真实程序跑起来，按以下流程推进；只有失败的步骤才定位与局部重验：

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
