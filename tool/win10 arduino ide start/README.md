# Install the T-Impulse-Plus board for Arduino IDE on Windows

## Requirements

1. Install [Python 3](https://www.python.org/downloads/).
2. Install **Adafruit nRF52 Boards** from Arduino IDE Boards Manager.
3. Close Arduino IDE before running the setup script.

## Automatic setup

Open Command Prompt in this directory and run:

```bat
python t_impulse_plus_arduino_setup.py
```

The script scans all versions installed under:

```text
%LOCALAPPDATA%\Arduino15\packages\adafruit\hardware\nrf52\
```

If more than one version is installed, select the version used by Arduino IDE.
You can also select it explicitly:

```bat
python t_impulse_plus_arduino_setup.py --version 1.6.1
```

To configure a core in another directory:

```bat
python t_impulse_plus_arduino_setup.py --core-path "D:\path\to\nrf52\version"
```

The script:

- installs the `t_impulse_plus_nrf52840` variant with 48-pin identity mapping;
- registers **LilyGo T-Impulse Plus nRF52840** in `boards.txt`;
- enables `Wire1` and `Serial2` through the dedicated variant;
- creates or updates `platform.local.txt` with `-lstdc++`;
- backs up changed files in this directory's `backup` folder.

After installation, restart Arduino IDE and select:

```text
Tools > Board > Adafruit nRF52 Boards > LilyGo T-Impulse Plus nRF52840
```

Recommended options:

```text
SoftDevice: S140 6.1.1
Debug Level: Level 0 (Release)
Debug Port: Serial
```

> Re-run this script after upgrading or reinstalling the Adafruit nRF52 board
> package. The selected core version must include `nrf52840_s140_v6.ld` and a
> linker recipe that supports `compiler.libraries.ldflags`.
