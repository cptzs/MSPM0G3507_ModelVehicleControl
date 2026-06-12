#!/usr/bin/env python3
r"""Patch IAR project entries for the registry-driven UI core migration.

This helper intentionally performs a small, deterministic text patch on
``basic.ewp`` instead of asking developers to edit the XML by hand.

What it changes:
  1. Adds ``$PROJ_DIR$\User_Application\UI`` to every IAR C include-path block.
  2. Replaces direct compilation of ``User_Application\user_ui.c`` with the
     transition wrapper ``User_Application\UI\user_ui_legacy_pages.c``.
  3. Adds a dedicated ``User_Application_UI`` group containing the new UI core,
     dispatch, input, route context and migrated page files.

Run from repository root:

    python tools/migrate_iar_ui_core.py

The script keeps ``Minimal_Bringup`` excluded for application UI files, matching
existing project semantics.
"""

from __future__ import annotations

from pathlib import Path

PROJECT_FILE = Path("basic.ewp")
UI_INCLUDE = r"$PROJ_DIR$\User_Application\UI"
LEGACY_UI_FILE = r"$PROJ_DIR$\User_Application\user_ui.c"
LEGACY_WRAPPER_FILE = r"$PROJ_DIR$\User_Application\UI\user_ui_legacy_pages.c"

UI_SOURCE_FILES = [
    r"$PROJ_DIR$\User_Application\UI\user_ui_core.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_core_state.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_dispatch.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_input.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_pages.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_route_context.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_page_motor.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_page_sensors.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_page_actuator.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_page_debug.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_page_route.c",
    r"$PROJ_DIR$\User_Application\UI\user_ui_page_unittest.c",
]


def _excluded_file_entry(path: str, indent: str = "        ") -> str:
    return (
        f"{indent}<file>\n"
        f"{indent}    <name>{path}</name>\n"
        f"{indent}    <excluded>\n"
        f"{indent}        <configuration>Minimal_Bringup</configuration>\n"
        f"{indent}    </excluded>\n"
        f"{indent}</file>"
    )


def _ui_group() -> str:
    entries = "\n".join(_excluded_file_entry(path) for path in UI_SOURCE_FILES)
    return (
        "    <group>\n"
        "        <name>User_Application_UI</name>\n"
        f"{entries}\n"
        "    </group>"
    )


def add_ui_include_path(text: str) -> str:
    if f"<state>{UI_INCLUDE}</state>" in text:
        return text

    anchor = "                    <state>$PROJ_DIR$\\User_Application</state>"
    replacement = anchor + f"\n                    <state>{UI_INCLUDE}</state>"
    if anchor not in text:
        raise RuntimeError("Could not find User_Application include-path anchor")
    return text.replace(anchor, replacement)


def switch_legacy_ui_entry(text: str) -> str:
    if LEGACY_WRAPPER_FILE in text:
        return text
    if LEGACY_UI_FILE not in text:
        raise RuntimeError("Could not find direct User_Application\\user_ui.c project entry")
    return text.replace(LEGACY_UI_FILE, LEGACY_WRAPPER_FILE, 1)


def add_ui_source_group(text: str) -> str:
    if "<name>User_Application_UI</name>" in text:
        return text

    anchor = "    <group>\n        <name>User_Devices</name>"
    if anchor not in text:
        raise RuntimeError("Could not find User_Devices group insertion anchor")
    return text.replace(anchor, _ui_group() + "\n" + anchor, 1)


def main() -> None:
    if not PROJECT_FILE.exists():
        raise SystemExit("basic.ewp not found; run this script from repository root")

    original = PROJECT_FILE.read_text(encoding="utf-8")
    patched = add_ui_include_path(original)
    patched = switch_legacy_ui_entry(patched)
    patched = add_ui_source_group(patched)

    if patched == original:
        print("basic.ewp already contains the UI core migration entries")
        return

    PROJECT_FILE.write_text(patched, encoding="utf-8")
    print("basic.ewp updated for registry-driven UI core migration")


if __name__ == "__main__":
    main()
