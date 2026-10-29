# OpenYAMM iPadOS 214：累计代码和完整资源交付

本次整合固定195发布之后196–214的修复，并纳入接收端195真机反馈的修复。
这是一次集中交付；固定195 tag和历史RC保持原样，不增加新的设备检查矩阵或开发停工门槛。

| 交付项 | 本次身份 |
| --- | --- |
| 发布tag | `ipados-ui-cumulative-214-20261029` |
| 源码提交 | 本发布 `VERSION.json → source_head` |
| 未签名iOS App | `OpenYAMM-214-iPadOS-unsigned.zip`；摘要见 `VERSION.json → ios` |
| 完整源码 | `source.tar.gz`；逐文件身份见 `SOURCE-MANIFEST.json` |
| 运行资源基线 | `local-data/validation/ui-function-completion-207/runtime-001`，六文件齐发 |
| 存档格式 | Save104，与195相同 |
| 附件校验与发布记录 | `ARTIFACT-SHA256SUMS`、`SHA256SUMS`、`delivery.json` |
| 本地产物目录 | `releases/ipados-ui-cumulative-214-20261029` |

最终源码hash、App hash和附件身份由打包程序写入VERSION与清单；不要从继续开发中的分支HEAD推断。
仓库为 [Mavskero/openyamm](https://github.com/Mavskero/openyamm)，
本次[发布页面](https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-cumulative-214-20261029)提供完整附件。
签名、导入与可选源码构建见随包 `IPADOS_HANDOFF.md`；可复制的接收端提示词见 `CODEX_IPAD_DEVICE_PROMPT.md`。

## 累计改动

| 类别 | 本次行为 |
| --- | --- |
| 中文和阅读 | 建队中文标点换行、键位两页和日志类别中文、法术详情学派名复用技能表；跨地图Notes继续可读 |
| 角色与保存 | 技能按下绑定稳定队员身份，切换取消旧手势；奖项实际行高与滚动命中统一；失败保存保留草稿，真实空槽保持空名；100手动槽、十行滚动和可见行IME定位 |
| 设置与画面 | 可拖动亮度与世界预览，彩色光和怪物Tinting接线，音量短触摸拖动及单次提交，室内脚步关闭即停；返回游戏完成关闭链，室内外最终房屋退出统一FlipOnExit |
| 键位与动作 | 单一冲突绑定交换，显式绑定优先于固定快捷键；16/32键盘转向与帧率无关长按；Yell包含正常玩法门控、现有语音/头像反应、附近角色反应及500/67毫秒长按处理 |
| 接收端反馈整合 | 建队及Standard角色页可见触摸检查入口、空槽名字和保存分页；反馈dcae3d56已合入14137bad，两个历史标识不替代本次VERSION身份 |

Yell附近角色反应和经典转向均沿用当前运动/状态约束及已声明的项目适配，不声称完整原版逐行为复刻。
本次没有更改Save104格式。

## 完整运行包

必须使用本发布的一整组六文件：`engine.zip`、`conversion.json`，以及四个world ZIP。
engine和conversion相对195已更新；共享UI文字表为112键，原有303张休息动画和后来补齐的滑条素材保留。
四个world与195基线同文件，但仍随新tag一并提供。MM6/MM7/MM8早在190已改变，不能拿RC2旧包替代。

```text
assets-214/
  engine.zip
  conversion.json
  SHA256SUMS
  worlds/
    mm6.zip
    mm7.zip
    mm8.zip
    mmmerge.zip
```

先在完整附件下载目录校验 `ARTIFACT-SHA256SUMS`；再按上面目录组装六文件，
在 `assets-214/` 内执行 `shasum -a 256 -c SHA256SUMS`，因为清单保留 `worlds/` 相对路径。
运行ZIP不要解压；向App选择整个新资源目录，保留旧资源目录，不混用旧engine、源码素材或开发夹具。
资源导入不会自动把存档放进游戏槽位。

源码包包含项目代码、配置、文档和构建所需源资源；运行素材由六文件提供，第三方依赖按固定版本获取。
不附旧build、CMakeCache、DerivedData、签名材料或用户存档。

## 证据与仍保留的限制

接收端195已经完成普通短流程、旅行一次结算、中文新槽Save104、前后台恢复及独立进程重启读档，
并复验触摸入口、空槽和第11槽保存，见仓库 `handoff/ipados-device-195-20261029/README.md`。
该设备证据属于195接收端修复版；原始文件仍在接收Mac，摘要索引不等于原图已经上传。
它不自动证明214所有设备行为，也不应要求再重打一次同套短流程。

本次原生和通用未签名iOS构建通过。213一次有界实际Yell检查通过：13.522秒、180正常续玩帧及保存，
2图已review；214长按500毫秒延迟/67毫秒重复及焦点处理通过0.833秒输入检查。
其他项目的受影响主机检查及源码/构建边界见各handoff；未新增设备或自然战役回放。
完整自然通关、全部UI、长期稳定性及Windows/MMMerge存档兼容性仍未证明。

这次交付不自动派45–90分钟矩阵。长NPC分页、问题中文输入、目标卷轴、昏睡队员用药、
银行金额、商店拖离取消等未验项，仅在手头自然有场景时补对应操作；没有场景继续标未验。
角色技能详情展开像素证据不足的限制也继续保留。不造物品、强制任务位或破坏资源制造验收场景，
不重跑旧核心路线、潜艇、结局或三大陆战役。新失败只做最短复现与受影响复查。

## 接收端数据与签名

更新前备份当前数据、全部工作档及摘要；保留已经备份的10个旧存档原件，旧档只用副本，
新保存只用未占槽位。Save104与195相同，但旧RC1/RC2/RC3不能读取新写的104档。
未签名App需接收Mac用自己的有效Team签名；保留 `org.openyamm.ipados` 的数据身份，
不卸载旧App、不清空容器或重置设备。

新机器使用自己的工作目录和新的build/DerivedData，不复制旧缓存、env.sh或NVMe别名。
源码设备构建显式 `CODE_SIGNING_ALLOWED=YES`，覆盖configure脚本的未签名设置；完整步骤见随包交接文档。
用户在接收端明确发送提示词才授权该会话的设备操作，文档保存或本次发布本身都不授权其他会话。
原开发主机的设备/模拟器限制不变；设备无关开发持续进行。
