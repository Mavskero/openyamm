# 17张时空之门目的地实际加载验收（Checkpoint 125）

日期：2026-09-16。结论：**ACCEPTED**。

本章从121的`unique-destinations.tsv`直接选取每张唯一地图的首个代表日，没有另写目标真值表。121共有21种地图／图标组合、17张唯一地图；122已实际验收其中3张，本章用28个独立Mac Metal进程补测其余14张（14次实际卷轴旅行、14次独立重载），全部正常退出。

## 验收方法

- fixture只声明代表日、健康且可操作的队伍、`MetVerdant=1`和一张item190。旅行必须经过背包拾取、纸娃娃普通物品使用和pointer选择，不能直接加载目标地图。
- 对新测14张地图核对目的地resolver、地图／世界／图标、原始Party Start装饰或center-height fallback来源、生产碰撞落地、8帧普通重力／碰撞、角色头像选择、至少8单位Forward移动、保存和独立冷／暖重载。
- 保存审计要求卷轴只减少一张，除物品和合法时间推进外队伍核心不变；旧地图`outdoor_event_vars`和归档actor核心保持，重载后目的地静态状态一致。
- 122的`out01.odm`、`out11.odm`、`outd2.odm`在相关产品实现未变化的前提下引用原验收，不重复运行。

## 逐图结果

| 地图 | 大陆／代表日／图标 | 证据 | 入口来源 | 选择／抵达截图 | 操作与重载 | 结果 |
| --- | --- | --- | --- | --- | --- | --- |
| `out01.odm` | Jadame / 0 / `tpjadam1` | 复用122 | Party Start `(3560,7696,545)` | `three-continent-scroll-122/continuous-005/leg2-mm8-{selection,arrived}.bmp` | item190→pointer→Forward；`reload-mm8-001` | 正常退出，接受 |
| `out02.odm` | Jadame / 1 / `tpjadam2` | 125实测 | center-height fallback `(0,0,513)` | `probe-out02-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`probe-reload-out02-001` | 正常退出，接受 |
| `out06.odm` | Jadame / 2 / `tpjadam3` | 125实测 | Party Start `(832,-12000,97)` | `actual-out06-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-out06-001` | 正常退出，接受 |
| `out07.odm` | Jadame / 5 / `tpjadam6` | 125实测 | Party Start提示`z=826`，碰撞地面`z=813` | `actual-out07-003/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-out07-003` | 正常退出，接受 |
| `out08.odm` | Jadame / 7 / `tpjadam8` | 125实测 | Party Start提示`z=0`，碰撞地面`z=141` | `actual-out08-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-out08-001` | 正常退出，接受 |
| `7out03.odm` | Antagarich / 0 / `tpantag1` | 125实测 | center-height fallback `(0,0,545)` | `actual-7out03-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-7out03-001` | 正常退出，接受 |
| `7out06.odm` | Antagarich / 1 / `tpantag2` | 125实测 | center-height fallback `(0,0,801)` | `actual-7out06-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-7out06-001` | 正常退出，接受 |
| `out09.odm` | Antagarich / 3 / `tpantag4` | 125实测 | Party Start `(15632,5520,22)` | `actual-out09-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-out09-001` | 正常退出，接受 |
| `out11.odm` | Antagarich / 4 / `tpantag5` | 复用122 | center-height fallback `(0,0,2241)` | `three-continent-scroll-122/continuous-005/leg1-mm7-{selection,arrived}.bmp` | item190→pointer→Forward；`reload-mm7-003` | 正常退出，来源限制保留 |
| `7out04.odm` | Antagarich / 5 / `tpantag6` | 125实测 | center-height fallback `(0,0,2465)` | `actual-7out04-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-7out04-001` | 正常退出，接受；镜头紧贴石质几何 |
| `7out05.odm` | Antagarich / 7 / `tpantag7` | 125实测 | center-height fallback `(0,0,1377)` | `actual-7out05-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-7out05-001` | 正常退出，接受；记录高处视界边界 |
| `7out13.odm` | Antagarich / 8 / `tpantag8` | 125实测 | center-height fallback `(0,0,3585)` | `actual-7out13-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-7out13-001` | 正常退出，接受；记录高处视界边界 |
| `outc2.odm` | Enroth / 0 / `tpenroth1` | 125实测 | Party Start `(2896,6768,257)` | `actual-outc2-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-outc2-001` | 正常退出，接受 |
| `oute3.odm` | Enroth / 1 / `tpenroth2` | 125实测 | Party Start `(-9728,-11319,161)` | `actual-oute3-003/{selection,arrived}.bmp` | 从正常`outd3`存档跨图进入；`reload-oute3-003` | 正常退出，接受 |
| `oute2.odm` | Enroth / 2 / `tpenroth3` | 125实测 | Party Start `(-1792,-14624,1)` | `actual-oute2-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-oute2-001` | 正常退出，接受 |
| `outc1.odm` | Enroth / 3 / `tpenroth4` | 125实测 | Party Start `(12512,-3040,97)` | `actual-outc1-001/{selection,arrived}.bmp` | 碰撞、头像、Forward；`reload-outc1-001` | 正常退出，接受 |
| `outd2.odm` | Enroth / 4 / `tpenroth5` | 复用122 | Party Start `(6880,-22208,97)` | `three-continent-scroll-122/continuous-005/leg3-mm6-{selection,arrived}.bmp` | item190→pointer→Forward；`reload-mm6-001` | 正常退出，接受 |

完整绝对/相对证据路径及每张图的来源细节见`local-data/validation/live-destinations-125/map-results.tsv`。

## 自动与可视检查

- `structural-audit.json`：790/790检查通过，覆盖17张图、28个新进程、70张新BMP及122复用证据。
- 人工复核42张本章代表画面：14张选择、14张抵达、14张独立冷启动；122的21张已复核BMP沿用原结论。PNG仅为诊断预览副本，原BMP未改动。
- 相关native回归：`dimension_door_check` 26/26、519断言；`dimension_door_destination_check` 14/14、15187断言；`town_portal_contact_check` 7/7、488断言。
- `out07`验证器最初把原始装饰Z误当最终脚底Z，修为经过生产碰撞地面解析后通过。`oute3`为证明实际地图加载改用正常`outd3`来源，随后让fixture动态寻找空背包格，第二次修复通过。最终同bug连续失败计数为0，未触发Astra升级。

`7out04`的center-height fallback靠近石质几何，`7out05`与`7out13`的高处画面能看到视界边界；这些画面与独立重载一致，运行时缺失纹理计数为0。本章没有任意改落点或补贴图，也不据此宣称完整画面保真。center-height fallback只证明既定规则可落地、可操作和可持久化，不证明原版存在同名Party Start。

## 边界与总体进度

本章通过的是17张唯一目的地的受控item190落点、基本操作与持久化，不是这些地图全部任务、路线或自然邀请剧情通过。没有操作iPad或模拟器；最终通用iOS无签名Release编译留到132。

总体工程估计仍为**55%–60%，低置信度**。依据是实际Mac目的地覆盖已从3张扩展到17张，但自然Merge剧情、代表性任务链、持续运行、最终完整导入和iPad真机验证仍未完成，不能按章节机械增加百分比。
