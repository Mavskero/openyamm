# 240–241：实际Mac粘贴与退出确认字幕

240使用239源码`01b5563b92868d889a5476a5448d955450b33557`的现有原生程序和完整235/runtime-002资源，
在隔离Mac工作目录进行正常UI操作。正常新建→恩洛斯→建队，名字栏通过macOS原生CUA粘贴“离线输入240”，
文字可见，Return结束编辑，Cancel离开草稿。随后从正常Load菜单读取本地Save10副本，ESC→保存菜单，
单击未激活字段时粘贴被忽略；双击空槽1进入编辑后粘贴“离线存档240”，文字只显示一次，Escape取消。
这是两处普通非组合粘贴的实际观察，没有操作IME preedit或UIKit，不把它计为237–239全部输入分支验收。

240首次Quit实际出现英文`Press Quit again to exit.`，第二次正常退出，由此触发241唯一生产修复：
菜单退出确认改用typed当前语言字幕，映射原`GlobalTxt82`。来源分别是合法原资源
`Data/mmmerge.T.lod::global.txt[82]`的`Are you sure?  Click again to quit`，以及
`Data/10 LocZHCN.EnglishT.lod::global.txt[82]`的“你确定吗？再次单击可退出”。
二次确认、超时、输入与SourceAction规则保留；新增`ui.menu_quit_confirmation`，完整旧227表兼容且旧值不变。

241原生构建143.473秒、通用未签名iOS构建134.596秒、核心导入0.195秒通过，4个冻结源码摘要与构建输入一致。
主代理随后正常Load副本→ESC菜单→首次Quit，内联CUA画面显示完整中文、无裁切；二次退出后，
CUA取AX状态可能重新拉起程序至冷菜单，该段不计冷启动验收。随后从冷菜单正常退出，`ps`确认PID60306不存在。
两批画面均仅由主代理通过内联CUA查看，没有导出／裁切图片或新增截图框架。
英文Quit显示、完整旧227迁移及其他继承分支仍仅计源码／构建范围，见[受限结果](scoped-result.json)及[源码摘要](source-sha256.json)。

两批均没有写游戏存档。隔离目录最终只有原Save10副本，原件与副本仍为原SHA，没有新档；
没有制造队伍／任务／金钱／位姿夹具或传送，也没有重放自然战役。240可能有正常模拟时间及一次未指定目标的鼠标攻击，
不声称gameplay delta0。240没有新增构建，两批均没有新增单元用例、设备／模拟器操作或Release。

当前完整资源在工作区外层`local-data/validation/quit-confirmation-ui-241/runtime-002/`。
相对235仅`data_tables/ui_text.txt`和conversion记录改变；UiText228＝157项原源＋71项项目译文，
其他17398个engine成员保留，四个world与235相同inode，继承既有已核对摘要、没有重读大文件。Save106不变，
真实旧105／104读取证据沿用236；固定229／235程序不能读取106，回退使用保留的105或更旧可读副本。

本目录[241的engine覆盖包](runtime-engine-overlay-241.zip)只有`data_tables/ui_text.txt`，不是完整runtime、App或新Release，摘要见[增量清单](overlay-manifest.json)。
使用当前匹配源码，以固定229完整六文件为基线，依次应用[231–232覆盖包](../ipados-device-231-232-20261031/runtime-engine-overlay.zip)、
[234覆盖包](../offline-function-completion-234/runtime-engine-overlay-234.zip)、
[235覆盖包](../offline-input-completion-235/runtime-engine-overlay-235.zip)，最后应用本目录241覆盖包；保留旧资源与附件。
完整241的conversion与六文件身份另按[六文件摘要](runtime-files.json)核对，不能把单成员覆盖包当作完整导入资源。

临时启动的裸CLI未注册CUA、外置位置打开阻塞、临时包缺shader，以及资源组装的ZipInfo元数据比较误报和系统Python的hashlib兼容性失败均保留。
成功启动只修正临时打包环境；资源重新开包确认只改ui_text后复用已组装payload，没有重跑已通过构建。这些环境／记录问题不计新增产品修复。

当前物理调试仍按用户要求暂停，子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git。
固定229／214／195／RC1–3和全部原存档继续保留；本次Mac副本观察不扩大232时点的真机存档保留结论。
实际preedit、UIKit回调／硬件键盘时序、目标iPad触控、银行／商店交易、长问题和完整自然通关仍未证明。
242正在认证Sunray怪物弹道支持缺口，尚未实现、不计通过，不能宣称全部offline已完成。
整体约88%只是低置信度工程判断，不是覆盖率或自然通关比例。公开交接不包含私有容器、存档内容、设备标识或原始控制台。
