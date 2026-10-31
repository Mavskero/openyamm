# OpenYAMM iPadOS：离线继续开发与真机接手

更新：2026-10-31。用户最新指令是“真机调试先中断，继续没有真机情况下的工作，subagent切为6.1 sol max”。
**当前真机调试暂停**：不连接、读取、更新安装或操作设备，不使用模拟器，不重复询问设备授权。
继续具体的设备无关修复；主代理独占构建、App运行、集成与Git，子代理按 `gpt-6.1-sol/max` 做有界源码或文档工作。
历史goal-tool的blocked记录不是本次离线继续开发的停止条件。

仓库为 [Mavskero/openyamm](https://github.com/Mavskero/openyamm)，开发分支为 `codex/ipados`。
`8989cf96dcd1d5c23de2d5da2a58030c92e3291d`记录231–232源码和脱敏受限真机证据；
后续233–253集成身份按当前Git提交及各handoff逐文件摘要核对，不改写固定发布`ipados-ui-cumulative-229-20261030`。
242已推送并核对`673040d434d17207c0ac02f37763f271076c7b1f`；243为`ACCEPTED_OFFLINE_MAC_SCOPED`，自然Mac银行及两项受影响修复已受限通过。
244同为`ACCEPTED_OFFLINE_MAC_SCOPED`，自然Mac商店及Cocoa拖离误购买修复已受限通过，不作iPad触控验收。
245自然Mac generated NPC与职业说明修复亦为`ACCEPTED_OFFLINE_MAC_SCOPED`；修后职业说明实际复查的是另一NPC伯尼，未声称威尔勒德职业16说明复验。
246随从字幕与247武器随从加成已按各自源码／构建及既有组件范围受限通过，由主代理一次累计源码提交；没有App或设备验收。
其累计基线`cb084d3a06c8085939ff60c1b0b00d8ed3125271`已推送并核对；248–249为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`，9份最终源码摘要已核对。
248–249提交`69c2fe94a574d0dbf34e06b5113670ccea1053bb`已推送，远端HEAD及提交内9文件摘要核对通过；250为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`，7文件最终摘要全部匹配。
250提交`1bfefd6ff5fa1b970922a8e3e7072bd19503b25b`已推送，远端及提交内7份摘要核对通过；251–253为`ACCEPTED_OFFLINE_MAC_SCOPED`，5源码摘要匹配，实际仅010首个装备画面受限通过。
固定229、214、195、RC1/RC2/RC3及其原始附件继续保留；231–253没有发布新Release。

## 当前状态与继续入口

| 工作 | 当前证据与边界 |
| --- | --- |
| [231–232真机短流程][device232] | 受限通过：真实Save104读取，UI穿戴护甲84／主手武器66，仅新增Save12／105，并在两次保留数据更新后的独立进程读取。中文Stats／技能表头、真实重叠双指检查、普通NPC新闻592、卷宗2125三页阅读，以及232绿色警戒透明和触控中性说明已复核。完整边界见[受限结果][result232] |
| [233物品检查状态字幕][h233] | “破损物品”／“未鉴定”接原GlobalTxt32／232，UiText224→226。源码、原生／未签名iOS构建及导入通过；没有App或设备复验，不能计受影响像素或触摸通过 |
| House64的“Learn Skills” | 实际店内发现的英文字幕接回原GlobalTxt160，已包含在234原生／未签名iOS构建；没有像素复验 |
| [234离线功能修复](handoff/offline-function-completion-234/README.md) | 原随机NPC问候与接待规则、SDL有序编辑、商店拖离取消、银行容量边界已集成。原生／未签名iOS／导入、现有输入检查及两条既有NPC用例52断言通过；零新增用例、App运行、存档写入、设备或Release |
| [235触屏与问题输入](handoff/offline-input-completion-235/README.md) | 快速tap保留真实起点，商店检查全部本批细小轨迹并隔离手势；问题临时组合文字与typed“答案：”前缀集成。原生／未签名iOS／核心导入及现有输入检查通过。组合显示只计源码／构建，未做实际输入法、像素或设备验收；UiText227，Save105 |
| [236飞行逃跑方向](handoff/yell-flight-direction-236/README.md) | Yell固定飞行Z持续消费，取消通用目标高度覆盖，生命周期清理；原生／未签名iOS及一次聚焦codec／AI通过。当前开发写Save106，真实旧105／104只读通过；根与缓存双世界字段、坏值拒绝和原文件保持已核对，无App／设备或实际飞行像素验收 |
| [237组合文字与输入](handoff/marked-text-bridge-237/README.md) | UIKit持久桥接、保存／建队／银行独立preedit、有序提交与取消、控制台清理、建队／答案粘贴；原生／未签名iOS及一次现有输入检查通过，15文件摘要一致。零App／设备／存档写入／新Release，实际UIKit、像素及交易仍未验 |
| [238保存退出规则](handoff/save-escape-admission-238/README.md) | 保存窗口在组合输入判定后只发一次原Action113／Screen11，拒绝时保留窗口和草稿；原生／未签名iOS及一次受控hook输入检查通过，3文件摘要一致。未证明正式战役触发拒绝，无App／设备或新Release |
| [239粘贴来源](handoff/clipboard-edit-source-239/README.md) | 手动粘贴独立于原生提交，组合中连空剪贴板也保留候选和确认保护；普通过滤／原生提交保留。原生／未签名iOS及一次typed消费者检查通过，4文件摘要一致。239当批没有实际剪贴板检查，240另补两处Mac普通粘贴；实际IME／交易仍未验 |
| [240实际Mac输入](handoff/offline-ui-input-240-241/README.md) | 正常建队名字粘贴“离线输入240”→Return→Cancel；正常Load本地Save10副本→ESC菜单保存→双击空槽1→粘贴“离线存档240”显示一次→Escape取消。使用macOS原生CUA粘贴，没有IME preedit或UIKit输入；零存档写入，最终只有Save10副本且原SHA保持。首次Quit英文提示成为241具体修复依据 |
| [241退出确认字幕](handoff/offline-ui-input-240-241/README.md) | 唯一生产修复是typed当前语言Quit确认接原GlobalTxt82，保留完整旧227表。原生143.473秒／未签名iOS134.596秒／核心导入0.195秒通过，4源码摘要一致；实际正常Load副本→ESC→首次Quit中文完整无裁切，随后二次退出。UiText228＝157原源＋71项目，Save106不变 |
| [242怪物Sunray弹道](handoff/monster-sunray-242/README.md) | 原脚本Paralyze81→Sunray87已认证，补87名称支持与接触后伤害掷骰，保留Light抗性和既有分支。原生136.936秒／未签名iOS132.004秒通过；一个既有组件case0.188秒、79断言（15新增，零新case，其他28跳过），5生产文件＋1既有测试文件共6摘要一致。只计源码／构建／组件范围，没有实际Sunray遭遇、App运行、存档读写、设备操作、资源变更或新Release |
| [243自然Mac银行及受影响修复](handoff/natural-mac-bank-243/README.md) | 本地Save10副本正常步行至MM6 House297，17:34闭门；正常休息至Day2 09:00入行，粘贴17／Return实存、7／Return实取，最终gold190／bank10。仅新隔离槽1“离线银行243”写Save106；修后正常读该档，默认短按Space入银行显示余额10，正常休息后短按E显示完整中文闭门提示。原生／未签名iOS／既有输入检查通过，4冻结源码摘要一致；修后零写档，旧原件、副本及新槽摘要均未变 |
| [244自然Mac商店及Cocoa拖离修复](handoff/natural-mac-shop-244/README.md) | 正常读隔离槽1／106并步行进入MM6 House62；初次拖离误购买头盔90，随后正常出售17。修后同商品拖向右侧空白实际取消，gold190及商品保留；紧接正常点击购买190→100，中文购买状态及行囊新头盔可见。原生首次2.938982秒通过，2冻结摘要一致、3个SDL补丁反向检查通过；该批零单元运行／新case／写档，iOS沿用243 |
| [245自然Mac generated NPC及职业说明](handoff/natural-mac-generated-npc-245/README.md) | 威尔勒德首会动态问候2662、乞求2667成功，更多信息暴露职业说明原样%01／%17。修复复用既有formatter；修后从新槽2正常读档，同日再乞求2651正确拒绝，休息至Day3后遇另一NPC伯尼，更多信息完整两行无占位符；其“议会成员”普通News完整三行无裁切，仅计该话题。原生7.232242秒／未签名iOS10.339974秒通过，1冻结源码摘要一致；本批仅正常新槽2写档一次、修后零写档，零单元运行／新case；008已正常退出并收集 |
| [246随从反馈字幕](handoff/follower-feedback-246/README.md) | 普通随从反馈接typed当前UI，主动能力反馈复用原GlobalTxt569／561／140；UiText233＝157原源＋76项目，旧228值及完整表兼容保留。原生114.342秒／未签名iOS107.300秒／导入0.191秒／组装6.791秒通过，4冻结摘要一致。零App／单元运行／新case／写档，成功加入／离开消息可见性未证明 |
| [247武器随从加成](handoff/weapon-follower-bonus-247/README.md) | 原脚本七武器加成接入实际战斗读取，以独立瞬态表保留stored技能、精通、装备资格及Save106。原生166.684508秒／未签名iOS171.877579秒通过，5生产＋1既有测试文件共6摘要匹配；一个既有case2.029935秒、27／27断言通过，14个其他case跳过，含Mace4→7命中＋3及未学Bow有效3／Normal。零新case／App／写档／资源变更，不作自然雇佣或实战验收 |
| [248–249被动随从加成](handoff/passive-follower-completion-248-249/README.md) | 23项技能projection接经验／价格／utility／护甲／施法，Luck和四抗接实际承伤并避免Event双加；两名合法职业37各＋20、合计四抗＋40。全量原生187.762773秒／iOS194.328930秒及单cpp增量3.467936秒／6.777035秒通过，7生产＋2既有测试共9摘要核对。首轮3个既有case中spell／defense通过，utility价格夹具修正后单独36／36断言通过；原两项未重跑，零新case／App／写档／资源变更 |
| [250随从步行减时与ActualLuck](handoff/follower-travel-luck-250/README.md) | ActualLuck脚本getter补实际值，步行quote／plan共享职业5／6／7／44减时，仅处理正Days的.odm→.odm。原生30.834420秒／未签名iOS30.590615秒通过，7摘要匹配；首次2个既有case2.060471秒、57断言通过，15跳过、零新case。两种确认形式quote／plan5→4，deferred结算后＋4日、food10→5并治疗；Luck两public别名及解雇清理通过。0日原源适配未证明，零App／写档／资源变更 |
| [251–253房屋字幕与头盔显示](handoff/house-ui-and-equipment-251-253/README.md) | 009普通任务全文／给key及穿戴暴露头盔magenta矩形，仅新槽3默认名保存一次，009已退出收集。最终原生176.964303秒／iOS171.930220秒通过、5源码摘要匹配；253只换原6helm1v1为Indexed8。010正常读槽3，首个inventory画面1691透明正常、armor84／mace66正确，无夹具或修后写档；010随后继续普通游玩；四档在装备画面核对后摘要一致，尚非最终收尾。学习／122／1690仍限源码构建，不计中文自定名或全部装备UI通过 |

暂停前安装的是232，最后观察为House64标准商品页。该页已有截图在暂停后仅作离线复核：8件商品和600金币报价可见，拖离、购买与交易结果未执行。
目标iPad的商店拖离取消、银行和长问题输入仍未验；243–245仅补Mac普通银行、商店、generated NPC及受影响修复，完整自然通关仍未证明。
卷宗三页阅读通过不替代长NPC或问题输入验收。
已通过的短流程不重跑，不新建大验收矩阵或为微小修复发布检查点。

存档保留结论只对应232安装、导入和受影响复验后的明确核对时点：13个工作档与10个保留原件各自前后摘要一致，
即22个既有档和新Save12均未变。之后自然行走、休息至次日6点及入店没有新增保存；暂停时没有再次回读核对，
不得把上次摘要结论扩大到暂停时点。新Save12／105与全部旧格式原件继续保留。
合法的 `save10.oysav` 是195正常新建牧师的“中文保存测试”手动档；未来用户明确恢复设备工作后可用于新索匹格镇银行条件，
暂停期间不访问设备；240–241仅在隔离Mac工作目录正常读取本地Save10副本，没有写新档或覆盖原件。
243另经正常UI只写新的本机隔离槽1供修复续查，不是设备槽1或覆盖原Save10；修后零写档，新槽及原Save10／隔离副本摘要均未变，该新私有档不进入公开交付。
244两次正常读该槽并进行商店检查，零写档；新槽、旧原件和隔离副本摘要继续保持。
245仅通过正常UI新写空槽2／Save106一次；修后零写档，原件、Save10副本、隔离槽1／槽2共4份摘要全部未变，隔离存档仍仅3个，私有档不进入公开交付。

快速tap、细小轨迹与问题组合文本的源码缺口已在235修复；目标iPad银行／商店及实际长问题输入仍未验。
Yell飞行方向已在236源码与聚焦离线范围修复；标准树精原数据没有证明新缺口，不套用别版本ID。
UIKit隐藏输入框的preedit桥接已在237完成源码与编译验收；保留原提交差分，没有声称候选曾提前写入答案。
普通Save Escape对脚本拒绝的传递缺口已在238修复并完成受影响源码／构建／输入检查。
各handoff中的旧设备继续语句以本次暂停指令为准。236–240资源完全沿用235；完整241位于外层
`local-data/validation/quit-confirmation-ui-241/runtime-002/`，只更新`data_tables/ui_text.txt`及conversion记录，
其他17398个engine成员与四个world沿用235；world身份由相同inode及235既有摘要继承，没有重读四个大文件。
241、246、253的单成员overlay按固定229完整六文件→231–232→234→235→241→246→253顺序搭配当前源码使用，不是完整runtime或新Release。
242–245沿用完整241；246/runtime-002只更新engine中的ui_text，247–250无资源变更。
253已组装完整开发资源，位于外层`local-data/validation/natural-mac-gameplay-251/runtime-001/`，只替换`icons/6helm1v1.bmp`，其余17398成员与四world保留；实际仅010首个受影响装备画面通过。
当前UiText233（157原源＋76项目）、Combat223、导航222不变；253单成员覆盖包见整组交接。
当前写Save106，回退固定229或235需保留的105或更旧可读副本；不覆盖原存档。
240–241只复核了内联CUA画面，没有导出截图；241二次退出后的AX观察可能重启至冷菜单，该段不计冷启动验收，
随后正常退出，最终确认该次进程不存在。临时启动／打包环境失败和资源组装误报均保留，不是新增产品修复；详情见240–241交接。
242已按受限源码／构建及单一既有组件case通过；室内外实际怪物AI→生成→命中串联、Sunray遭遇和像素仍未验。
DayOfGods83未实现，MassCurse／FingerOfDeath保留原范围，详见242交接。设备无关开发继续，不能声称全部offline已完成。
243两项修复只涉及传递既有HouseServiceTextTable以复用GlobalTxt414，及Trigger／Use保留同批pressed边沿；
原生21.744259秒、通用未签名iOS15.954347秒及一次既有输入检查0.897902秒均通过，4冻结源码摘要一致。
既有检查新增6条受影响断言、零新case；两个旧headless闭门期望接当前表但不回放旧路线，不重复生产构建或存取款。
修后正常读新槽保留gold190，短按Space入行显示bank10；正常R休息8小时由09:14至17:14、food5→3，
右键仅放置门上指针，默认短按E显示完整无裁切的“此处在9AM到5PM开放”。005进程已正常退出并收集；
243结束时006停在菜单暂停状态，随后完成244初次自然商店流程并正常退出、收集。
244基于243源码`d956e8cb`追加独立Cocoa补丁，仅对绝对Dragged跳过按钮reconcile，保留旧release／position等路径。
原生首次2.938982秒通过，2冻结文件摘要一致，3个补丁对实际SDL依赖的反向检查通过；没有单元运行或新case，iOS不重建、沿用243产物。
修后007正常读同槽、步行至同店，头盔报价90；拖离实际取消后gold190／商品仍在，正常点击再购成功190→100并在行囊确认。
源码可解释旧XY合成UP风险，但未记录本次global按钮值，不声称已捕获该具体分支；007随后完成245初次NPC流程并退出。
245只将NpcProfessionDescription接回既有runtimeNpcEntry与formatNpcProfessionText，保留原问候、乞求、职业和费用规则。
初次威尔勒德职业16说明（兵器技能＋3、原费用字段400）原样显示%01／%17；修后实际由另一NPC伯尼验证通用formatter，
显示“你的所有角色的交际术增加8点。伯尼花费你得到的所有金币的百分之3。”，完整两行、无占位符，金币100／食物5未变。
同NPC同日再乞求2651拒绝，以及正常休息到Day3后访问不同NPC，均为普通流程，不是产品失败。
随后同伯尼“议会成员”普通News完整三行中文、无裁切，只计该话题，不泛化长文本。
原生7.232242秒／通用未签名iOS10.339974秒通过，1冻结源码摘要一致，零单元运行／新case；008正常Quit两次后PID71068确认不存在，最终日志已收集。
246失败正文直接进入重开对话的源码链路已核对，但该批没有App运行，未证明像素或主动能力实际使用；加入／离开成功信息不冒称HUD通过。
246组装001的排序前228行预检错误已改按key／value比较；初失败保留，没有重复构建／导入，4233字节单成员覆盖包见246交接。
247复用原脚本职业15／16／46的七武器＋2／＋3／＋2，以独立瞬态weapon表接有效技能；有效rank非零时窄化处理None→Normal，stored技能／精通和装备资格不改。
雇佣、更新、解雇、恢复、seed、成员加入／重入均接刷新；Inn／Detached清旧，不因刷新叠加。既有case也检查item bonus隔离、重建、内存snapshot恢复及解雇复原，不称实际磁盘读档或冷启动。
首次case仅因Bow夹具误选equipStat而停止，改为原表Missile后通过，未重跑已过生产构建；没有App、存档写入、路线或战役回放。
248–249已完成23项技能有效副本及独立防御projection，stored技能／精通、装备资格、item处理、原公式和Save106保留。
职业37可合法雇不同NPC两名，原NPCFollowers按id去重、Stats逐人加四抗20；两名应＋40，不能把刷新去重写成同职业不叠加，来源见248–249交接。
首次聚焦72断言71通过，价格夹具触最低价1000而失败；只改两个multiplier为2.0，生产源不变，聚焦编译11.934844秒后仅重查失败项1.942915秒／36断言通过，其他60个case未跑。
既有组件检查覆盖XP125、价格／utility／AC、真实buff与projectile rank、override／解雇、无Event／不双加／stale Event及实际伤害函数；没有资源导入、App、存档写入或战役回放。
250只在两个既有case检查walking生产确认→deferred路径和ActualLuck脚本getter；Party有职业5、Event followers为空时quote／plan5→4，结算前时间／食物不动，结算后＋4日／food10→5并治疗。
ActualLuck 0x2D／0x26读取已投影值，Base／未投影／解雇语义保留；相邻Resistance仅raw／Base／Bonus，未找到ActualResistance消费者，未改。
步行减时只处理正Days的.odm→.odm，原Lua无条件min1但未找到认证0日实例，default／未证0日保留，不能称完整原源parity；脚本／MM9／boat等路径保持。
250本批零App、存档写入、资源导入、设备／模拟器、旧路线／战役回放或Release，不包含后续普通Mac游玩。
设备无关工作与完整自然战役仍未完成，不能宣称所有NPC或全部离线工作完成。
这些Mac观察不替代实际IME／preedit、极端容量或目标iPad触控验收。
整体约88%只是低置信度工程判断，不是任务覆盖率或自然通关比例，完整目标仍未完成。
只公开已脱敏的handoff、源码与资源摘要及已复核游戏画面；私有容器、存档、UDID、profile、证书与原始控制台不进入交付。

## 固定229累计交付

以下保留固定229的下载、校验、签名与源码构建说明，供未来明确恢复设备工作时参考；不构成当前设备任务。
该发布集中包含固定214之后的216–229修复，以及214已有的196–214累计内容和195接收端修复。
tag为 `ipados-ui-cumulative-229-20261030`，见[发布页面][release]；发布身份保持原样，不能用当前分支HEAD替代。

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
这些受控主机画面只按原范围引用；后续231–232受限真机结果见上方当前入口，不能扩为全部装备组合验收。

[195接收端记录][device195]已报告普通短流程、旅行、中文Save104新槽保存、前后台恢复、独立进程读档及音画通过。
原始设备证据仍在接收Mac；仓库摘要索引不能替代原图，也不能推定229全部设备行为通过。
195及231–232已通过的短流程无需重打。完整自然通关、全部UI和长期稳定性仍未证明；
当前整体约88%的低置信度工程估算及具体未验项以上方当前入口为准。

## 存档和数据保留

固定229程序写入 **Save105**，已验证读取真实Save104；该新增字段保留普通旅行到达意图。
当前253开发程序沿用236引入的Save106，真实105／104读取证据沿用236，具体差异见上方当前入口；固定229不能读取106。
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

## 未来恢复设备工作的边界

2026-10-30的明确授权已用于231–232；最新暂停指令覆盖当前设备工作。
[真机接手提示词](CODEX_IPAD_DEVICE_PROMPT.md)仅供将来用户明确恢复设备调试后使用，当前不发送、不执行，也不重复请求授权。
保存文档、发布附件或收到通知都不会自动启动设备任务；设备无关开发继续推进。

未来恢复后，由主代理先确认当时实际源码／App／资源身份并完整备份、核对工作档及保留原件，再做保留数据更新。
仅在正常续玩自然具备银行金额、商店拖离、长问题等未验条件时补对应操作；无场景继续标未验，
不制造物品、技能、任务位、传送或故障资源。新失败保留原始操作与证据，只做最短复现和受影响复查。
不重跑既有195及231–232短流程、潜艇、旧结局、三大陆战役或旅行／休息／冷启动全套；未验和跳过不计通过。

新增设备证据使用工作区外层独立目录，公开交接只保留脱敏结果和已复核游戏画面。
私有设备标识、签名信息、容器、存档和原始日志仍保留本地。主代理继续独占构建、App、Git及未来获授权的设备操作。

[release]: https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-cumulative-229-20261030
[device195]: handoff/ipados-device-195-20261029/README.md
[device232]: handoff/ipados-device-231-232-20261031/README.md
[result232]: handoff/ipados-device-231-232-20261031/scoped-result.json
[h233]: handoff/item-status-ui-233/README.md
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
