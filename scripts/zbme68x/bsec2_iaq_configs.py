# Copyright (c) 2025 Chris Duf
# SPDX-License-Identifier: Apache-2.0
#
# Update available BSEC2 IAQ configurations in lib/bme68x-iaq
#
# Configurations are retrieved from Bosch-BSEC2-Library/src/config/bme680.
# The Bosch-BSEC2-Library repository publishes the binary configurations as one-line
# comma-separated CSV files:
# - the first integer is the configuration size in bytes
# - followed by the configuration bytes
# For each IAQ configuration, create/update the C files needed for integration.
#
# Note: sub-directories of Bosch-BSEC2-Library/src/config/bme688 aren't IAQ configurations.

import logging
import sys

from pathlib import Path

from zbme68x import BME68X_ZEPHYR_PATH, mkdirs, continue_yes_no


_log = logging.getLogger("zbme68x")


class BsecIaqConfig:
    name: str
    size: int
    data: list[int]

    @classmethod
    def from_dir(cls, root_dir: Path) -> list["BsecIaqConfig"]:
        """Retrieve all BSEC2 IAQ configurations under directory.

        Args:
          root_dir: Root directory of IAQ configurations,
            typically Bosch-BSEC2-Library/src/config/bme680
        """
        iaq_cfgs: list[BsecIaqConfig] = [
            BsecIaqConfig(iaq_cfg_dir) for iaq_cfg_dir in root_dir.iterdir()
        ]
        return iaq_cfgs

    def __init__(self, iaq_cfg_dir: Path) -> None:
        """Initialize BSEC2 IAQ configuration from CSV file.

        Args:
            iaq_cf_dir: BSEC2 IAQ configuration directory,
              e.g. bme680_iaq_33v_300s_4d
        """
        self.size = 0
        self.data = []
        self.name = iaq_cfg_dir.name
        self._init_data(iaq_cfg_dir)

    def create_bsec_iaq_h(self, dest_dir: Path) -> bool:
        """Generate bsec_iaq.h for this configuration.

        Args:
            dest_dir: Destination directory.
        """
        bsec_iaq_h_path = dest_dir / "bsec_iaq.h"
        try:
            with open(bsec_iaq_h_path, "w") as bsec_iaq_h_file:
                bsec_iaq_h_file.write("#include <stdint.h>\n")
                bsec_iaq_h_file.write("\n")
                bsec_iaq_h_file.write(f"extern const uint8_t bsec_config_iaq[{self.size}];\n")

                _log.info(f"created {bsec_iaq_h_path}")
                return True
        except OSError as e:
            _log.error(f"failed to create header file: {e}")
        return False

    def create_bsec_iaq_c(self, dest_dir: Path) -> bool:
        """Generate bsec_iaq.c for this configuration.

        Args:
            dest_dir: Destination directory.
        """
        bsec_iaq_c_path = dest_dir / "bsec_iaq.c"
        bsec_iaq_hex = [f"0x{x:02X}" for x in self.data]

        try:
            with open(bsec_iaq_c_path, "w") as bsec_iaq_c_file:
                bsec_iaq_c_file.write('#include "bsec_iaq.h"\n')
                bsec_iaq_c_file.write("\n")
                bsec_iaq_c_file.write(f"const uint8_t bsec_config_iaq[{self.size}] = {{\n")

                offset = 0
                while offset < len(self.data):
                    data = bsec_iaq_hex[offset : offset + 16]
                    bsec_iaq_c_file.write(", ".join(data))
                    bsec_iaq_c_file.write(",\n")
                    offset += 16
                bsec_iaq_c_file.write("};\n")

                _log.info(f"created {bsec_iaq_c_path}")
                return True
        except OSError as e:
            print(f"failed to create C file: {e}", sys.stderr)
        return False

    def _init_data(self, iaq_cfg_dir: Path) -> None:
        _log.debug(f"IAQ cfg dir: {iaq_cfg_dir}")
        csv_path: Path = iaq_cfg_dir / "bsec_iaq.csv"
        try:
            with open(csv_path, "r") as csv_file:
                csv_txt = csv_file.read()
        except IOError as e:
            _log.error(f"failed to open file: {e}")
        else:
            csv_data: list[int] = [int(data) for data in csv_txt.split(",")]
            size = csv_data[0]
            data = csv_data[1:]
            if size != len(data):
                _log.error(f"inconsistent file '{csv_path}', got {len(data)} bytes")
            else:
                self.size = size
                self.data = data
                _log.info(f"{self.name}: {self.size} bytes")

    def __len__(self) -> int:
        return self.size


def update_iaq_configurations(src: Path, dest: Path) -> bool:
    """Update BSEC2 IAQ configuration.

    Args:
        src: Root directory of IAQ configurations,
          typically Bosch-BSEC2-Library/src/config/bme680
        dest: Destination directory,
          typically bme68x-zephyr/lib/bme68x-iaq/config/bme680

    Returns: True if all configurations are successfully updated.
    """
    iaq_cfgs: list[BsecIaqConfig] = BsecIaqConfig.from_dir(src)
    for cfg in iaq_cfgs:
        path = dest / cfg.name
        if not all((mkdirs(path), cfg.create_bsec_iaq_h(path), cfg.create_bsec_iaq_c(path))):
            return False
    return True


if __name__ == "__main__":
    import argparse

    cliparser = argparse.ArgumentParser("bsec2_iaq_config")
    cliparser.add_argument(
        "bsec2_library_root",
        help="path to Bosch-BSEC2-Library directory",
        nargs=1,
        metavar="BSEC2_LIBRARY_DIR",
    )
    cliparser.add_argument(
        "dest_dir",
        help="destination directory (parent of bme680 and bme688)",
        nargs="?",
        metavar="DEST_DIR",
    )
    cliargs = cliparser.parse_args()

    iaq_cfgs_src_dir = Path(cliargs.bsec2_library_root[0]) / "src" / "config" / "bme680"
    if cliargs.dest_dir:
        iaq_cfgs_dest_dir = Path(cliargs.dest_dir)
    else:
        iaq_cfgs_dest_dir = BME68X_ZEPHYR_PATH / "lib" / "bme68x-iaq" / "config" / "bme680"

    print(f"From: {iaq_cfgs_src_dir}")
    print(f"To:   {iaq_cfgs_dest_dir}")

    if not continue_yes_no():
        exit(0)

    update_iaq_configurations(iaq_cfgs_src_dir, iaq_cfgs_dest_dir)
