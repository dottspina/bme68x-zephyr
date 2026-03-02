# Copyright (c) 2025 Chris Duf
# SPDX-License-Identifier: Apache-2.0

import logging.config
import os
import shutil
import subprocess
import sys

from pathlib import Path

BME68X_ZEPHYR_PATH: Path = Path(__file__).parent.parent.parent


try:
    logging.config.fileConfig(Path(__file__).parent / "logging.conf")
except Exception as e:
    print(f"invalid logging configuration: {e}", sys.stderr)

_log = logging.getLogger("zbme68x")


def continue_yes_no() -> bool:
    yes_no = input("Continue? [y/N] ")
    return yes_no.lower() in ["yes", "y"]


def cp(src: Path, dest: Path) -> bool:
    try:
        shutil.copy(src, dest)
        return True
    except OSError as e:
        _log.error(f"failed to copy file: {e}")
    return False


def mkdirs(path: Path) -> bool:
    try:
        os.makedirs(path, exist_ok=True)
        return True
    except OSError as e:
        _log.error(f"failed to create directory: {e}")
    return False


def git_describe_tag(bsec2_base_dir: Path) -> str:
    git_cmd: list[str] = [
        "git.exe" if os.name == "nt" else "git",
        "describe",
        "--exact-match",
        "--tags",
    ]
    try:
        with subprocess.Popen(
            git_cmd,
            cwd=bsec2_base_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        ) as proc:
            ret = proc.wait()
            # We know proc.stdout is set.
            output = proc.stdout.read().decode("utf-8").strip()  # type: ignore
            if ret == 0:
                return output
    except OSError as e:
        _log.error(f"failed to get BSEC2 tag: {e}")
    return ""
