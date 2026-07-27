import argparse
import os
import re
import shutil
import sys
from datetime import datetime
from pathlib import Path

BOARD_START = "# >>> T-IMPULSE-PLUS BOARD START >>>"
BOARD_END = "# <<< T-IMPULSE-PLUS BOARD END <<<"
REQUIRED_LINKER_FLAG = "-lstdc++"


def version_key(path):
    parts = re.split(r"[^0-9]+", path.name)
    return tuple(int(part) for part in parts if part)


def find_installed_cores():
    local_app_data = Path.home() / "AppData" / "Local"
    arduino15 = Path(
        os.environ.get("LOCALAPPDATA", str(local_app_data))) / "Arduino15"
    core_root = arduino15 / "packages" / "adafruit" / "hardware" / "nrf52"
    if not core_root.is_dir():
        return []
    return sorted(
        [path for path in core_root.iterdir() if path.is_dir()],
        key=version_key,
        reverse=True,
    )


def select_core(cores, requested_version, assume_yes):
    if requested_version:
        for core in cores:
            if core.name == requested_version:
                return core
        raise RuntimeError(
            f"Adafruit nRF52 version {requested_version} is not installed.")

    if len(cores) == 1 or assume_yes:
        return cores[0]

    print("Installed Adafruit nRF52 versions:")
    for index, core in enumerate(cores, start=1):
        print(f"  {index}. {core.name} ({core})")

    while True:
        choice = input("Select the version to configure [1]: ").strip() or "1"
        if choice.isdigit() and 1 <= int(choice) <= len(cores):
            return cores[int(choice) - 1]
        print("Invalid selection.")


def validate_core(core):
    required_paths = [
        core / "boards.txt",
        core / "platform.txt",
        core / "variants",
        core / "cores" / "nRF5" / "linker" / "nrf52840_s140_v6.ld",
    ]
    missing = [str(path) for path in required_paths if not path.exists()]
    if missing:
        raise RuntimeError(
            "The selected Adafruit nRF52 core is incompatible or incomplete. "
            "Missing:\n  " + "\n  ".join(missing))

    platform_text = (core / "platform.txt").read_text(
        encoding="utf-8", errors="replace")
    if "{compiler.libraries.ldflags}" not in platform_text:
        raise RuntimeError(
            "The selected core does not expose {compiler.libraries.ldflags} "
            "in its linker recipe. It cannot be configured safely by this tool.")


def backup_file(path, backup_dir):
    if not path.exists():
        return
    backup_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, backup_dir / path.name)


def replace_marked_block(text, block):
    pattern = re.compile(
        re.escape(BOARD_START) + r".*?" + re.escape(BOARD_END), re.DOTALL)
    if pattern.search(text):
        return pattern.sub(block.strip(), text)
    return text.rstrip() + "\n\n" + block.strip() + "\n"


def install_board(core, source_root, backup_dir):
    boards_path = core / "boards.txt"
    board_block = (source_root / "boards" / "t_impulse_plus_boards.txt").read_text(
        encoding="utf-8")
    boards_text = boards_path.read_text(encoding="utf-8", errors="replace")
    backup_file(boards_path, backup_dir)
    boards_path.write_text(
        replace_marked_block(boards_text, board_block), encoding="utf-8")

    source_variant = source_root / "variants" / "t_impulse_plus_nrf52840"
    target_variant = core / "variants" / "t_impulse_plus_nrf52840"
    if target_variant.exists():
        shutil.copytree(
            target_variant, backup_dir / target_variant.name,
            dirs_exist_ok=True)
    shutil.copytree(source_variant, target_variant, dirs_exist_ok=True)


def configure_linker(core, backup_dir):
    local_path = core / "platform.local.txt"
    backup_file(local_path, backup_dir)
    text = (local_path.read_text(encoding="utf-8", errors="replace")
            if local_path.exists() else "")
    lines = text.splitlines()
    setting_prefix = "compiler.libraries.ldflags="
    updated = False

    for index, line in enumerate(lines):
        if not line.strip().startswith(setting_prefix):
            continue
        current_flags = line.split("=", 1)[1].strip().split()
        if REQUIRED_LINKER_FLAG not in current_flags:
            current_flags.append(REQUIRED_LINKER_FLAG)
        lines[index] = setting_prefix + " ".join(current_flags)
        updated = True
        break

    if not updated:
        if lines and lines[-1].strip():
            lines.append("")
        lines.append(setting_prefix + REQUIRED_LINKER_FLAG)

    local_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Install the T-Impulse-Plus board into Adafruit nRF52 Arduino core.")
    parser.add_argument(
        "--version", help="Adafruit nRF52 version to configure, for example 1.6.1")
    parser.add_argument(
        "--core-path", type=Path,
        help="Explicit Adafruit nRF52 core directory; overrides auto-detection")
    parser.add_argument(
        "--yes", action="store_true",
        help="Select the newest installed version without prompting")
    return parser.parse_args()


def main():
    args = parse_args()
    source_root = Path(__file__).resolve().parent

    if args.core_path:
        core = args.core_path.expanduser().resolve()
    else:
        cores = find_installed_cores()
        if not cores:
            raise RuntimeError(
                "No Adafruit nRF52 core was found under Arduino15. Install it "
                "from Arduino Boards Manager first.")
        core = select_core(cores, args.version, args.yes)

    validate_core(core)
    print(f"Configuring Adafruit nRF52 core: {core}")

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_dir = source_root / "backup" / f"nrf52_{core.name}_{timestamp}"
    install_board(core, source_root, backup_dir)
    configure_linker(core, backup_dir)

    print("\nT-Impulse-Plus Arduino board installation successful.")
    print(f"Backup directory: {backup_dir}")
    print("Restart Arduino IDE and select:")
    print("  Adafruit nRF52 Boards > LilyGo T-Impulse Plus nRF52840")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(f"\nInstallation failed: {error}", file=sys.stderr)
        sys.exit(1)
