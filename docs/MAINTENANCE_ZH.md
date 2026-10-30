# 中文版后续维护

这份说明和 `tools/`、`tests/local/` 是维护工具，整理目录时应保留。

## 更新方式

现在的中文 NRO 是用修改后的开源 C 源码编译得到的。后续应取得新版本对应的源码，迁移中文适配，再重新生成并编译；无需先反编译新版 NRO。

`tools/regen.sh` 处理的是你自己的 PC 版 `sh2pc.exe`：应用 EE 补丁、转换分析输入、把 x86 函数翻译成 C。之后由 devkitA64 编译成 Switch ARM64 的 NRO。

如果只有新 NRO 而没有对应源码，下面的工具只能检查版本、结构和哈希，不能自动把任意 NRO 反编译后汉化。此时先查找对应提交；二进制反汇编需要重新定位，不能直接复用旧版 NRO 的指令偏移。

## 现有工具

| 工具 | 用途 |
| --- | --- |
| `tools/convert_ee_font.py` | 把自己的中文 PNG 转为保留透明度的 RLE TGA，并逐像素校验 |
| `tools/build_zh.ps1` | Windows 下检查输入、启动固定版本 Docker 编译环境、生成源码、验证并构建 NRO |
| `tools/package_zh.py` | 读取 NRO 版本/哈希；检查字库；制作并校验精简包，拒绝覆盖现有输出 |
| `tests/local/path_driver.c` | 验证 lang 优先级、暂停菜单资源、大小写及存档路径 |
| `tests/local/font_driver.c` | 验证 C 字库解码、字宽、异常 RLE 和字形输出 |
| `tests/local/check_font_output.py` | 对照 Pillow 校验全部解码像素，并独立计算六个字形的期望像素；通过后清理大体积临时像素文件 |
| `tests/local/test_package_zh.py` | 使用合成文件测试 NRO 解析、精简包内容、可选语言设置及拒绝覆盖等保护 |
| `tests/local/png_font_driver.c`、`check_png_font.py` | 直接验证当前上游 PNG 字体函数的解码、字宽、高清缓存和回退；运行步骤见 CHINESE.md |
| `AGENTS.md` | 让后续接手的编码助手自动读到本项目的维护约定 |

## 先检查环境

在 PowerShell 中进入本仓库（目录移动后也可用，脚本不依赖旧的 E:\Game\sh2-nx-diagnosis 路径）：

```powershell
cd 'E:\Game\Silent_Hill_2\04_源码\sh2-nx'
.\tools\build_zh.ps1 -CheckOnly
```

需要 Git、Python 3、已启动的 Docker Desktop，以及固定编译镜像：

```powershell
docker pull devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

`-CheckOnly` 不安装依赖、不创建编译缓存。首次完整构建会在容器中按需要安装 python3-venv，在仓库 `.venv/` 建立 Linux 环境，并按上游 requirements.txt 安装依赖；实际版本记录到 `build/zh-python-requirements.txt`。它需要访问官方系统包/Python 包仓库。

保留以下私人输入，它们受 `.gitignore` 忽略：

- `game/sh2pc.exe`：5,685,248 字节，SHA-1 为 `3201a5e1029f3ae3b77770255dcbe10360b3cd09`。
- `game/lang/font/font000.tga` 和同包的 `fontwdata.bin`：供 native 字体验证使用。
- `game/lang/font/font000.png`：上游 PNG 字体路径的额外 native 验证输入。

原始完整游戏副本在总目录的 `01_游戏文件`。这些游戏文件和字体不提交到 Git。

## 跟进新版本源码

当前适配基于上游 `v1.02`，基线提交为 `95ae2f16efc09c82f405f04383a27d98c2fa804a`，分支为 `chinese-v1.02`。此前的 `chinese-v1.00` 保留在 Git 历史中。

v1.02 已有上游 PNG 高清字库和 lang 优先路径。当前先运行上游 PNG 字体初始化，只有 PNG 未能加载时才初始化旧 TGA 后备；两套绘制逻辑不能同时生效。日语槽暂停菜单映射仍需保留。

1. 确认当前工作已提交，运行 `git fetch upstream --tags`，查明新 NRO 对应的实际标签或提交。
2. 从当前中文分支创建新分支，再合并新版本对应的上游标签/提交；先不提交合并，审查冲突和适配行为。保留原分支作为 Git 内的基线，不需要另存历史源码目录。
3. 用 `git diff v1.02..chinese-v1.02 -- src tools CMakeLists.txt` 查看当前的最终适配差异。不要复制整个旧 src 覆盖上游新源码；如果上游已经实现某项功能，保留上游行为并审查旧补丁是否仍需要。迁移合并历史时不能机械地 cherry-pick 所有旧提交。
4. 更新 CMakeLists.txt 的本地版本号，例如新版本后追加 `-zh1`，以便与官方 NRO 区分。打包器要求这个标记；标记只用于识别，不能证明运行时汉化正确。
5. 核对下表中的适配点，完成构建和测试，再打包。

| 文件/位置 | 要保留或重新核对的行为 |
| --- | --- |
| `src/win32/kernel32.c` 的 rt_path | lang 优先于启用的 sh2e，最后回退 data；缺文件时回退正常 |
| 同文件的暂停菜单映射 | 语言槽 0 时优先 lang/etc/resource/r_menu_j.res |
| `src/game/ee_font.c/.h` | TGA 后备的完整解码、字宽、44×64 单元、两套样式、22×32 字形输出 |
| `src/game/ee.c` 的 ee_init | 先调用上游 ee_fonts_init；仅在 font_px 为空时调用 ee_font_init |
| `src/game/ee.c` 的 Custom fonts 部分 | 上游 PNG 解码、44×64 高清缓存和字宽，保留当前上游 hook |
| `src/d3d8/d3d8_res.c` 的 unlock_surface | 后备字库未初始化时 ee_font_overlay 不生效；重新检查 Res 字段和像素格式 |
| `CMakeLists.txt` | 新增 game 源文件实际参与构建；本地中文版本标签正确 |

当前代码使用的 PC 来宾地址：语言 `0x01DBC006`、字宽表指针 `0x008038A0`、字体纹理 `0x00943928`、当前字形槽 `0x01F5FEC0`、字形编号表 `0x01F5F560`。
这些是支持的 PC 可执行文件中的地址，不是新版 NRO 的文件偏移。如果上游换用另一份 PC exe，必须重新定位；build_zh.ps1 会先检查 exe 哈希，防止直接套用。

## 字库转换与构建

当前原始中文包提供 4400×9600 PNG 和 448 字节字宽表，v1.02 上游字体路径直接使用 PNG。TGA 用于兼容后备和后备测试；俄版残留 TGA 为 4400×896，不能和中文编号混用。已有正确中文 TGA 时无需再转换。

转换脚本需要 Pillow；可使用已安装 Pillow 的 Python：

```powershell
python -m pip install Pillow
python .\tools\convert_ee_font.py '中文包\lang\font\font000.png' '中文包\lang\font\font000.tga'
```

把匹配的 TGA/字宽表放入 `game/lang/font/` 后构建：

```powershell
.\tools\build_zh.ps1
```

默认重新生成 PC 函数、运行资源路径和字体验证，再编译 Switch NRO。Windows 签出脚本即使是 CRLF，也会在 build/ 生成 LF 副本运行，不修改跟踪的原文件。产物为 `build/switch/sh2-nx.nro`。

只有确认 exe、补丁和代码生成输入都没变化、生成结果仍有效时，才使用 `-SkipRegeneration`。迁移上游版本时使用完整构建。

## 精简打包

先检查一个现有 NRO：

```powershell
python .\tools\package_zh.py inspect '..\..\03_修复包\当前_v1.00\SH2_中文修复精简包_v1.00\switch\sh2-nx\sh2-nx.nro'
```

打包时指定重新构建的 NRO 和完整中文 lang 目录。下面输出名是示例，实际更新时使用对应版本名：

```powershell
python .\tools\package_zh.py pack --nro .\build\switch\sh2-nx.nro --lang '..\..\03_修复包\当前_v1.00\SH2_中文修复精简包_v1.00\switch\sh2-nx\lang' --output .\dist\SH2_中文修复精简包_新版本 --dry-run
# 核对后移除 --dry-run 正式生成文件夹和 ZIP。
```

默认包内只有 `switch/sh2-nx/sh2-nx.nro` 和 `lang/`，保留必需的 `fontwdata.bin`，不复制 keyconf.dat、settings.ini、sh2e.ini。需要附带语言槽设置时显式加 `--include-language-ini`，只会生成语言值 0 的 language.ini。
输出路径已存在时会报错，不覆盖当前交付包。打包会检查 NRO 结构、TGA/PNG 尺寸及字宽，逐文件核对复制结果、ZIP CRC 和 SHA-256。尺寸/标记检查不能代替完整字体解码或实机测试。

修改打包工具后可运行：`python -B -m unittest discover -s tests/local -p test_package_zh.py`。测试使用临时合成文件并自动清理。

Switch 上仍要确认语言槽为 0；遗留旧版 keyconf.dat 如与新 NRO 不匹配，应备份后移走或换成匹配的新版本设置。

## 交付前检查和可清理内容

在 Switch 上检查标题、选项、暂停菜单、字幕、存读档、中文字符、+ 键/奔跑、开场雾效及一段对话音频。编译和像素测试通过不等于已完成实机验证。

实机通过后更新 `03_修复包/当前版本`。可按需清理 `build/`、Docker 创建的 `.venv/` 和过时 `dist/`；保留当前源码及 .git、tools/、tests/、本说明和 AGENTS.md。
