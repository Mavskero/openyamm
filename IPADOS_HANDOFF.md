# OpenYAMM iPadOS：229 累计交付

更新：2026-10-30。本次集中交付包含固定214之后的216–229修复，以及214已有的196–214累计内容和195接收端修复。
仓库为 [Mavskero/openyamm](https://github.com/Mavskero/openyamm)，开发分支为 `codex/ipados`；
本次 tag 为 `ipados-ui-cumulative-229-20261030`，见[发布页面][release]。
这是一次源码、App和完整资源的配套交付，不增加设备检查矩阵，也不构成继续开发的停止条件。
固定214、195和RC1/RC2/RC3的发布及原始证据均保留。

## 交付身份

| 项目 | 本次身份 |
| --- | --- |
| 完整源码快照 | 发布附件 `VERSION.json → source_head`；逐文件身份见 `SOURCE-MANIFEST.json` |
| 生产代码提交 | `production_commit = 8cdae9215ef7bb52e8fe8f6e83bf16641280fd0e`（229） |
| 未签名App | `OpenYAMM-229-iPadOS-unsigned.zip`；摘要和构建来源见 `VERSION.json → ios` |
| 完整运行资源 | `local-data/validation/equipment-feedback-227/runtime-001`，本次六文件齐发 |
| 文字和导航 | UiText共214项（146项原源、68项项目译文），Combat223、MM8导航222 |
| 存档 | 当前写Save105，可读Save104；旧214及RC不能读取新写105 |
| 附件和发布核对 | `ARTIFACT-SHA256SUMS`、`SHA256SUMS`、`delivery.json` |
| 本地交付目录 | `releases/ipados-ui-cumulative-229-20261030` |

`source_head` 标识包含交接文档的完整源码快照，`production_commit` 标识对应的游戏生产代码。
最终源码提交、文件数、App摘要及附件大小从本次VERSION和清单读取，不从继续更新的分支HEAD推断。
发布状态和远端核对结果以 `delivery.json` 为准。230交付本身未新增构建、App运行、用例或设备操作，沿用229已有产物。

## 累计改动和证据

214已有的累计内容全部保留：

- 中文和阅读：建队标点换行、键位两页和日志类别、精通及法术学派名称、跨地图Notes。
- 角色和保存：技能手势绑定稳定队员身份、奖项布局与滚动命中统一、失败保存保留名字草稿、
  真实空槽初始化、100个手动槽和十行滚动、可见行软键盘定位。
- 设置和画面：亮度50–200%及世界预览、彩色光和怪物Tinting、音量拖动及室内脚步开关、
  设置返回关闭链、室内外最终房屋退出的FlipOnExit。
- 键位和动作：冲突绑定交换、显式绑定优先、经典16/32转向、Yell及500毫秒延迟／67毫秒长按重复和焦点保护。
- 195接收端反馈整合：建队和Standard角色页的触摸检查入口、空槽名字、保存分页与IME区域。
  历史反馈 `dcae3d56` 已在 `14137bad` 合并，这两个历史标识不替代本次VERSION身份。

216–229的新增内容和验收范围如下；每批的原始失败、修复过程和具体限制见相应handoff。

| 批次 | 本次包含的行为 | 已有证据范围 |
| --- | --- | --- |
| [216][h216]、[217][h217] | 精通后缀、教师报价／成功／注记、三项50属性要求、选物提示 | 源码／导入／构建；三项属性拒绝无像素验收 |
| [218][h218]、[219][h219] | 技能页八个中文表头，首次实际长剑专家训练UI | 218一图；219真实SDL报价→学习→技能页，两图已查看 |
| [220][h220]、[221][h221] | NPC字幕、满随从原文、服刑入口、旅行确认／成本、App析构顺序 | 源码／构建；221旅行资源缺口在222关闭 |
| [222][h222] | MM8原生导航及Merge覆盖、named start／高度、Save105选择器 | 一条受控无GUI旅行事务、真实104读取和105聚焦往返 |
| [223][h223]、[224][h224] | 施法／闪避和具体失败中文、iPad轻点目标／菜单取消说明 | 源码／导入／构建及既有字表兼容检查，无新增实际触控验收 |
| [225][h225]、[226][h226] | 鉴定修理、拾取偷窃、消耗品／书籍／奖励、卷轴失败、燃烧／溺水 | 源码／导入／构建；地面写回被拒不再误报成功 |
| [227][h227] | 装备／放置使用本次失败原因，修复显示上次成功诊断的问题 | 源码／导入／构建；原库存、装备资格及事务保留 |
| [228][h228]、[229][h229] | 恢复原源左臂姿态、锚点和遮挡，两条渲染路径共用结果 | 原角色法杖、空装备／盾牌共三图；其他组合及fallback为源码／构建范围 |

219只设置原NPC805／房屋1285对话上下文，原normal132队伍、技能、金钱、地图和任务保持。
点击原话题1515后，金币9067→7067，长剑4级普通→4级专家；中文报价、成功提示和“长剑 专家”可见。
运行15.921秒，不证明自然步行进入教师、全部教师或三项50属性拒绝。原输入和副本摘要未变，没有写游戏存档。

222认证原exe的28条非零MM8路线；只有Out02东→Out06经生产移动、确认、换图事务检查通过，
费用为5天及既有5份食物，West Start为(-22080,-5776)、yaw0、Z480。
该检查带诊断队伍和坐标前提，10.223秒、无GUI；四条缺named marker路线保留原出发XY/yaw后投影目标地形，
这些分支接受源码范围，没有另做路线矩阵。221崩溃、资源缺口和222早期检查前提失败记录均保留。

228／229分别运行一次13.725秒和14.164秒的Mac Metal显示，合计三张图已查看；
直接使用原Save85已有装备，只用正常头像输入切换角色，gameplay delta0，原件和副本摘要不变，没有写档。
229最终原生构建5.631秒、通用未签名iOS构建10.106407秒通过，生产源码摘要与构建一致。
这些受控主机画面不代表自然续玩、全部装备组合或本次目标iPad验收。

[195接收端记录][device195]已报告普通短流程、旅行、中文Save104新槽保存、前后台恢复、独立进程读档及音画通过。
原始设备证据仍在接收Mac；仓库摘要索引不能替代原图，也不能推定229全部设备行为通过。
195已通过的短流程无需重打。完整自然通关、全部UI、长期稳定性和本次目标iPad验收仍未证明；
整体约85%只是低置信度工程估算，完整目标继续进行。

## 存档和数据保留

本次程序写入 **Save105**，已验证读取真实Save104；新增字段保留普通旅行到达意图。
固定214程序和旧RC不能读取新写105。回退214使用保留的104副本，回退更旧RC使用其可读的旧格式副本。
更新前备份当前App全部数据和工作存档、记录摘要，持续保留接收端已备份的10个旧存档原件。
旧档只用副本，新保存使用未占用槽位和新名称，不覆盖原件。Windows/MMMerge原始存档兼容性仍未证明。

保持bundle ID `org.openyamm.ipados`，由接收Mac使用自己的有效Team签名并保留数据地更新安装。
不要卸载旧App、清空容器、重置设备或随意更改bundle ID来解决签名问题。
资源与存档分别导入：资源复制到App的 `Application Support/GameImports/<id>`；
游戏槽位在App实际的 `Library/Application Support/saves`，普通Documents共享不会自动变成游戏槽位。

## 下载、校验和完整资源导入

本次附件包括 `OpenYAMM-229-iPadOS-unsigned.zip`、`source.tar.gz`、全部六运行文件、
`README-IPAD-229.md`、`IPADOS_HANDOFF.md`、`CODEX_IPAD_DEVICE_PROMPT.md`，以及
`VERSION.json`、`SOURCE-MANIFEST.json`、`ARTIFACT-SHA256SUMS`、`SHA256SUMS`、`delivery.json`。
未签名App需要接收Mac签名，不是可直接安装的已签名IPA。

完整资源采用227基线。相对固定214，engine累计更新ui_text、combat_text和map_navigation，conversion同步记录导入；
UiText共214项，Combat共7项，原有303张休息动画及滑条素材保留。四个world与214／195为同一文件，仍随本tag齐发。
MM6/MM7/MM8在190已改变，不能复用RC2旧world。228／229只改渲染代码，无需另补资源。

在新的下载目录获取同一tag的全部附件：

```sh
mkdir release-229
gh release download ipados-ui-cumulative-229-20261030 --repo Mavskero/openyamm --dir release-229
(cd release-229 && shasum -a 256 -c ARTIFACT-SHA256SUMS)
mkdir assets-229
mkdir assets-229/worlds
cp release-229/engine.zip release-229/conversion.json release-229/SHA256SUMS assets-229/
cp release-229/mm6.zip release-229/mm7.zip release-229/mm8.zip release-229/mmmerge.zip assets-229/worlds/
(cd assets-229 && shasum -a 256 -c SHA256SUMS)
```

只在前一步成功后继续；使用新的空目录，遇同名文件先核对，不加覆盖下载参数。
GitHub将四个world附件平铺下载，须移入 `worlds/` 后再校验保留该相对路径的 `SHA256SUMS`：

```text
assets-229/
  engine.zip
  conversion.json
  SHA256SUMS
  worlds/
    mm6.zip
    mm7.zip
    mm8.zip
    mmmerge.zip
```

五个运行ZIP保持压缩状态，将整个新 `assets-229` 目录交给App原生资源选择器，完成六文件复制后继续。
保留旧资源目录，不混入旧engine、开发夹具或源码素材。本次完整交付无需再手工叠加历史小型overlay。
源码包包含完整项目代码、配置、交接文档和构建所需源资源；游戏运行素材由上述六文件提供。
源码快照不包含Git历史、旧主机构建缓存、用户存档或全部原始游戏素材；依赖按固定版本联网获取。

## 接收Mac签名和可选源码构建

可以重签本次未签名App副本，也可以用本次源码快照构建设备Release；不要求为了交付再跑一套构建或游戏回放。
新机器使用自己的工作目录、Team和全新build／DerivedData；不要复制旧CMakeCache、`_deps`、
`external-storage/env.sh`、旧DerivedData或NVMe路径别名。
已有调试分支先比较与本次 `VERSION.json → source_head` 的差异，保留未提交工作后合并，不reset／clean。

使用源码附件时，先校验附件摘要，解包后在 `openyamm/` 内执行。完整Xcode、iPhoneOS SDK、Git、
CMake、Ninja／make和Python需由本机提供；首次下载和构建时间另计。

```sh
mkdir source-229
tar -xzf release-229/source.tar.gz -C source-229
cd source-229/openyamm
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
export OPENYAMM_DEVICE_DERIVED_DATA="$PWD/build-device-deriveddata-229"
xcodebuild -project build-ios-xcode/OpenYAMM.xcodeproj \
  -scheme openyamm_ipados -configuration Release \
  -destination "id=$OPENYAMM_DEVICE_ID" \
  -derivedDataPath "$OPENYAMM_DEVICE_DERIVED_DATA" -jobs 3 \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration \
  CODE_SIGNING_ALLOWED=YES CODE_SIGN_STYLE=Automatic \
  DEVELOPMENT_TEAM="$OPENYAMM_DEVELOPMENT_TEAM" build
```

Team和设备ID替换为接收本机值。`CODE_SIGNING_ALLOWED=YES` 必须显式给出，覆盖configure脚本的未签名设置。
从实际构建输出确认App路径，用Xcode或本机支持的devicectl保留数据地安装。
签名密钥、证书、令牌、provisioning文件及用户存档不进入公开交付物。

## 延续已有验收和授权边界

[接收端Codex提示词](CODEX_IPAD_DEVICE_PROMPT.md)只有由用户作为消息发送到接收Mac会话后，才授权该会话的设备操作。
保存文档或发布附件本身不构成授权；原开发主机继续禁止设备及模拟器操作。
本次交付不自动启动45–90分钟矩阵，不重跑195已通过短流程、潜艇、旧结局、三大陆战役或旅行／休息／冷启动全套。

长NPC分页、问题中文输入、目标卷轴、昏睡队员用药、银行金额、商店拖离取消等条件项，
只在正常续玩自然具备场景时补对应操作；没有场景继续标未验，不制造物品、任务位或故障资源。
角色技能详情展开像素证据不足的限制继续保留，列表或关闭状态不替代展开证据；未验和跳过不计通过。
新失败保留操作、截图、日志、版本及前后存档摘要，只做最短复现与受影响复查。

用户在接收端明确启动后，新的设备证据放入 `local-data/device-validation/cumulative-229-<实际日期>/`，
记录实际源码／App／资源身份、设备和签名信息、通过／失败／未验项。原始失败及10个旧存档原件持续保留。
主代理独占共享构建目录、集成和获授权接收端的设备操作；设备无关开发持续进行。

[release]: https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-cumulative-229-20261030
[device195]: handoff/ipados-device-195-20261029/README.md
[h216]: handoff/ui-function-completion-216/README.md
[h217]: handoff/ui-function-completion-217/README.md
[h218]: handoff/ui-function-completion-218/README.md
[h219]: handoff/sword-teacher-ui-219/README.md
[h220]: handoff/ui-function-completion-220/README.md
[h221]: handoff/ui-function-completion-221/README.md
[h222]: handoff/mm8-native-boundary-222/README.md
[h223]: handoff/spell-combat-ui-223/README.md
[h224]: handoff/spell-failure-ui-224/README.md
[h225]: handoff/spell-item-feedback-225/README.md
[h226]: handoff/consumable-feedback-226/README.md
[h227]: handoff/equipment-feedback-227/README.md
[h228]: handoff/paperdoll-staff-arm-228/README.md
[h229]: handoff/paperdoll-left-arm-229/README.md
