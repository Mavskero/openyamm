# 248–249：被动随从加成接入实际计算

状态为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`，见[受限结果](scoped-result.json)和[源码摘要](source-sha256.json)。
基线`cb084d3a06c8085939ff60c1b0b00d8ed3125271`已推送并核对；7个生产源＋2个既有测试文件共9份最终摘要已核对，不代表所有NPC功能已完成。

248共享`Character::skillWithNpcBonus`返回stored＋NPC的技能副本，不含item；23项canonical临时projection覆盖7武器、9法术学派、Learning、Merchant、Disarm、Perception和三类护甲。
consumer接经验、价格、陷阱、暗门、AC及施法实际计算；原公式、item别名、GM、canAct、override、known／装备资格、raw stored和Save106保留。

249补职业27／28的Luck＋10／＋20、职业37每人的四抗＋20实际承伤／状态路径；独立typed defense及projection marker由Party权威重建，包括空／inactive followers。
未投影standalone的Event fallback保留；带Event时不双加、不复活stale Event，Luck summary与承伤使用同一实际值。
原NPCFollowers.lua108–115按NPC id去重，220–257／569–570及职业表37 Enchanter（Cost1000、Joins=x）没有职业互斥，两个不同NPC可合法同为37。
NPCFollowersSkills.lua116–132逐人累计四抗＋20，因此两名37合计＋40；原helper的presence-only固定20也已修正。
其他源依据为NPCFollowersSkills.lua54–57／239–261／296–301、ExtraEvents.lua770–779／824–850／892–903、RemoveSkillValueLimits.lua60–89／443–463及原exe调用路径。

全量原生187.762773秒／通用未签名iOS194.328930秒通过；随后37叠加修正仅重编NpcFollowerRuntime.cpp，增量原生3.467936秒／iOS6.777035秒通过，之后才首次运行聚焦case。
首次3个既有case用时1.975212秒，72断言中71通过；spell和defense两项通过，utility仅因价格夹具multiplier1.0触最低价1000而失败。
只将两个夹具multiplier改为2.0，生产源不变；聚焦编译002为11.934844秒，仅复查失败utility用例1.942915秒、36／36断言通过，其他60个case未跑，原两项通过没有重跑。
合计覆盖3个不同既有case、零新case：真实经验函数100→125、价格优惠、Perception／Disarm有效7、护甲AC＋2、buff／projectile有效rank、override及解雇，
借用法术有效Normal而stored仍None，以及无Event防御／Event去重／stale Event／真实伤害RNG函数和两名37的＋40。这里只是组件范围，没有App、自然雇佣或实战像素验收。

完整资源继续246/runtime-002、UiText233、Combat223、导航222和Save106，没有资源导入、App运行、存档写入、设备／模拟器操作、战役回放或新Release。
固定发布与原存档保留，真机调试仍按用户要求暂停；子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git。
两个主入口已累计248–249；整体约88%仍为低置信度工程判断，完整自然战役及全部离线工作完成均未证明。
