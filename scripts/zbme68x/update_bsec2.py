# Copyright (c) 2025 Chris Duf
# SPDX-License-Identifier: Apache-2.0
#
# Update Bosch-BSEC2-Library:
# - update BSEC2 API header files
# - update IAQ configurations
# - update BSEC2 blobs definitions in Zephyr module file.

import argparse
import logging
import sys

from pathlib import Path

from zbme68x import BME68X_ZEPHYR_PATH, cp, git_describe_tag, continue_yes_no
from zbme68x.bsec2_iaq_configs import update_iaq_configurations
from zbme68x.bsec2_algo_blobs import AlgobsecBlob, update_module_manifest

_log = logging.getLogger("zbme68x")


def update_bsec2_include_files(bsec2_base_dir: Path) -> bool:
    """Update BSEC2 API header files.

    Args:
        bsec2_base_dir: Absolute path to Bosch-BSEC2-Library local repository

    Returns: True if all header files are successfully updated.
    """
    inc_src_dir = bsec2_base_dir / "src" / "inc"
    inc_dest_dir = BME68X_ZEPHYR_PATH / "lib" / "bsec" / "include"

    for h_src, h_dest in [(h, inc_dest_dir / h.name) for h in inc_src_dir.glob("*.h")]:
        _log.debug(f"{h_src.name} -> {h_dest}")
        if not cp(h_src, h_dest):
            return False
    return True


if __name__ == "__main__":
    cliparser = argparse.ArgumentParser(
        prog="update_bsec2",
        description="Update Bosch-BSEC2-Library (headers, IAQ configurations, blobs)",
    )
    cliparser.add_argument(
        "bsec2_base_dir",
        help="path to Bosch-BSEC2-Library local repository",
        nargs=1,
        metavar="BSEC2_LIBRARY_DIR",
    )
    cliargs = cliparser.parse_args()

    bsec2_base_dir = Path(cliargs.bsec2_base_dir[0]).resolve()
    bsec2_git_tag = git_describe_tag(bsec2_base_dir)

    if not bsec2_git_tag:
        print("not a valid repository: {bsec2_base_dir}", sys.stderr)
        exit(1)

    print(f"bme68x-zephyr: {BME68X_ZEPHYR_PATH}")
    print(f"Bosch-BSEC2-Library: {bsec2_base_dir}")
    print(f"Version: {bsec2_git_tag}")
    if not continue_yes_no():
        exit(0)

    print("updating BSEC2 API headers ...")
    if not update_bsec2_include_files(bsec2_base_dir):
        exit(1)

    iaq_cfgs_src_dir = bsec2_base_dir / "src" / "config" / "bme680"
    iaq_cfgs_dest_dir = BME68X_ZEPHYR_PATH / "lib" / "bme68x-iaq" / "config" / "bme680"

    print("updating BSEC2 IAQ condifurations ...")
    if not update_iaq_configurations(iaq_cfgs_src_dir, iaq_cfgs_dest_dir):
        exit(1)

    module_yml_path: Path = BME68X_ZEPHYR_PATH / "zephyr" / "module.yml"
    blobs: list[AlgobsecBlob] = AlgobsecBlob.gather(bsec2_base_dir, bsec2_git_tag)
    print(f"found {len(blobs)} BSEC2 blobs")
    print("updating manifest file ...")
    if not update_module_manifest(blobs, module_yml_path):
        exit(1)

    print("done.")
    exit(0)
