#!/usr/bin/env python3
"""Provision, build, and launch the Mega Man X4 port."""

from __future__ import annotations

import os
import sys
from collections.abc import Sequence
from pathlib import Path

from port import launcher
from port.launch_environment import player_environment
from resolve_disc import ENV_KEY

ROOT = Path(__file__).resolve().parents[1]


def launch(disc: str | None, prepare_only: bool) -> None:
    environment = dict(os.environ)
    if disc:
        environment[ENV_KEY] = disc
    launcher.preflight()
    launcher.run([sys.executable, "tools/extract_exe.py", *([disc] if disc else [])], environment, cwd=ROOT)
    build = launcher.build_product(ROOT, "megamanx4_port", environment)
    if prepare_only:
        launcher.say("Mega Man X4 is built and ready.")
        return
    launcher.exec_product(build / "bin" / "megamanx4_port", [],
                          player_environment(environment, product="megamanx4"), ROOT)


def main(argv: Sequence[str] | None = None) -> int:
    args = launcher.argument_parser(__doc__).parse_args(argv)
    return launcher.main(lambda: launch(args.disc, args.prepare_only))


if __name__ == "__main__":
    raise SystemExit(main())
