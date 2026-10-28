# OpenYAMM 首版候选 RC2

版本：**0.1.0-rc2-20261028**，存档格式：**Save103**。

本目录提供 Apple Silicon Mac 程序、完整运行资源、未签名 iPadOS 应用和源码恢复材料。RC1 原包另行保留。候选包实际检查状态见 [VERSION.json](VERSION.json)，已验证范围与限制见 [KNOWN_LIMITS.md](KNOWN_LIMITS.md)。

## 在 Mac 上开始

1. 将整个候选目录放在可写位置，双击 **Start-Mac.command**。
2. 在菜单中新建游戏，或按下文复制已有 OpenYAMM 存档后读取。
3. 默认以 **1280×800 窗口模式**启动；按 **F9 保存、F10 读取**。

请一起保留 `Start-Mac.command`、`macOS/`、`assets/` 和 `userdata/`。需要搬移时请保留整个目录；尚未在搬移后实跑。单独搬走 `OpenYAMM.app` 无法保留所需资源与配置。

默认使用现代控制：

| 操作 | 按键 |
|---|---|
| 移动、转向 | W/A/S/D；鼠标转向 |
| 显示指针 | 按住鼠标右键 |
| 攻击、交互 | 鼠标左键；E；Space 与近处目标交互 |
| 跳跃、法术、快速施法 | X；C；Q |
| 回合模式、切换成员 | Enter；Tab |
| 背包、历史、地图、休息 | I；H；L；R |
| 快速存档、快速读档 | F9；F10 |

## 设置与旧存档

- 设置保存在 `userdata/settings.ini`，首次启动生成，后续保留。
- 存档位于 `userdata/saves/`，交付时为空，不含测试队伍或受控流程存档。
- 续玩前退出程序，将已有 OpenYAMM `.oysav` **复制**到 RC2 的 `userdata/saves/`，保留原件并避免覆盖同名文件。
- 旧档读取时在内存中迁移；再次保存会写成 **Save103**。RC1 使用 Save102，请不要用 RC1 读取 RC2 新写出的存档。

Save103 记录永久遣散后可重新生成角色的招募槽；旧档缺少这项记录时按空集合迁移，不会推断旧角色已被永久遣散。兼容范围是 OpenYAMM `.oysav`，不承诺读取原 Windows 游戏或 MMMerge 存档，也未实测所有旧档组合。

本版增加了 Inn 中已离队角色的永久遣散确认及槽位回收，并补充 BrAlvar 交互、局部角色行为与脚步映射等内容。受原源保护的角色仍不能永久遣散；各项验证边界见 [KNOWN_LIMITS.md](KNOWN_LIMITS.md)。

## iPadOS 与资源

`iPadOS-unsigned/openyamm_ipados.app` 需要有效签名后才能安装。安装后选择“选择游戏资源目录”，导入整个 `assets` 文件夹，再点“开始游戏”（或“继续测试现有内容”）。

```text
assets/
  engine.zip
  worlds/
    mm6.zip
    mm7.zip
    mm8.zip
    mmmerge.zip
```

不要只选单个 ZIP。资源导入不会导入 `.oysav` 存档。当前没有 iPad 真机或模拟器验收，安装、资源导入、触控、生命周期、媒体和性能尚未确认。

## 源码与范围

源码快照位于 `source/`，恢复步骤见 [source/RESTORE.md](source/RESTORE.md)；运行资源独立位于 `assets/`。`evidence/` 与 `VERSION.json` 记录本候选的构建、资源和限定检查结果。

本候选尚无完整自然战役或主观音频体验验收。原始游戏资源仅用于本地交付，不作公开发布。
