# 254：头盔palette0同源资源修复

状态为`ACCEPTED_OFFLINE_RESOURCE_SCOPED`，见[受限结果](scoped-result.json)。251–253源码提交`dba702bf92d8a38fb4097f1b419b89803a44715a`已推送并核对远端5份源码摘要；254没有源码修改、构建、单元运行、新case或App操作。

现有装备stage的1062张图中，6helm1v1已由253保留Indexed8；1058张原palette0颜色可被现有色键识别，剩余3张同源头盔图的(252,0,216)未被识别。
复用253已有Indexed8导出路径，只将以下3张旧24bpp BMP恢复为8bpp；各图所有RGB像素逐点相同，未改原图内容或调色板。

| 成员 | 尺寸 | 保留palette0像素 | 新BMP字节 |
| --- | --- | --- | --- |
| icons/6helm1v2.bmp | 40×36 | 720 | 2518 |
| icons/6helm1v4.bmp | 47×43 | 1169 | 3142 |
| icons/6helm1v5.bmp | 37×34 | 659 | 2438 |

数值依据为外层`local-data/validation/equipment-indexed-family-254/`中的`source-inspection.json`、`image-conversion-001.json`及`runtime-files-001.json`，仅引用已完成记录，没有重复扫描或验证。
完整254/runtime-001位于该目录的`runtime-001/`，基于253只换上述3个BMP，17396个其他engine成员及四world保留，组装9.721012秒完成；Ui233／Combat223／导航222／Save106不变。
[engine-overlay-254.zip](engine-overlay-254.zip)为7394字节，相对完整246/runtime-002累计包含6helm1v1／v2／v4／v5四图，已含253的v1；不是完整runtime或新Release，摘要见受限结果。

新增3图只计原源／数值及资源范围，没有实际像素或触控验收；1058张色键结论也不是全部装备UI通过。45个既有missing引用被排除，未在本批补齐。
Mac010始终使用253程序／253资源，已正常退出、PID不存在、日志收集，4／4私有档SHA完全不变，修后零写档；254组装没有更新该进程。
首幅头盔透明画面通过范围不变。后续普通走路误入喷泉／遇伤死亡，正常复活并经UI重读槽3后又死亡，未到训练营；本轮未主动Attack／偷窃／施法。
255正在定点只读诊断，该死亡观察不是新产品缺陷证明，学习界面仍为源码／构建范围；没有额外单元运行、构建或设备操作。
固定发布与原存档保留，真机调试继续暂停；子代理保持`gpt-6.1-sol/max`，无设备／模拟器操作。
整体约88%仍为低置信度工程判断，完整自然战役与全部离线工作完成均未证明。
