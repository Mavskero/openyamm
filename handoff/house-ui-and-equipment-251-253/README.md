# 251–253：房屋字幕与头盔显示

状态为`ACCEPTED_OFFLINE_MAC_SCOPED`，见[受限结果](scoped-result.json)和[源码摘要](source-sha256.json)，累计记录这一组工作，不为每个小修另建交接。
250基线`1bfefd6ff5fa1b970922a8e3e7072bd19503b25b`已推送，远端及提交内7份摘要核对通过。
251–252字幕及253头盔资源修复已构建，5份最终源码摘要经主代理before／after比对全部匹配；实际修后验收仅限010首个受影响装备画面，学习／122／1690仍为源码和构建范围。

初次009使用250可执行文件及246资源，正常读取本机隔离Save2，步行经TownHall到Janice大耳怪前哨任务，中文全文及正常给key已有观察。
通过正常UI给characterData1穿戴Mace66、armor84、helmet1691，发现头盔固定magenta矩形；该次为修前缺陷观察。
只经正常UI在新空槽3写档一次，使用默认名“新索匹格镇”；粘贴未进入编辑态，不能计中文自定名验收。
旧3份隔离存档副本SHA未变；009已正常Quit，PID确认退出、日志已收集，不公开私有档或路径。

251学习UI单cpp原生4.8234925秒／未签名iOS9.709751秒通过，零测试运行；未收到受影响字幕的修后实际复查结果。
252将所有LearnSkill接通用现有HouseServiceTextTable，guild／deck分别复用当前NPCText122／1690并保留fallback与门控，无新资源key；学习／122／1690仍只计源码和构建，未证明actual UI。
最终原生game＋equipment_picture_import构建176.964303秒、通用未签名iOS171.930220秒通过；未为本组新增或运行单元用例。

253用现有convertLodImages API单成员提取原6helm1v1为Indexed8，耗时0.010753秒，复用原图、未重绘。
39×33旧24bpp→新8bpp，1287个RGB像素逐点相同，恢复668个palette0索引（252,0,216）；新BMP为2398字节，
SHA256为`24051417a0f2f51e4d04c66dd06a438d655357ffb21705174aff2b05c9e420bc`。
完整253/runtime-001位于外层`local-data/validation/natural-mac-gameplay-251/runtime-001/`，组装6.454302秒通过；只替换`icons/6helm1v1.bmp`，其余17398个engine成员及四world保留。
[engine-overlay-253.zip](engine-overlay-253.zip)为1774字节，仅含这一成员，不是完整runtime或新Release。

010使用最新可执行文件`e915b9251ba5d7a4e866c850039279d161f5ed3a243db68b274b49e97fbc0f31`及完整253资源，正常读取槽3，inventory中1691头盔透明正常，armor84／mace66显示正确。
只计这一首个受影响画面，没有状态夹具或修后存档写入；其他装备runtime未在本批修复，不能称全部装备UI完成。
010在该受影响画面核对时停在inventory，随后继续普通游玩，进程仍由主代理运行，尚未退出或收集最终日志。4份隔离存档摘要在该画面核对后仍一致，不代替010最终收尾。

当前开发资源为完整253/runtime-001，UiText233、Combat223、导航222及Save106不变，固定发布与原存档保留，没有新Release。
真机／模拟器操作继续暂停；子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git。
任务证据限于全文与给key；整体约88%仍为低置信度工程判断，大耳怪前哨完整任务、完整自然战役和全部离线工作完成均未证明。
