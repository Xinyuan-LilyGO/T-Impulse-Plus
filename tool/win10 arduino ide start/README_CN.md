# Windows Arduino IDE 安装 T-Impulse-Plus 开发板

## 准备工作

1. 安装 [Python 3](https://www.python.org/downloads/)。
2. 在 Arduino IDE 开发板管理器中安装 **Adafruit nRF52 Boards**。
3. 运行配置脚本前关闭 Arduino IDE。

## 自动配置

在当前目录打开命令提示符并运行：

```bat
python t_impulse_plus_arduino_setup.py
```

脚本会自动扫描以下目录中的全部已安装版本：

```text
%LOCALAPPDATA%\Arduino15\packages\adafruit\hardware\nrf52\
```

如果安装了多个版本，按提示选择 Arduino IDE 当前使用的版本。也可以明确指定：

```bat
python t_impulse_plus_arduino_setup.py --version 1.6.1
```

如果 Adafruit nRF52 Core 安装在其他目录，可以指定完整路径：

```bat
python t_impulse_plus_arduino_setup.py --core-path "D:\path\to\nrf52\version"
```

脚本会执行以下操作：

- 安装具有 48 引脚恒等映射的 `t_impulse_plus_nrf52840` variant；
- 在 `boards.txt` 中注册 **LilyGo T-Impulse Plus nRF52840**；
- 通过专属 variant 提供 `Wire1` 和 `Serial2`；
- 创建或更新 `platform.local.txt`，加入 `-lstdc++`；
- 在本目录的 `backup` 文件夹中备份被修改的文件。

安装完成后重启 Arduino IDE，并选择：

```text
工具 > 开发板 > Adafruit nRF52 Boards > LilyGo T-Impulse Plus nRF52840
```

推荐选项：

```text
SoftDevice: S140 6.1.1
Debug Level: Level 0 (Release)
Debug Port: Serial
```

> 升级或重新安装 Adafruit nRF52 开发板包后，需要重新运行此脚本。所选
> Core 版本必须包含 `nrf52840_s140_v6.ld`，并且链接规则支持
> `compiler.libraries.ldflags`。
