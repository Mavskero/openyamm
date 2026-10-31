# 247：武器随从加成接入战斗

状态为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`：原脚本武器随从加成已接入生产战斗读取，源码／构建及一个既有组件case受限通过，见[受限结果](scoped-result.json)和[源码摘要](source-sha256.json)。
原NPCFollowersSkills.lua第54–57、240–246行及RemoveSkillValueLimits.lua第60–89行认证七武器加成；当前已有helper原本无人调用，实际战斗漏加。
职业15／16／46分别为＋2／＋3／＋2，使用独立瞬态weapon表，与item bonus分开。
有效rank非零时，只在原None情形临时按Normal处理，遵原packed skill语义；stored技能／精通、装备资格与Save106均不改变。

5个生产源为Party.h／cpp、GameMechanics.cpp、NpcFollowerRuntime.h／cpp；雇佣、更新、解雇、恢复、seed、成员加入／重入均刷新，Inn／Detached清旧，不因重复刷新叠加。
主代理只扩展一个既有MM7Mechanics case，没有新增case；5生产＋1既有测试文件共6冻结摘要匹配。
原生166.684508秒、通用未签名iOS171.877579秒通过；聚焦编译002为7.129129秒，`focused-mechanics-002` 用时2.029935秒，1个既有case、27／27断言通过，14个其他case跳过。
该case确认Mace4→7、命中＋3，未学Bow有效3／Normal而stored仍None，item bonus不混，重建不叠加，内存snapshot恢复有效，解雇后复原。
这些是组件范围，不是自然雇佣、装备操作或实际战斗画面验收；snapshot指内存快照，没有实际磁盘读档或冷启动验收。

首次case001因夹具按equipStat=Bow筛选，未找到原表实际标为Missile的物品，在战斗断言前停止，并非产品失败。
只修这一筛选后重新聚焦编译／运行；已通过的生产构建没有重复，初次失败保留。
本批零App运行、零存档写入、零设备／模拟器操作，不回放旧路线或战役，没有新Release。

资源沿用完整246/runtime-002、UiText233（157原源＋76项目）、Combat223和导航222，Save106不变；固定229／214／195／RC1–3及旧存档保留。
246字幕与247功能由主代理一次累计源码提交，246的4233字节覆盖包不是完整runtime或新Release。
248九系magic加成遗漏已确认、准备继续，尚未通过，不计入247；整体约88%仍为低置信度工程判断，完整自然战役与全部离线工作完成均未证明。
真机调试仍暂停，子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git。
