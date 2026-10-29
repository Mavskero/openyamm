# OpenYAMM iPadOS：195 远程测试交接

更新：2026-10-29。仓库：[Mavskero/openyamm](https://github.com/Mavskero/openyamm)，源码分支 `codex/ipados`。
本次按用户要求准备远程 iPad 测试包，整合183–188及190–195的修复。旧RC1、RC2、RC3保留。

发布 tag：`ipados-ui-function-195-20261029`。
[发布页面](https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-function-195-20261029)
的附件 `VERSION.json` 固定195源码提交、App附件名及摘要；`ARTIFACT-SHA256SUMS` 校验交付附件。
源码文档引用该记录，避免用继续开发中的分支HEAD代替发布版本。新版目标iPad尚待接收端验收。
权威交付字段与受影响检查表见[195远程测试记录](handoff/ipados-remote-test-195/README.md)。

[可复制的接收端Codex提示词](CODEX_IPAD_DEVICE_PROMPT.md)须由用户在接收Mac的会话中发送才构成该会话的设备授权。
本文件或提示词存放在仓库中不构成授权，原开发主机的设备/模拟器禁令继续有效。

## 版本和存档

本次App写入 **Save104**。194之前的RC1/RC2/RC3二进制不能读取新写的104存档。
先备份旧App数据和全部旧存档，记录文件摘要；旧档只用副本，新保存使用未占用槽位。
回退旧App时同时使用保留的旧格式存档，不覆盖原件。Windows/MMMerge原始存档不属于本次兼容承诺。

194前置源码为 `47776561`；195源码以本次发布的 `VERSION.json` 中 `source_head` 为准。
195最终提交、运行资源基线、未签名iOS附件与验收记录见195交付字段；不得以分支最新HEAD或旧RC3快照替代。
190–194已有Mac受影响检查和通用iOS构建证据。195原生native001通过（72.058秒），core导入72键通过；
独立启动后的主菜单读档页/建队预览首次通过（3.882秒、15帧、2图已查看）。
已看到自动存档/快速保存/存档99、中文星期/月和四项精通；没有载入游戏会话、创建队伍或推进战役。
195通用未签名iOS build-ios-001通过（54.982秒、exit 0），没有目标设备运行。
这些证据不等于新版目标iPad通过，也不证明完整自然通关。

## 完整资源来自本次同一发布

从本次195发布下载全部资源：`engine.zip`、`conversion.json`、`SHA256SUMS`，以及
`mm6.zip`、`mm7.zip`、`mm8.zip`、`mmmerge.zip`。全部附件来自同一tag `ipados-ui-function-195-20261029`。
195完整本地基线为 `local-data/validation/ui-function-completion-195/runtime-001`：继承194完整包，
保留194新增的303张原版休息动画图。194→195仅把UI文字表更新为72键，Save104和四个world相对194不变。
四个world实际继承 `local-data/validation/device-independent-completion-190/runtime-003`；与RC2相比，
MM6/MM7/MM8已增加190脚本，三个ZIP均已改变，只有mmmerge字节相同。本次仍从195发布下载全部四个world并校验，
不要复用RC2的MM6/MM7/MM8。不混用188、RC3或RC2旧engine；不解压运行ZIP，不把源码ZIP或零散补丁当作完整运行包。

在接收端一个新的可写目录中执行：

```sh
mkdir -p assets/worlds
gh release download ipados-ui-function-195-20261029 --repo Mavskero/openyamm \
  --dir assets --pattern engine.zip --pattern conversion.json --pattern SHA256SUMS
gh release download ipados-ui-function-195-20261029 --repo Mavskero/openyamm \
  --dir assets/worlds --pattern mm6.zip --pattern mm7.zip --pattern mm8.zip --pattern mmmerge.zip
(cd assets && shasum -a 256 -c SHA256SUMS)
```

清单使用 `worlds/mm6.zip` 等相对路径，应在 `assets/` 中校验全部资源。
遇到同名文件先核对版本，不加覆盖下载参数。App附件按195记录中的实际名称另行下载并核对其摘要。

```text
assets/
  engine.zip
  conversion.json
  SHA256SUMS
  worlds/
    mm6.zip
    mm7.zip
    mm8.zip
    mmmerge.zip
```

把整个新 `assets` 文件夹传到iPad“文件”可访问的位置，在App启动页选择整个资源目录，完成复制后开始游戏。
导入到App自己的 `Application Support/GameImports/<id>`，资源导入不自动导入存档。
旧资源目录保持原样，供定位问题和回退使用。

旧OpenYAMM存档先在接收端备份，再确认App容器实际的 `Library/Application Support/saves` 路径。
菜单使用 `save1.oysav` 等槽位文件，只把副本放入未占用槽位；普通Documents共享不会自动放到游戏读取目录。
请使用发布记录指定的195App和195engine作为一组，不能只换文字表或只更新App。

## 新Mac源码与构建

未签名iOS App不能直接安装。接收Mac需用有效Team签名；可按本机签名环境重签附件，
或从交付字段指定的同一源码提交构建设备Release。先确认旧App的数据备份及bundle ID。
默认bundle ID为 `org.openyamm.ipados`；不要通过卸载、清空容器或随意改ID来绕过签名问题。

新机器使用实际目录和全新构建缓存，不复制旧主机的CMakeCache、build、DerivedData、`_deps`、
`external-storage/env.sh`或NVMe路径别名。旧主机路径仅是历史证据来源，不应在新Mac重建同名目录。
完整Xcode、iPhoneOS SDK、Git、CMake、Ninja/make和Python是构建前提；首次依赖下载时间另计。

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

核对195交付字段中的源码提交，再在独立调试分支工作。已有仓库或调试分支先检查未提交修改和分歧，
保留现状后合并需要的195改动，不reset/clean。主代理独占构建目录和设备。

先检查 `xcode-select -p` 与 `xcrun --sdk iphoneos --show-sdk-path`，使用接收Mac实际的Xcode和Team：

```sh
cmake -S . -B build-apple -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DOPENYAMM_FETCH_DEPENDENCIES_ONLY=ON \
  -DOPENYAMM_BUILD_TESTS=OFF
BX_DIR="$PWD/build-apple/_deps/bx-src" \
  BIMG_DIR="$PWD/build-apple/_deps/bimg-src" \
  make -C build-apple/_deps/bgfx-src shaderc -j3

export OPENYAMM_DEVELOPMENT_TEAM='YOUR_ACTUAL_TEAM_ID'
scripts/configure_ipados_xcode.sh
export OPENYAMM_DEVICE_ID='ACTUAL_DEVICE_IDENTIFIER'
export OPENYAMM_DEVICE_DERIVED_DATA="$PWD/build-device-deriveddata-195"
xcodebuild -project build-ios-xcode/OpenYAMM.xcodeproj \
  -scheme openyamm_ipados -configuration Release \
  -destination "id=$OPENYAMM_DEVICE_ID" \
  -derivedDataPath "$OPENYAMM_DEVICE_DERIVED_DATA" -jobs 3 \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
  CODE_SIGNING_ALLOWED=YES CODE_SIGN_STYLE=Automatic \
  DEVELOPMENT_TEAM="$OPENYAMM_DEVELOPMENT_TEAM" build
```

这些Team/设备标识是需替换的本机值。设备构建显式使用 `CODE_SIGNING_ALLOWED=YES`，覆盖未签名构建设置。
从本次构建输出确认真实App路径，用Xcode Run或本机支持的devicectl保留数据地更新安装。
签名密钥、证书、令牌、provisioning文件和用户存档不进入Git或公开交付物。

## 受影响真机检查

先读[已有接收端记录](handoff/ipados-device-20261028/README.md)，跳过有匹配版本证据的已通过步骤。
本次紧凑检查估计 **45–90分钟**；首次下载/构建、人工登录/信任/解锁及失败定位时间另计。
详细操作按[195远程测试表](handoff/ipados-remote-test-195/README.md)执行，不重打旧核心路线或自然战役。

优先确认新App/完整资源、中文触控与键盘、卷宗/问题输入、角色/商店/银行面板、休息、普通旅行，
最后在新槽写Save104并退出App后重新读取一次。没有可达场景的项目记录未验，不为检查重打VARN、
雷格那潜艇、结局或三大陆旅行。失败旅行回滚只在实际遇到加载失败时核对，不破坏正常资源制造故障。
187仅改Mac Cocoa输入，186仅补Mac潜艇证据，均不增加iPad回放任务。

原始设备日志、截图、真实存档与各项通过/失败/跳过理由保存在接收端新的验证目录。
发现问题先区分产品缺陷、签名/设备环境和自动操作原因，修完只复查受影响步骤。
普通短流程、源码检查、通用iOS构建及受控夹具均不能写成完整自然通关。

历史[RC3交付](handoff/third-playable-candidate-189/README.md)、
[188资源](handoff/loadscreen-parchment-188/README.md)及190–194各批记录继续保留，
其中“当时未发布”的描述是历史事实，不代表本次195发布已经完成或禁止本次用户授权的远程测试。
