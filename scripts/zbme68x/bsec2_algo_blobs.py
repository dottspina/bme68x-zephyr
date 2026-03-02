# Copyright (c) 2025 Chris Duf
# SPDX-License-Identifier: Apache-2.0
#
# Update BSEC2 blobs definition in zephyr/module.yml.
#
# Algorithm binaries are retrieved from relevant
# directories in Bosch-BSEC2-Library/src, e.g.
# - Bosch-BSEC2-Library/src/cortex-m4, target cortex-m4 (no FPU)
# - Bosch-BSEC2-Library/src/cortex-m4/fpv4-sp-d16-hard, target cortex-m4f (FPU)
#
# Don't forget to set BSEC2_LIBRARY_GIT_TAG before running this script.

import hashlib
import logging

from pathlib import Path

from zbme68x import BME68X_ZEPHYR_PATH, git_describe_tag, continue_yes_no


_log = logging.getLogger("zbme68x")


# Bosch-BSEC2-Library GitHub repository.
BSEC2_LIBRARY_GIT_BASE = "https://github.com/boschsensortec/Bosch-BSEC2-Library"

# Licensing terms:
# - URL from Bosch-BSEC2-Library/LICENSE.md is unreachable
# - URL from Bosch-BSEC2-Library/README.md#software-license-agreement is also unreachable
#
# Best guess (see https://www.bosch-sensortec.com/en/software-tools/software/bme680-software-bsec):
BSEC2_LICENSE_PATH = (
    "zephyr/blobs/bsec/20241219_clickthrough_license_terms_bsec_bme680_bme688_bme690.pdf"
)

# Relative paths to relevant BSEC2 algorithm blobs,
# i.e. directories that contain a libalgobsec.a static library for a relevant target.
# Path are relative to the Bosch-BSEC2-Library/src
# where Bosch-BSEC2-Library points to the library local repository.
LIBALGOBSEC_DIRS: list[Path] = [
    Path("cortex-m3"),
    Path("cortex-m33"),
    Path("cortex-m33", "fpv5-sp-d16-hard"),
    Path("cortex-m4"),
    Path("cortex-m4", "fpv4-sp-d16-hard"),
    Path("esp32"),
    Path("esp32s2"),
    Path("esp32s3"),
    Path("esp32c3"),
]

MODULE_YML_HEADER = """# Copyright (c) 2025, Chris Duf
#
# SPDX-License-Identifier: Apache-2.0

name: bme68x

build:
  cmake: .
  kconfig: Kconfig
  settings:
    dts_root: .
blobs:
"""


class AlgobsecBlob:
    """Manifest metadata defining to declare BSEC2 static libraries as Zephyr blobs."""

    _target: str
    _sha256: str
    _tag: str
    _url: str

    @classmethod
    def gather(cls, bsec2_base_dir: Path, release: str) -> list["AlgobsecBlob"]:
        """Retrieve relevant BSEC2 blobs.

        Args:
            bsec2_base_dir: Path to Bosch-BSEC2-Library.
            release: BSEC2 release names and Git tags have the form major.minor.algo,
              e.g. 1.10.2610 means library release 1.10 and algorithm version 2.6.1.0.
        """
        return [AlgobsecBlob(bsec2_base_dir, blob_dir, release) for blob_dir in LIBALGOBSEC_DIRS]

    def __init__(self, bsec2_base_dir: Path, blob_dir: Path, tag: str) -> None:
        """Initialize BSEC2 blob meta-data."""
        self._tag = tag
        self._url = self._get_url(blob_dir)
        self._target = self._get_target(blob_dir)
        self._sha256 = self._get_sha256(bsec2_base_dir, blob_dir)

    @property
    def target(self) -> str:
        """Target, e.g. cortex-m4 or cortex-m4f (FPU)."""
        return self._target

    @property
    def path(self) -> str:
        """Path, e.g. bsec/cortex-m4f/libalgobsec.a."""
        return f"bsec/{self._target}/libalgobsec.a"

    @property
    def sha256(self) -> str:
        """Checksum verified by the `west blobs fetch` command."""
        return self._sha256

    @property
    def url(self) -> str:
        """Download URL for the `west blobs fetch` command."""
        return self._url

    @property
    def version(self) -> str:
        """Bosch-BSEC2-Library release tag.

        E.g. 1.10.2610 for Bosch-BSEC2-Library release 1.10,
        and BSEC2 version algorithm 2.6.1.0.
        Used in blob GitHub URL.
        """
        return self._tag

    def dump(self) -> None:
        _log.debug(f"{self.target.upper()}")
        _log.debug(f"  path:    {self.path}")
        _log.debug(f"  version: {self.version}")
        _log.debug(f"  sha256: {self.sha256}")
        _log.debug(f"  URL:    {self.url}")

    def _get_target(self, blob_dir: Path) -> str:
        return f"{blob_dir.parent.name}f" if blob_dir.name.startswith("fp") else blob_dir.name

    def _get_sha256(self, bsec2_base_dir: Path, blob_dir: Path) -> str:
        libalgobsec = bsec2_base_dir / "src" / blob_dir / "libalgobsec.a"
        try:
            with open(libalgobsec, "rb") as libalgobsec:
                return hashlib.file_digest(libalgobsec, "sha256").hexdigest()
        except OSError as e:
            _log.error(f"{blob_dir}: no sha256: {e}")
        # Empty sha256 sum.
        return ""

    def _get_url(self, blob_dir: Path) -> str:
        return f"{BSEC2_LIBRARY_GIT_BASE}/raw/{self.version}/src/{blob_dir}/libalgobsec.a"


def update_module_manifest(blobs: list[AlgobsecBlob], module_yml_path: Path) -> bool:
    try:
        with open(module_yml_path, "w") as module_yml_file:
            module_yml_file.write(f"{MODULE_YML_HEADER}")
            for blob in blobs:
                module_yml_file.write(f"  # {blob.target.upper()}\n")
                module_yml_file.write(f"  - path: {blob.path}\n")
                module_yml_file.write(f"    sha256: {blob.sha256}\n")
                module_yml_file.write("    type: lib\n")
                module_yml_file.write(f'    version: "{blob.version}"\n')
                module_yml_file.write(f"    license-path: {BSEC2_LICENSE_PATH}\n")
                module_yml_file.write("    description: BSEC2 algorithm\n")
                module_yml_file.write(f"    doc-url: {BSEC2_LIBRARY_GIT_BASE}\n")
                module_yml_file.write(f"    url: {blob.url}\n")
        return True
    except OSError as e:
        _log.error(f"failed to create manifest: {e}")
    return False


if __name__ == "__main__":
    import argparse

    cliparser = argparse.ArgumentParser("bsec2_algo_blob")
    cliparser.add_argument(
        "bsec2_library_root",
        help="path to Bosch-BSEC2-Library directory",
        nargs=1,
        metavar="BSEC2_LIBRARY_DIR",
    )
    cliargs = cliparser.parse_args()

    module_yml_path: Path = BME68X_ZEPHYR_PATH / "zephyr" / "module.yml"
    bsec2_base_dir: Path = Path(cliargs.bsec2_library_root[0])
    bsec2_git_tag = git_describe_tag(bsec2_base_dir)

    print(f"BSEC2-Library directory: {bsec2_base_dir}")
    print(f"BSEC2 release: {bsec2_git_tag}")
    print(f"Manifest file: {module_yml_path}")

    if not continue_yes_no():
        exit(0)

    blobs: list[AlgobsecBlob] = AlgobsecBlob.gather(bsec2_base_dir, bsec2_git_tag)
    _log.info(f"gathered {len(blobs)} BSEC2 blobs")
    for blob in blobs:
        blob.dump()

    _log.info("updating manifest")
    update_module_manifest(blobs, module_yml_path)
