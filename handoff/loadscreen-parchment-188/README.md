# 侧栏纹理与皮肤包导入：checkpoint 188

实际读档页持续报告缺失 UI-Parch。原版真实侧栏纹理未被收集，且所选皮肤的伴随
`.icons.UI.lod` 未按原版顺序覆盖。现已补齐动态必需引用及伴随包优先级；不将 UI-Parch
替换为不同的 parchment 图片。原问题只影响侧栏和日志，不阻断中央存档列表或读取操作。

UI3导入新增1张ui-parch，按原包覆盖6张边框图，其余437个本次UI导入成员不变；完整包其他
成员和四个world ZIP保留。原版左右装饰图宽度分别恢复为32和33，未另做HUD矩阵。

原生导入器、原源转换、完整资源装配和未签名通用iOS Release通过。一次约37秒Mac全屏
菜单进入读档页，侧栏纹理可见，缺失纹理告警为0；Cmd+Q正常退出。只检查1张目标截图，
没有读档、重打战役或新增单元测试。Save103和游戏逻辑不变；没有新iPad实际验收。

新资源见 [188资源补丁](https://github.com/Mavskero/openyamm/releases/tag/ipados-ui-resources-188-20261029)：
下载engine.zip和conversion.json，配合原RC2四个world ZIP。不需要重新下载四个世界。
此engine包已包含183建队背景与移动按钮补丁；旧183和RC2发行物保留。未来从原始游戏数据
重新导入时需使用本次导入器源码；仅使用预转换ZIP时，该资源修复不要求新的游戏逻辑。

详见 [限定结果](scoped-result.json) 与 [完整资源清单](runtime-manifest.json)。
原始日志和图像在原工作区 `local-data/validation/loadscreen-parchment-188/`。
整体工程粗估仍80–85%，完整目标保持active。
