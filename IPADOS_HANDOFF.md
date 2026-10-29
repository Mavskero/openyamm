# OpenYAMM iPadOS：214 累计交付

更新：2026-10-29。本次集中交付固定195之后的196–214修复，并包含接收端195真机反馈中的修复。
仓库为 [Mavskero/openyamm](https://github.com/Mavskero/openyamm)，开发分支为 `codex/ipados`。
本次 tag 为 `ipados-ui-cumulative-214-20261029`，见[发布页面](https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-cumulative-214-20261029)。
附件 `VERSION.json` 的 `source_head`、`ios` 和 `runtime_payloads` 是本次源码、App及资源身份的权威记录；
`SOURCE-MANIFEST.json` 记录源码快照，`ARTIFACT-SHA256SUMS` 校验交付附件，`delivery.json` 记录发布核对结果。
不以继续更新的分支HEAD代替发布身份。固定195 tag和RC1/RC2/RC3均保留。

开发分支补充（2026-10-30）：222已修复MM8原生基础旅行，当前分支写Save105；源码/限定检查与6702字节UI及导航增量见 [222说明](handoff/mm8-native-boundary-222/README.md)。它与固定214发布身份分开；接手当前分支时使用配套增量，新写105档不能交给214或旧候选读取。该补充不创建新设备检查矩阵，下面214固定交付提示保持其原范围。

这是一次累计交付，不增加一套设备回放矩阵，也不是继续开发的停工或验收门槛。
[接收端Codex提示词](CODEX_IPAD_DEVICE_PROMPT.md)只有在用户将其作为消息发送到接收Mac会话后才授权该会话操作设备。
保存文档本身不构成授权；原开发主机的设备及模拟器禁令不变。

## 本次包含的改动和证据

- 中文与阅读：建队标点换行、技能精通和法术学派名称、键位两页及日志类别显示；跨地图Notes继续可见。
- 角色与保存：技能手势绑定稳定队员身份，切换队员取消旧操作；奖项内容与滚动条共用实际布局；
  保存失败保留名字草稿，空槽依据真实占用状态初始化；100个手动槽按十行滚动，点击和IME定位使用同一可见行。
- 设置与画面：亮度50–200%滑条、100%中点和世界预览；彩色光、怪物Tinting、音量触摸拖动与室内脚步开关接线；
  设置页“返回游戏”完成关闭链，最终退出房屋时FlipOnExit在室内外统一生效。
- 键位与动作：单一绑定冲突交换，显式绑定优先于M、F9/F10、KP Enter、数字及P等固定快捷键；
  经典键盘16/32转向保留角度步长，长按按时间累计；Yell接入正常玩法，包含500毫秒延迟、67毫秒长按重复和焦点保护。
- 接收端修复：建队及Standard角色页的可见触摸检查入口、空槽名字、保存分页和滚动后的软键盘区域。
  195反馈提交 `dcae3d56` 对应的接收端工作已在 `14137bad` 合并；这些是历史集成标识，不是本发布源码head。

195接收端已记录正常短流程、普通旅行、中文Save104新槽保存、前后台恢复和独立进程重启读档通过，
详见[195接收端记录](handoff/ipados-device-195-20261029/README.md)。原始设备证据仍保存在接收Mac，
仓库中的摘要索引不能替代原始截图或存档，也不能当作214新版的设备验收。

196–214的原生及通用未签名iOS构建与有界受影响检查见对应handoff。
213首次实际Yell检查通过：13.522秒、180个正常续玩帧及保存，2张图已查看。
214长按重复和焦点处理已通过0.833秒输入检查，原生及通用未签名iOS构建通过。
这些是限定范围证据；未新增设备检查或自然战役回放，完整自然通关仍未证明。

## 存档和数据保留

本次继续写入 **Save104**。195已写的104档沿用同一格式；旧RC1/RC2/RC3不能读取新写的104档。
更新前备份当前App数据和全部存档并记录摘要，保留接收端已备份的10个旧存档原件。
旧档只用副本，工作档同样先备份；新保存使用未占用槽位，不覆盖原件。
回退旧程序时配合保留的旧格式存档。Windows/MMMerge原始存档兼容性不在本次已验证范围内。

默认bundle ID为 `org.openyamm.ipados`。接收Mac使用自己的有效Team，保留数据地更新安装。
不要通过卸载旧App、清空容器、重置设备或随意更改bundle ID解决签名问题。
资源导入和存档导入是两件事：资源复制到App的 `Application Support/GameImports/<id>`；
游戏槽位位于App实际的 `Library/Application Support/saves`，普通Documents共享不会自动变成槽位文件。

## 完整资源和附件

本次本地资源基线为 `local-data/validation/ui-function-completion-207/runtime-001`。
相对195，`engine.zip` 和 `conversion.json` 已更新；四个world与195基线为同一实际文件。
新发布仍同时提供全部六文件：`engine.zip`、`conversion.json`、`mm6.zip`、`mm7.zip`、`mm8.zip`、`mmmerge.zip`。
engine保留原有303张休息动画及后来补齐的滑条素材，共享UI文字表为112键。
不要把“world与195相同”误读为可复用RC2：MM6/MM7/MM8在190已经改变。

在一个新的下载目录获取同一tag的完整交付并校验：

```sh
mkdir release-214
gh release download ipados-ui-cumulative-214-20261029 --repo Mavskero/openyamm --dir release-214
(cd release-214 && shasum -a 256 -c ARTIFACT-SHA256SUMS)
mkdir -p assets-214/worlds
cp release-214/engine.zip release-214/conversion.json release-214/SHA256SUMS assets-214/
cp release-214/mm6.zip release-214/mm7.zip release-214/mm8.zip release-214/mmmerge.zip assets-214/worlds/
(cd assets-214 && shasum -a 256 -c SHA256SUMS)
```

使用新的空目录，遇到同名文件先核对，不加覆盖下载参数。`SHA256SUMS` 保留 `worlds/` 相对路径，
因此在组装后的 `assets-214/` 中校验。五个运行ZIP保持压缩状态，不混入旧engine或开发夹具。
将整个 `assets-214` 目录交给App原生资源选择器，完成六文件复制后继续；旧资源目录保留。

未签名App附件为 `OpenYAMM-214-iPadOS-unsigned.zip`，它不是可直接安装的已签名IPA。
源码附件 `source.tar.gz` 包含完整项目代码、配置、交接文档和构建所需源资源；原游戏运行素材由上述六文件提供。
源码快照不包含Git历史、旧主机build/cache或全部原始游戏素材；依赖按固定版本联网获取。
最终源码提交、源码文件数、App摘要及附件摘要以本次VERSION和清单为准，不沿用195的数值。

## 接收Mac签名和可选源码构建

可以用接收Mac有效签名重签未签名App副本，也可以从本次源码快照构建设备Release。
两者择其适合的方式，不要求为了交付再次运行一套构建或游戏回放。
新机器使用自己的工作目录、Team和全新build/DerivedData；不要拷贝旧CMakeCache、`_deps`、
`external-storage/env.sh`、旧DerivedData或NVMe路径别名。
已有调试分支先比较与本次 `VERSION.json → source_head` 的差异，保留未提交工作后合并，不reset/clean。

使用源码附件时，先完成附件摘要校验，解包后在 `openyamm/` 内执行。完整Xcode、iPhoneOS SDK、Git、
CMake、Ninja/make和Python需由本机提供；首次下载和构建时间另计。

```sh
mkdir source-214
tar -xzf release-214/source.tar.gz -C source-214
cd source-214/openyamm
xcode-select -p
xcrun --sdk iphoneos --show-sdk-path
cmake -S . -B build-apple -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_FETCH_DEPENDENCIES_ONLY=ON \
  -DOPENYAMM_BUILD_TESTS=OFF
BX_DIR="$PWD/build-apple/_deps/bx-src" \
  BIMG_DIR="$PWD/build-apple/_deps/bimg-src" \
  make -C build-apple/_deps/bgfx-src shaderc -j3
export OPENYAMM_DEVELOPMENT_TEAM='YOUR_ACTUAL_TEAM_ID'
scripts/configure_ipados_xcode.sh
export OPENYAMM_DEVICE_ID='ACTUAL_DEVICE_IDENTIFIER'
export OPENYAMM_DEVICE_DERIVED_DATA="$PWD/build-device-deriveddata-214"
xcodebuild -project build-ios-xcode/OpenYAMM.xcodeproj \
  -scheme openyamm_ipados -configuration Release \
  -destination "id=$OPENYAMM_DEVICE_ID" \
  -derivedDataPath "$OPENYAMM_DEVICE_DERIVED_DATA" -jobs 3 \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
  CODE_SIGNING_ALLOWED=YES CODE_SIGN_STYLE=Automatic \
  DEVELOPMENT_TEAM="$OPENYAMM_DEVELOPMENT_TEAM" build
```

示例中的Team和设备ID须替换为本机值。`CODE_SIGNING_ALLOWED=YES` 必须显式给出，以覆盖configure脚本的未签名设置。
从本次构建输出确认实际App路径，用Xcode或本机支持的devicectl保留数据地安装。
签名密钥、证书、令牌、provisioning文件及用户存档不进入公开交付物。

## 延续已有验收

195已有匹配证据的短流程不必重打。本次不自动启动45–90分钟检查矩阵，也不重跑潜艇、旧结局、
三大陆战役或已通过的旅行/休息/冷启动全套。正常续玩遇到新版本的具体问题时保留证据并只复查受影响路径。

长NPC分页、问题中文输入、目标卷轴、昏睡队员用药、银行金额、商店拖离取消等旧记录未验项，
只在手头自然具备对应场景时补；无场景继续标未验，不制造物品、任务位或故障资源。
已有角色技能详情“展开像素证据不足”的限制继续保留。195通过不推定214全部设备行为通过，跳过也不写成通过。

如用户在接收端明确启动后续设备工作，证据放入新的 `local-data/device-validation/cumulative-214-<实际日期>/`。
记录实际源码/App/资源身份、通过/失败/未验及最小复现；原始失败与10个旧存档原件持续保留。
主代理独占共享构建目录、集成和设备；该发布不改变原开发主机的设备限制，也不阻止持续的设备无关修复。
