# 222：恢复 MM8 原生边界旅行

MM8 的完整导入缺少原生默认导航层，已有 Merge 旅行表只覆盖另外的起点。因此普通越界可以生成提示，但 Out02 东侧等基础路线不存在。标准 world_rule_import 现在认证原 mm8.exe、函数与三个字节数组，生成33列 map_navigation.txt：13组原始槽位、28条非零路线，以及原目的地、旅行天数和 named start。先加载原生基础表，再叠已有 Merge 覆盖，不恢复旧 Git 中天数和方向不符的原型表。

当前地图注册表中 out09–12 属于 MM7，原 MM8 对应槽位又全为零，因此明确跳过这些槽位并记录原因；保留属于 MM8 的 out01–08/out13 九行。原有九张世界规则表保持相同内容。

到达意图贯穿实际越界、确认及 App 换图事务。目标标记从原始 entities 和 decoration 表解析，不依赖可渲染 sprite。19种所需标记中16种存在且唯一；其余三个缺失影响四条路线，仍按原引擎保留出发 XY/yaw，再投影目标地形，不猜坐标，也不改用 Party Start。命中和缺失都使用原 D3D 分支的两三角整数地形插值；水/火按已有 Levitate 状态处理，不选 BModel，不加1。通用离开方向不会覆盖原标记方向。费用、OnLeave、加载失败的整笔回滚沿用现有事务。

Save105在 PendingMapMove 尾部追加可选起点方向，覆盖根队列、地图状态与对话嵌套队列。Save104及更早存档默认没有该选择器，继续保留既有落点与已支付费用语义。读写都拒绝非法方向，写入前验证保留已有文件的原子保护。固定214/195/RC1–3发布不改写。

本批只使用既有 Out02 east → Out06 副作用案例：经实际移动帧生成生产确认请求，再确认、支付5天/既有5份食物并落在原 West Start(-22080,-5776)，yaw0；原高度四角均15、descriptor794无水/火标志，独立期待 Z/FallStartZ=480。它包含诊断队伍/坐标/状态前提，不等于自然旅行或完整通关。四条缺标记路线只接受源码路径，未追加路线矩阵。

另做一次聚焦存档格式检查：只读真实104输入，105分别往返根North(0)、地图East、对话West，保持时间/金钱/食物和travelPlan，拒绝损坏方向与无效写入；不跑完整历史存档矩阵。现有codec用例随新尾字段调整；没有新增持久单元测试用例。

完整开发资源以221为底，仅新增 data_tables/map_navigation.txt，原有成员和世界包保留。构建与受影响检查结果写入 scoped-result.json；不将源码完成当作真机、全路线或自然战役验收。完整目标保持 active，整体约85%、低置信度；继续集中修复不依赖真机的具体问题，不新发真机检查点。

原生构建120.831秒、标准世界规则导入0.123秒、运行资源组装2.874秒、聚焦存档检查0.130秒、通用unsigned iOS构建124.270秒通过。既有旅行case第三次10.223秒通过，汇总passed=1/failed=0；前两次检查前提错误及日志保留。两次增量编译只修该case的落地高度前提和session队列读取，不改生产规则。

## GitHub 源码与增量资源

固定214累计发布、195及RC1–3均不改写。本文件夹的 `runtime-overlay/engine/data_tables/` 包含当前132键ui_text和新增map_navigation，合计6702字节，摘要见OVERLAY.json。相对本机221只有导航新增；相对已发布214则包含216–221累计UI字表和222导航。四个world包保持原样。

若接手当前开发分支源码，须使用本分支的Save105构建配套资源；固定214 App和附件继续按214 VERSION使用。需要在另一台Mac组装本分支开发资源时，可保留已下载的214完整资源，在一个新的目录复制后只更新engine的这两个成员。例如下列占位路径都需替换为本机绝对路径，在仓库根目录运行：

```sh
mkdir /ABS/assets-222
cp -c /ABS/assets-214/engine.zip /ABS/assets-222/engine.zip
cp /ABS/assets-214/conversion.json /ABS/assets-222/conversion.json
cp -R /ABS/assets-214/worlds /ABS/assets-222/worlds
(cd handoff/mm8-native-boundary-222/runtime-overlay/engine && /usr/bin/zip -q /ABS/assets-222/engine.zip data_tables/ui_text.txt data_tables/map_navigation.txt)
```

上述覆盖仅操作新目录，原214目录与原校验表继续保留；新engine不再匹配214的旧engine摘要，不能沿用旧SHA256SUMS声称仍为214原包。可把OVERLAY.json附在新目录并记录当前源码head与新engine摘要；conversion的原场景/资源audit仍描述未改的世界包。本机222的完整开发包另外保留新增导航的conversion记录与runtime-diff。

这是一份源码和资源增量说明，不创建新Release或要求新的设备检查矩阵。本主机仍不执行设备/模拟器操作，195已通过的旧短流程不重打。新写Save105应放新槽并保留104原件；214和旧候选不能读取105。完整自然通关未证明，整体估算仍约85%，低置信度。
