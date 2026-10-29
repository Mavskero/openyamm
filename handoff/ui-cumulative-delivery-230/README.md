# OpenYAMM iPadOS 229：累计源码、App和完整资源交付

本次230交付工作集中发布229生产代码，包含216–229新增修复、原214累计内容及195接收端修复。
固定tag为 `ipados-ui-cumulative-229-20261030`，见[发布页面][release]；
仓库为 [Mavskero/openyamm](https://github.com/Mavskero/openyamm)，开发分支为 `codex/ipados`。
固定214仍是独立Save104发布，195和RC1/RC2/RC3及其附件、原始输入和失败记录均保留。
本次提供配套的Save105程序和完整六文件资源，不要求接收端继续手工叠加历史overlay。

## 身份和附件

| 项目 | 本次身份 |
| --- | --- |
| 完整源码 | `VERSION.json → source_head`，`source.tar.gz`，逐文件清单 `SOURCE-MANIFEST.json` |
| 生产代码 | `production_commit = 8cdae9215ef7bb52e8fe8f6e83bf16641280fd0e`（229） |
| 未签名iOS App | `OpenYAMM-229-iPadOS-unsigned.zip`；摘要和已有229构建来源见 `VERSION.json → ios` |
| 完整资源基线 | `local-data/validation/equipment-feedback-227/runtime-001` |
| UI／战斗／导航 | UiText214项：146原源／68项目译文；Combat223共7项；MM8导航222 |
| 存档 | 写Save105，可读Save104；固定214及旧RC不能读取新写105 |
| 校验和回执 | `ARTIFACT-SHA256SUMS`、`SHA256SUMS`、`delivery.json` |
| 本地交付目录 | `releases/ipados-ui-cumulative-229-20261030` |

最终源码身份从本次VERSION读取，包含文档的 `source_head` 与游戏 `production_commit` 分别记录；
继续开发或回执提交不改变固定附件身份。源码文件数、App和附件摘要以本次清单为准，发布核对结果见delivery。
关键说明附件为 `README-IPAD-229.md`、`IPADOS_HANDOFF.md` 和 `CODEX_IPAD_DEVICE_PROMPT.md`。
完整签名、下载、校验、导入和可选源码构建步骤见 `IPADOS_HANDOFF.md`，脚本及项目路径保持原样。

## 累计内容和已有证据

原196–214的中文阅读、跨地图Notes、角色技能手势、奖项滚动、失败保存草稿、100槽分页／IME、
亮度预览、彩色光／Tinting、音量／室内脚步、设置返回、房屋退出、键位优先级、经典转向和Yell均保留。
195接收端的触摸检查入口、空槽名字和保存分页修复已并入；历史反馈dcae3d56合入14137bad，
这两个历史标识不替代本次VERSION身份。

| 范围 | 新增行为 | 验收边界 |
| --- | --- | --- |
| 216–219 | 技能精通后缀、教师报价／成功／注记、三项属性要求、技能页表头、选物提示 | 219真实SDL专家长剑学习，两图已查看；三项属性拒绝无像素验收 |
| 220–221 | NPC确认和职业字幕、满随从原文、服刑、旅行确认／成本、App析构顺序 | 源码／导入／构建；旅行资源缺口由222关闭，221失败保留 |
| 222 | 原生MM8导航后叠Merge、named start／高度及Save105选择器 | 一条受控无GUI事务、真实104读取和105聚焦往返 |
| 223–224 | 施法、法力不足、闪避、具体失败原因及iPad目标操作说明 | 源码／导入／构建与既有字表兼容检查；无新增实际触控验收 |
| 225–227 | 鉴定修理、拾取搜尸偷窃、消耗品／书籍／奖励、危险地形、装备放置反馈 | 源码／导入／构建；修复写回被拒的成功误报及陈旧提示 |
| 228–229 | 原源左臂姿态、锚点和前后遮挡由两条渲染路径共用 | 原角色法杖、空装备和盾牌共三图；其他组合／fallback为源码／构建范围 |

逐批事实、检查结果和保留失败位于源码 `handoff/` 下对应216–229目录。
219只设置原NPC805／house1285上下文，未改原normal132队伍、资格、金币、地图或任务；
15.921秒实际SDL学习后9067→7067金币、长剑4普通→4专家，中文报价、成功和“长剑 专家”可见。
这不等于自然步行进入教师，没有写档，原输入和副本摘要未变。

222唯一生产移动→确认→提交案例Out02东→Out06，10.223秒，无GUI；5天及既有5份食物结算，
原West Start(-22080,-5776)、yaw0、Z480。其余路线及四条缺marker分支没有追加实际路线矩阵。
228／229各一次13.725秒／14.164秒Mac显示，使用原Save85已有装备、正常头像输入、gameplay delta0，
原件和副本摘要不变，没有写档；三张图已查看。最终229原生5.631秒、unsigned iOS10.106407秒构建通过，
两个生产源码摘要与构建一致。230集中交付沿用这些已有产物，不新增构建、App、用例或设备回放。

195接收端已完成普通短流程、旅行、中文Save104新槽保存、前后台恢复、独立进程读档和音画确认，
见源码 `handoff/ipados-device-195-20261029/README.md`。原始证据仍在接收Mac，仓库摘要索引不能替代原图。
195通过不推定229设备行为已验；完整自然通关、全部UI、长期稳定性及本次目标iPad验收仍未证明。
整体约85%为低置信度工程估算，完整目标继续进行。

## 完整六文件资源

本tag同时提供 `engine.zip`、`conversion.json`、`mm6.zip`、`mm7.zip`、`mm8.zip`、`mmmerge.zip`。
engine相对214累计更新ui_text、combat_text、map_navigation，conversion包含对应导入记录；
其余engine内容及原有303张休息动画、滑条素材保留。四个world与214／195同文件，仍随本次完整提供；
MM6/MM7/MM8已在190改变，不能使用RC2旧world替代。

先在同一tag的全附件下载目录校验 `ARTIFACT-SHA256SUMS`。GitHub把world附件平铺下载，
须按以下结构放入 `worlds/`，再在 `assets-229/` 内执行 `shasum -a 256 -c SHA256SUMS`：

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

使用新的空目录，保留旧包和旧校验表。五个运行ZIP保持压缩，选择整个资源目录导入，
不混用旧engine、源码素材或开发夹具。资源导入与游戏存档槽位分别处理。
源码包包含完整项目代码、配置、交接和构建所需源资源；第三方依赖按固定版本获取。
源码包不附Git历史、旧构建缓存、签名材料、用户存档或全部原始游戏素材。

## 数据、签名和接收边界

更新前备份当前App全部数据和工作存档、记录摘要，保留接收端已有10个旧存档原件。
旧档只用副本，新保存用未占槽位和新名称。当前可读104但写105；214／旧RC不能读取新105，
回退须使用保留的对应旧格式副本。Windows/MMMerge原始存档兼容性未证明。

未签名App由接收Mac使用自己的有效Team签名，保持 `org.openyamm.ipados`，保留数据更新；
不卸载、不清容器、不重置设备。新机使用自己的目录和全新build／DerivedData，不复制旧缓存、env.sh或NVMe别名。
源码设备构建显式给出 `CODE_SIGNING_ALLOWED=YES`，覆盖configure脚本的NO；已有工作先比较并合并，不reset／clean。

只有用户把 `CODEX_IPAD_DEVICE_PROMPT.md` 提示词发到接收会话，才授权该会话操作目标iPad。
文档、附件和发布本身不授权设备操作；原开发主机的设备及模拟器禁令保持。
195已通过短流程不重打，不新增45–90分钟矩阵或回放旧战役、潜艇和结局。
长NPC分页、问题中文输入、目标卷轴、昏睡队员用药、银行金额、商店拖离取消等条件项，
只在正常续玩自然具备场景时补，没场景标未验，不造物品、任务位或故障资源。
技能详情展开截图缺口继续保留；未验和跳过不计通过，新失败保留原始证据并只做最短受影响复查。
获授权接收端的新证据使用 `local-data/device-validation/cumulative-229-<实际日期>/`；设备无关开发持续进行。

[release]: https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-cumulative-229-20261030
