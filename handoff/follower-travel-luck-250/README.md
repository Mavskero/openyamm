# 250：随从步行减时与ActualLuck脚本读取

状态为`ACCEPTED_OFFLINE_MECHANICS_SCOPED`，见[受限结果](scoped-result.json)和[源码摘要](source-sha256.json)。
已验收基线248–249为`69c2fe94a574d0dbf34e06b5113670ccea1053bb`，已推送，远端HEAD及9文件提交摘要核对通过。

脚本`ActualLuck` public getter的0x2D及现有0x26别名读取已投影的随从Luck，raw／Base语义保留；这是脚本读取路径。
步行跨图quote与两种plan producer共享职业5／6／7／44的减时，以Party随从为准；只处理正Days且源／目标均为.odm的旅行，减时下限1日。
减时来源为NPCFollowersSkills.lua63–75／267–272；本地已核对的原表非空edge均为正Days。Lua无条件min1也覆盖0，但没有认证的0日实例。
因此default／未证0日语义保留，0日原源适配未证明，不称完全parity；脚本指定旅行、MM9、boat／stable和原户外食物成本保留。
Luck来源为NPCFollowersSkills.lua131–132／301及原GetLuck 48C672→48E213。相邻Resistance脚本变量只有raw／Base／Bonus，未找到ActualResistance消费者，保持不改。

首次原生30.834420秒、通用未签名iOS30.590615秒通过；聚焦编译7.845368秒，首次仅2个既有case运行2.060471秒，2项／57断言全部通过，15项跳过，零新case。
walking case经生产确认与deferred路径检查unpositioned及认证map_boundary两种形式：Party有职业5而Event followers为空，quote／plan5→4，结算前时间／食物不动，apply后＋4日、food10→5并触原治疗。
Luck case新增5条受影响断言，确认0x2D／0x26实际值20、Base10、未投影10、解雇后10；这些是组件结果，不是自然雇佣随从旅行或实际UI验收。
5个生产源＋2个既有测试源共7份最终摘要，经主代理before／after逐字比对全部匹配。

资源仍为完整246/runtime-002、UiText233、Combat223、导航222和Save106；本批零App运行、存档写入、资源导入、设备／模拟器操作、旧路线／战役回放或新Release。
固定发布与全部原存档保留，真机调试暂停；子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git。
整体约88%仍是低置信度工程判断，完整自然战役和全部离线工作完成均未证明。
