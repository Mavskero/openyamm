# RC3 独立交付：checkpoint 189

RC2独立候选仍停在182，后续183–188的输入、卷宗、问题和资源修复尚未收入可直接使用的独立包。RC3将现有修复、最新完整资源、独立用户目录及固定源码统一交付；主交接文档的下载命令也已改用188 engine/conversion与RC2四个world ZIP。

当前版本为0.1.0-rc3-20261029，Save103。生产实现基线为 `8f91f2b9276b649c3529fae049beaf91a5668996`；固定源码归档提交为 `09ae801f432c8772230b37ae512392f3c2f94346`，此后的189交付记录提交只补元数据，不改变归档中的生产程序。

[RC3下载](https://github.com/Mavskero/openyamm/releases/tag/ipados-rc3-20261029)提供Mac启动ZIP、未签名iPadOS ZIP、源码归档及清单/恢复说明。Mac启动ZIP不含运行资源，完整下载路径见仓库根目录 `IPADOS_HANDOFF.md`。本地完整候选保留于 `releases/first-playable-20261029-rc3/`；原RC1/RC2及失败记录不改动。

沿用187原生程序与188通用iOS构建。本轮没有生产代码修改或重复构建，只做一次27.763秒新启动器→相对资源→1280×800菜单→Cmd+Q退出，退出码0；没有进入新游戏或读取存档。源码归档2,088个文件恢复核对通过。交付存档为空；iOS副本排除旧签名和provisioning文件，原构建目录保持不变。

Mac预编译程序要求Apple Silicon/macOS26或更高。其他Mac系统需自行调整部署目标构建，尚无该路径验收。iOS需接收Mac有效签名安装，当前电脑仍不操作设备或模拟器。183–185、188目标iPad受影响界面确认待接收端继续；不增加旧战役回放任务。

见 [限定结果](scoped-result.json)、[交付版本](VERSION.json)、[源码恢复核对](source-restore-result-001.json)和[附件清单](download-manifest.json)。原日志、唯一菜单截图与恢复出的源码留在原工作区 `local-data/validation/third-playable-candidate-189/`。源码恢复不等于新机器构建或运行通过；完整自然战役仍未证明，工程估算保持80–85%。
