# 244：首次自然进入MM6商店的Mac短流程

状态为`ACCEPTED_OFFLINE_MAC_SCOPED`：自然Mac商店流程及Cocoa拖离误购买修复已受限通过，不作目标iPad触控验收，见[受限结果](scoped-result.json)和[源码摘要](source-sha256.json)。
初次流程使用243源码`d956e8cbba637e07a2da30c2752288d73bb93d91`及完整241资源，006正常读取本机隔离槽1／Save106，步行进入MM6 House62常规防御商店，food5、gold190。

普通库存中右键检查第二个头盔，中文报价90。通过CUA将商品拖向右侧空白时意外购买，gold190→100，头盔从商店库存消失并进入行囊。
随后正常出售该新头盔：右键报价17，正常左键点击后gold100→117，头盔从行囊消失。该观察不把误购买计为拖离取消通过。
全程零存档写入，原Save10本地原件、隔离副本及新槽1摘要均未变；006正常双击Quit后已退出并收集。私有档不进入公开交付。

源码确认Cocoa在显式Dragged事件前，可能使用独立全局按钮快照与旧XY合成UP；本次没有记录该全局值，不能声称实际已捕获这一分支。
冻结修复只涉及[PatchSdlCocoa.cmake](../../cmake/PatchSdlCocoa.cmake)追加独立的[sdl-cocoa-drag-button-state.patch](../../cmake/patches/sdl-cocoa-drag-button-state.patch)。
新patch仅在唯一调用点对绝对模式的Left／Right／OtherMouseDragged跳过按钮reconcile；既有release、position补丁、相对模式及普通motion／enter路径保留。

首次原生构建2.938982秒通过，仅重建Cocoa源对象、SDL静态库及可执行文件；2个改动文件的冻结SHA一致，3个补丁对实际依赖的`reverse --check`均通过。
实际SDL_cocoamouse源码SHA256为`b0f5908df31fb5a3f74fa79467abd47577117d783aad78180dd9fbc8a63a6d10`，
Mac可执行文件SHA256为`0c21b2ca405726a1f4504386b57dcffb2b7c02e9940550cbbf9f38f366d22af0`。
提交前diff检查指出patch的空上下文行带尾随空格；仅删去三个尾部上下文行并修正hunk计数，生产增删行不变，
实际SDL源码摘要及修后程序不变，新patch反向检查通过；原始构建输入及最终发布文件摘要分列保留，不重复构建或实际检查。

修后007／PID49181正常读取同一槽1／106，gold190、food5，再步行进入同一House62，右键检查同头盔仍报价90。
在本次1280截图坐标将商品从(440,210)拖到右侧空白(1040,500)，实际取消购买，gold190且商品仍在。
紧接普通点击该头盔成功购买，gold190→100、库存商品消失，中文购买状态可见；Escape后打开行囊确认新头盔存在。
修后零写档，原Save10本地原件、隔离副本及新槽1摘要全部未变。007仍由主代理持有，准备后续普通NPC流程，不能记为已退出或已验收NPC。
画面仅经内联CUA复核，没有导出截图文件。

该修改受Mac平台guard限制，不重建iOS，沿用243通用未签名iOS产物；零单元运行、零新增case，不回放旧路线或战役。
资源仍为完整241／UiText228（157原源＋71项目）／Combat223／导航222，Save106不变，没有资源、schema或存档版本变更。
没有设备、模拟器操作或新Release，固定229／214／195／RC1–3及全部原存档继续保留。
当前真机调试暂停，子代理保持`gpt-6.1-sol/max`，主代理独占构建、App、集成与Git。
本次Mac观察不证明目标iPad触控、全部商店行为或完整自然通关；整体约88%仍是低置信度工程判断，完整目标未完成。
