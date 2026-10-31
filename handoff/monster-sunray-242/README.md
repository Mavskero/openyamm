# 242：怪物Sunray弹道支持与伤害阶段

共享怪物表145／146／147／196／197／198／575七行使用Paralyze；原`SpellsTweaks.lua:183–186`在MonsterCastSpellM将81替换为Sunray87，
修复前支持名称却拒绝实际87。本批认证该原源缺口，新增Sunray名称支持；这些是共享表出现位置，不代表已在每个world自然遇敌。

原mm8.exe的MonsterCast入口与selector将87交给共用弹道；Party命中路径进入CalcSpellDamage，87原表为20＋rank×d20，
MonsterToMonster也在命中后调用伤害helper，没有认证生成阶段伤害掷骰。具体地址及原exe摘要见[受限结果](scoped-result.json)。
修复仅为实际87构造瞬态profile，使用实际SpellEntry及弹道最终rank／mastery，不新增持久字段；未结算的87在接触前／area／direct预算为0，
Party或Actor接触通过生命周期、noCollision和反弹等早退后才掷骰一次。室内私有共享AI的87占位伤害清除；显式正projectileDamage、Party／Event及其他法术分支保留。
既有Light抗性、87的FX8090／8091及音频保留，瞬时impact仍仅沿用视觉分支，不编造命中。其他怪物法术阶段与87怪物对怪物命中率未作全局重写。

DayOfGods83原路径直接退出，未认证到可用buff字段，因此未实现；MassCurse保留既有44映射，FingerOfDeath不支持标记保持。
本批没有把这些分支或全部怪物法术计为完成。

原生构建136.936秒、通用未签名iOS构建132.004秒通过，5个生产文件加1个既有测试文件共6个冻结摘要一致，见[源码摘要](source-sha256.json)。
只运行一个既有弹道组件case，0.188秒通过79断言，其中15条为受影响新增断言；零新case，其他28个跳过。
检查使用受控87条目／profile及接触帧决策，覆盖支持名称、接触前预算0、飞行／world不结算伤害、Party／Actor伤害范围、rank0及显式正值覆盖；
不读取线程局部RNG状态，掷骰阶段另作源码核对。室内外实际AI→生成→命中整合仅计源码／构建，没有真实Sunray遭遇或像素验收。

首次检查编译2.737秒因两条doctest布尔宏缺括号失败，当时没有运行case；仅修测试语法，生产5文件与已通过构建不变。
compile002用时3.766秒，随后该case只跑一次。原失败记录保留，没有因此重跑生产构建或全测试套件。

完整241/runtime-002资源、UiText228（157原源＋71项目）、Combat223、导航222及Save106全部沿用。
旧105／104读取证据沿用236，没有重跑codec。本批没有资源修改、App运行、存档读取或写入、设备／模拟器操作、战役回放或新Release，
固定229／214／195／RC1–3及全部原存档保留。

当前真机调试仍按用户要求暂停；子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git，继续具体离线工作。
实际UIKit preedit／输入时序、银行／商店交易、目标iPad触控、自然Sunray实战及完整自然通关仍未证明。
完整目标未完成，整体约88%是低置信度工程判断，不是覆盖率或自然通关比例。
