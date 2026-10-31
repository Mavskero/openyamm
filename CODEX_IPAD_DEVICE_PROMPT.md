# 真机接手提示词：仅供未来明确恢复时使用

更新：2026-10-31。用户现已要求暂时中止全部开发并上传GitHub，goal工具已确认PAUSED。
**当前全部开发与真机调试均暂停。** 模拟器和本地游戏已确认未运行，所有子代理及构建任务已收尾。
先读[暂停总交接](handoff/paused-offline-256-20261031/README.md)；其中包含相对固定229的累计14成员资源覆盖包和当前conversion。
256源码／构建／资源通过，修后树名实际画面未运行；不得把本文件的未来提示词当作当前恢复授权。

下方是未来用户明确恢复设备工作时才使用的提示词，不是当前设备任务。
保存此文件、发布附件、交付通知或历史2026-10-30授权均不覆盖当前暂停；现在不要发送或执行下方任务。

---

我现在明确恢复OpenYAMM iPadOS目标真机调试，授权本会话操作我提供或连接的目标iPad：
识别设备，使用本机有效Team签名，完整备份后保留数据地更新安装，启动调试、读取日志和截图，
导入我合法拥有的资源；只在正常续玩遇到具体问题或自然具备未验条件时做最短必要检查。
不使用模拟器，不卸载旧App、不清容器、不重置设备、不更改bundle ID `org.openyamm.ipados`。
需要用户登录、信任或解锁时指出具体一步，其余已授权准备继续推进。

先读当前 `AGENTS.md`、`IPADOS_HANDOFF.md`，以及
`handoff/ipados-device-231-232-20261031/README.md` 与 `scoped-result.json`。
仓库为 [Mavskero/openyamm](https://github.com/Mavskero/openyamm)，开发分支 `codex/ipados`；
提交`8989cf96dcd1d5c23de2d5da2a58030c92e3291d`记录231–232源码和脱敏受限真机结果。
233–254及后续集成身份按当前Git提交和对应handoff摘要核对，254仅资源变更；242已推送并核对`673040d434d17207c0ac02f37763f271076c7b1f`，
243–245为`ACCEPTED_OFFLINE_MAC_SCOPED`，Mac自然银行、商店、generated NPC及受影响修复已受限通过，未扩大到目标iPad验收。
246随从字幕、247武器随从加成按源码／构建及既有组件范围受限通过，由主代理一次累计源码提交；没有新增App或设备验收。
其累计基线`cb084d3a06c8085939ff60c1b0b00d8ed3125271`已推送并核对；248–249为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`，7生产＋2既有测试共9份最终摘要核对。
248–249已提交并推送`69c2fe94a574d0dbf34e06b5113670ccea1053bb`，远端HEAD及提交内9文件摘要核对通过；250已按源码／构建及两个既有case受限通过，7文件最终摘要全部匹配。
250已提交推送`1bfefd6ff5fa1b970922a8e3e7072bd19503b25b`，远端及提交内7份摘要核对通过；251–253合并交接为`ACCEPTED_OFFLINE_MAC_SCOPED`，5源码摘要匹配，实际仅010首个装备画面受限通过。
251–253提交`dba702bf92d8a38fb4097f1b419b89803a44715a`已推送且远端5源码摘要核实；254为`ACCEPTED_OFFLINE_RESOURCE_SCOPED`，零源码／构建／单元／App操作。
恢复前先核对最新工作树和主代理后续结果，保留未提交工作，不reset／clean，不把该提交或旧Release当成更新后程序身份。

固定Release229为 `ipados-ui-cumulative-229-20261030`，生产代码提交为
`8cdae9215ef7bb52e8fe8f6e83bf16641280fd0e`；完整发布源码身份从该发布 `VERSION.json → source_head` 读取。
按 `SOURCE-MANIFEST.json`、`ARTIFACT-SHA256SUMS`、`SHA256SUMS` 和 `delivery.json` 核对该固定发布，
保留229、214、195和RC1–3原附件。231–254没有新Release，不能声称229附件已含后续修复。

231–232实际通过旧Save104读取、正常UI穿戴护甲84／主手武器66、新槽Save12／105，
以及两次保留数据更新后的独立进程读档。中文Stats／技能表头、真实重叠双指物品检查、
普通NPC新闻592、卷宗2125三页阅读／关闭／归位、232绿色警戒透明和卷宗触控中性说明已复核。
这些是受限短流程，不是完整正常游玩、全部UI或自然通关证明。
232完整资源保留固定229四个world；公开handoff中的8成员engine覆盖包只搭配匹配源码与完整229资源使用，不能单独启动游戏。

233破损／未鉴定字幕通过源码、原生／未签名iOS构建及导入，见`handoff/item-status-ui-233/`。
234已集成随机generated NPC动态问候与原接待规则、银行／保存有序编辑、商店拖离取消、银行容量边界，
以及House64“学习技能”。原生／未签名iOS／导入、现有输入检查和两条既有NPC用例52断言通过；
没有新增用例、App运行、存档写入或设备验收，详见`handoff/offline-function-completion-234/`。
234增量包含相对232的ui_text和Global.lua；235继续修快速tap起点和细小运动路径，问题临时显示组合文本，
UiText227增加“答案：”。235原生／未签名iOS／核心导入和现有输入检查通过，实际问题IME及像素未验。
详见`handoff/offline-input-completion-235/`，使用匹配源码及完整资源，恢复前继续核对后续身份；
不要把源码／离线通过当作像素或触摸通过；235时UIKit缺少的TEXT_EDITING桥接已由237源码补齐，实际UIKit输入仍未验。
236已补原源认证的Yell固定飞行Z，当前开发程序写Save106；原生／未签名iOS、一次聚焦codec／AI检查通过，
真实旧105／104可读且原字节未变，但没有实际飞行像素或设备验收。见`handoff/yell-flight-direction-236/`。
237继续补UIKit持久preedit桥接、保存／建队／银行显示及有序控制、控制台清理，建队／答案补粘贴。
原生／未签名iOS及一次现有输入检查通过，15源码摘要一致；无App／设备／交易或实际IME验收，
见`handoff/marked-text-bridge-237/`。238已修保存页Action113／Screen11的Escape拒绝传递，原生／未签名iOS及一次受控hook输入检查通过；
组合内不发退出，拒绝保留草稿，新按键可立即重试。见`handoff/save-escape-admission-238/`，无设备检查点。
239将保存／银行手动粘贴标为独立PasteText，组合期间保留候选，避免空剪贴板也提前放行确认。
原生／未签名iOS及一次typed消费者检查通过，4源码摘要一致；239当批未做实际剪贴板／输入法／交易检查，
见`handoff/clipboard-edit-source-239/`，没有资源、存档格式或设备变更。
240另用macOS原生CUA粘贴实际观察正常建队名字“离线输入240”→Return→Cancel，及正常Load本地Save10副本→ESC菜单保存→
双击空槽1→粘贴“离线存档240”只显示一次→Escape取消；没有IME preedit、UIKit输入或存档写入，副本原SHA保持。
241仅把首次Quit确认接原GlobalTxt82，UiText228＝157原源＋71项目，完整旧227值保留。
原生143.473秒／通用未签名iOS134.596秒／核心导入0.195秒通过，4源码摘要一致；实际正常Load副本→ESC→首次Quit中文完整无裁切。
二次退出后的CUA AX观察可能重新拉起至冷菜单，不能计冷启动验收；随后正常菜单退出，最终该次进程不存在。
两批只查看内联CUA画面，没有导出截图，见`handoff/offline-ui-input-240-241/`；这些Mac观察不替代实际preedit、触控或银行／商店交易验收。
236–240沿用完整235；241/runtime-002只更新ui_text和conversion记录，其他17398个engine成员／四world沿用235，Save106不变。
固定229及235程序不能读取新106，回退须用保留的105或更旧可读副本。
242已受限通过：原脚本Paralyze81→Sunray87和原exe命中后伤害路径已认证，补87名称支持及接触后掷骰。
原生136.936秒／未签名iOS132.004秒通过，5生产＋1既有测试文件共6摘要一致；一个既有组件case0.188秒、79断言，
其中15条受影响新增断言，零新case、其他28跳过。首次仅测试宏括号语法编译失败，未跑case，修正后未重跑已通过的生产构建。
两world实际AI→生成→接触整合仍仅源码／构建，未做App、Sunray实战、像素或设备验收；DayOfGods83未实现，MassCurse／FingerOfDeath保留原范围。
见`handoff/monster-sunray-242/`。242无资源或存档格式变更，完整241/runtime-002／Ui228／Save106继续保留，不能宣称全部离线工作已完成。
243使用本地旧Save10副本正常步行到MM6 House297储蓄所，闭门时R显示17:34；正常休息到Day2 09:00后入行，
原生CUA粘贴17／Return实存、7／Return实取，gold200→183→190、bank0→17→10，正常离开。
只经正常UI新增本机隔离槽1“离线银行243”Save106供修复续查，原Save10本地原件与隔离副本原SHA保持，私有档不上传。
闭门字幕传递已有HouseServiceTextTable及短Trigger／Use边沿两项修复已通过原生21.744259秒／通用未签名iOS15.954347秒／
一次既有输入检查0.897902秒，4冻结源码摘要一致，6条新增受影响断言、零新case，旧headless路线不回放。
修后正常读新槽仍为gold190，默认短按Space入行显示余额10；正常R休息8小时由09:14至17:14、food5→3，
右键仅放置门上指针，默认短按E显示完整无裁切的“此处在9AM到5PM开放”。不重复存取款，修后零写档，
原Save10、本机副本及新槽摘要均未变。005已正常退出并收集；243结束时006菜单暂停，随后完成244初次商店流程并正常退出、收集。
没有资源／schema／存档版本变化，仍沿用完整241、Ui228／Combat223／导航222／Save106，详见`handoff/natural-mac-bank-243/`。
244从243源码`d956e8cb`及同隔离槽1正常步行到MM6 House62，初次拖离误购买90头盔，190→100；正常出售17后100→117。
Cocoa独立补丁只在绝对Dragged事件前跳过按钮reconcile，既有release／position、相对模式及普通motion／enter保留。
原生首次2.938982秒通过，2冻结文件摘要一致，3个SDL补丁反向检查通过；零单元运行／新case，Mac平台guard下不重建iOS、沿用243产物。
修后007正常读同槽，gold190、food5；步行至同店，同头盔报价90，拖向右侧空白实际取消、商品及190金币保留，
紧接普通点击购买190→100，中文购买状态及行囊新头盔可见。全批零写档，旧原件、副本及新槽摘要均未变。
没有记录global按钮值，不将源码解释写成已捕获的运行分支；007随后完成245初次NPC流程并退出，详见[244交接](handoff/natural-mac-shop-244/README.md)。
245正常城镇交互中，威尔勒德首会动态问候2662、乞求2667成功；更多信息显示职业16原样%01／%17，占位符问题据此修复。
唯一源码改动是NpcProfessionDescription调用既有runtimeNpcEntry与formatNpcProfessionText，保留问候、乞求、职业及费用规则。
正常UI仅新写空槽2／Save106一次，原件保留；修后008正常读取槽2，同NPC当天再次乞求2651正确拒绝。
正常休息到Day3后遇另一NPC伯尼，乞求成功后更多信息完整两行显示“你的所有角色的交际术增加8点。伯尼花费你得到的所有金币的百分之3。”，
无%01／%17，金币100／食物5未变；这验证通用formatter，不声称修后威尔勒德原职业分支已复验，同日拒绝与次日正常访问不是产品失败。
随后同伯尼“议会成员”普通News完整三行中文、无裁切，仅计此话题，不泛化所有长文本。
原生7.232242秒／通用未签名iOS10.339974秒通过，1冻结源码摘要一致，零单元运行／新case；零修后写档，原件／Save10副本／隔离槽1／槽2四哈希未变，隔离存档仍仅3个。
008正常Quit两次后PID71068确认不存在，最终日志已收集。
资源和Save106不变，没有状态夹具、传送或规则修改，详见[245交接](handoff/natural-mac-generated-npc-245/README.md)。
246普通随从不可用／已跟随／加入／离开／名字fallback接typed当前UI，主动能力复用原GlobalTxt569／561／140；UiText233＝157原源＋76项目，旧228键值和完整表兼容保留。
原生114.342001秒／未签名iOS107.299712秒／导入0.190589秒／组装002为6.790999秒通过，4冻结摘要一致；零单元运行／新case／App／写档。
失败消息进入重开对话的源码链路已核对，加入／离开成功消息可见性未独立确认，不能声称HUD或实际主动能力通过，见[246交接](handoff/follower-feedback-246/README.md)。
246/runtime-002只更新engine内ui_text，其他17398成员／四world保留；4233字节覆盖包仅含ui_text，不是完整runtime或新Release。
247将原职业15／16／46七武器＋2／＋3／＋2接入实际战斗有效技能，独立瞬态weapon表与item bonus隔离；窄None→Normal遵原packed skill，stored技能／精通、装备资格及Save106不改。
雇佣、更新、解雇、恢复、seed、成员加入／重入均刷新，Inn／Detached清旧不叠加；原生166.684508秒／未签名iOS171.877579秒通过，5生产＋1既有测试共6摘要一致。
一个既有MM7Mechanics case2.029935秒、27／27断言通过，14个其他case跳过；含Mace4→7命中＋3、未学Bow有效3／Normal但storedNone、重建／内存snapshot／解雇，不称实际磁盘读档或冷启动。
零新case／App／写档／资源变更。
首次case仅Bow夹具筛选误把原Missile当Bow，修夹具后通过，生产构建未重跑，见[247交接](handoff/weapon-follower-bonus-247/README.md)。
248–249共享stored＋NPC且不含item的技能副本，以23项临时projection接经验／价格／utility／护甲／施法；独立防御projection补Luck和四抗，保留原公式、stored技能／精通、known／装备资格及Save106。
Party权威刷新含空followers，standalone Event fallback保留，带Event时不双加或复活旧值；原NPCFollowers按id去重且允许不同NPC同为37，原Stats逐人四抗＋20，两名合计＋40。
全量原生187.762773秒／iOS194.328930秒通过；37叠加修正仅单cpp增量3.467936秒／6.777035秒通过，之后首次3个既有case中spell／defense通过、utility因价格夹具触最低价1000失败。
首轮72断言71通过；只改两个夹具multiplier1.0→2.0，生产源不变，聚焦编译11.934844秒后仅该失败case复查1.942915秒／36断言通过，其他60个case未跑，原两项没有重跑，零新case。
组件检查含XP125／价格／Perception与Disarm7／护甲AC＋2、buff／projectile rank／override／解雇、借用None→有效Normal、无Event防御／去重／stale Event／实际伤害函数及两名37叠加。
247–249继续完整246/runtime-002／Ui233／Combat223／导航222／Save106；本批零App／写档／资源导入／设备／模拟器／战役回放／新Release，详见[248–249交接](handoff/passive-follower-completion-248-249/README.md)。
250为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`：ActualLuck脚本getter的0x2D／0x26别名读已投影Luck；步行quote／plan共享随从5／6／7／44减时，只处理正Days且源／目标均.odm。
首次原生30.834420秒／未签名iOS30.590615秒及聚焦编译7.845368秒通过；首次仅2个既有case2.060471秒、57断言通过，15项跳过，零新case。
walking两种生产确认形式（unpositioned／认证map_boundary）在Party有职业5、Event followers为空时quote／plan5→4，deferred前时间／食物不动、apply后＋4日／food10→5并治疗；Luck public值20、Base／未投影／解雇后10保持。
原Lua无条件min1，但已知原表非空edge均正Days，未找到认证0日实例；default／未证0日保留，0日原源适配未证明。脚本／MM9／boat等路径不变；相邻Resistance仅raw／Base／Bonus，未找到ActualResistance消费者，未改。
250零App／写档／资源导入／设备／模拟器／旧路线／战役回放／Release，继续完整246/runtime-002／Ui233／Combat223／导航222／Save106，见[250交接](handoff/follower-travel-luck-250/README.md)。
251–253为`ACCEPTED_OFFLINE_MAC_SCOPED`：009以250程序／246资源正常经TownHall到Janice大耳怪前哨任务，中文全文与给key已有观察；正常UI穿戴Mace66／armor84／helmet1691（characterData1）后发现头盔magenta矩形。
只正常新增隔离空槽3一次，默认名“新索匹格镇”，粘贴未进入编辑，不能计中文自定名；旧3副本摘要保持，009正常Quit／PID退出／日志收集。
251学习UI单cpp原生4.8234925秒／iOS9.709751秒通过；252通用LearnSkill接现有HouseServiceTextTable，guild122／deck1690复用当前NPC表，保留fallback／门控、无新资源key，学习／122／1690仍只计源码构建。
最终game＋equipment_picture_import原生176.964303秒／iOS171.930220秒通过，5最终源码摘要before／after全部匹配，零新增／运行单元用例。
253复用convertLodImages API提取原6helm1v1为39×33 Indexed8，0.010753秒；1287个RGB像素与旧24bpp逐点相同，恢复668个palette0索引，BMP2398字节。
完整253/runtime-001组装6.454302秒通过，只换`icons/6helm1v1.bmp`，其余17398成员／四world及Ui233／Combat223／导航222／Save106不变；253覆盖包仅该成员，不是完整runtime或新Release。
010用最新程序及完整253资源正常读槽3，首个inventory画面1691头盔透明正常，armor84／mace66正确；只计此画面，无夹具／修后写档，不扩为其他装备runtime或全部装备UI验收。
010已正常退出、PID不存在、日志收集，4／4私有档SHA完全不变，修后零写档；009也已退出并收集，见[251–253交接](handoff/house-ui-and-equipment-251-253/README.md)。
254只将剩余同源6helm1v2／v4／v5三BMP恢复Indexed8，RGB逐点不变，零源码／构建／单元／App；新增三图仅原源和数值范围，未证明实际像素，45个既有missing引用未解决。
待用完整254/runtime-001相对253只改三BMP，17396个其他成员及四world保留；Ui233／Combat223／导航222／Save106不变。
254覆盖包相对246累计v1／v2／v4／v5四图，已含253的v1，不是完整runtime或Release。010始终使用253程序／资源；后续喷泉／受伤两次死亡，正常复活及UI重读槽3，未到训练营，未主动Attack／偷窃／施法。
255仅作定点只读诊断，死亡不是新产品缺陷证明，学习界面仍限源码构建；没有额外单元、构建或设备操作，见[254交接](handoff/helmet-palette-family-254/README.md)。
完整自然战役和所有离线工作完成仍未证明。

暂停前设备最后观察为232的House64标准商品页；已有库存截图在暂停后作了离线复核，但未执行拖离、购买或交易结果检查，商店条件仍未验。
目标iPad的商店拖离取消、银行、长问题输入仍未验；243–245限于Mac普通银行、商店、generated NPC及受影响修复，未验IME／preedit、极端容量或iPad触控。
完整自然通关仍未证明；卷宗三页通过不能替代长NPC或问题输入证据。
只补当时自然具备的一项，不建立大验收矩阵，不重跑195及231–232已通过短流程、旧路线、潜艇、结局或三大陆战役，
也不为微小修复另发设备检查点。无场景标未验，不造物品、技能、任务位、传送或其他游戏状态夹具。

先备份当时全部可访问App数据和全部工作存档并记录摘要，持续保留10个旧原件、新Save12／105和历史备份。
此前“13个工作档与10个保留原件各自前后摘要一致”仅对应232安装、导入及受影响复验后的核对时点，
即22个既有档及新Save12摘要未变。之后自然行走、休息至次日6点及入店未新增保存；暂停时没有再次回读，
不得沿用上次摘要冒充恢复前的新核对。旧档只用副本，新保存只用未占用槽和新名称，不覆盖原件。
合法 `save10.oysav` 是195正常新建牧师的“中文保存测试”手动档，恢复授权后可用于新索匹格镇银行自然条件；
不能修改其物品、金钱或位姿来凑条件。243／245隔离Mac新档不扩大232设备原核对时点的结论。
当前253仍写Save106、真实105／104读取证据沿用236；固定229及235不能读106，
固定214／195及旧RC也不能读取105，回退须用保留的相应可读旧格式副本。

固定229完整六文件为 `engine.zip`、`conversion.json`、`mm6.zip`、`mm7.zip`、`mm8.zip`、`mmmerge.zip`。
其基线runtime227包含214项UiText、Combat223和导航222；当前资源使用固定229→231–232→234→235→241→246→254的engine覆盖顺序。
241与246覆盖包各只有ui_text，254相对246累计`icons/6helm1v1.bmp`／v2／v4／v5四图、已含253单图；均不是完整runtime或新Release，具体摘要见对应交接，完整资源及conversion身份按当时handoff核对。
新机器按 `IPADOS_HANDOFF.md` 组装完整assets目录，engine与conversion在根目录，四个world在 `worlds/`，
从组装后的根目录核对保留该相对路径的 `SHA256SUMS`。五个ZIP保持压缩，旧资源目录保留。
MM6／MM7／MM8在190已改变，不能复用RC2旧world；不能把小型覆盖包或故障资源目录当成完整运行资源。

未签名App需本机有效签名后安装；源码构建和资源必须对应当时集成身份。
新Mac使用本机Team及全新build／DerivedData，不复制旧缓存、env.sh或NVMe路径别名；
原迁移工作区保留既有别名。每次xcodebuild显式写scheme和项目外置derivedDataPath，设备签名构建显式给出 `CODE_SIGNING_ALLOWED=YES`。
不上传签名密钥、证书、令牌、profile、设备标识、用户存档、私有容器或原始控制台。

新失败保留原始操作顺序、截图、日志、版本和前后存档摘要，区分产品与环境问题，只做最短复现及受影响复查。
主代理独占构建、App运行、Git和设备；子代理沿用 `gpt-6.1-sol/max`，只做明确划分的源码或文档任务。
历史goal-tool blocked不阻止已恢复授权的具体工作，也不代表完整目标已完成。
整体约88%是低置信度工程判断，不是覆盖率或自然通关比例。

新增设备原始证据保留工作区外层独立目录；公开交接仅引用脱敏handoff与已复核游戏画面。
报告实际源码／App／资源身份及通过／失败／未验项；跳过和未验不计通过，不重复已通过矩阵。
设备无关修复继续推进，不因缺少某项真机条件而停工。
