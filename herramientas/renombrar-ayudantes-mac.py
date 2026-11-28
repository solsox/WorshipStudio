#!/usr/bin/env python3
"""Renames the Chromium helper bundles inside the built app so they carry the
name of the app.

Chromium looks for its helper processes by the name of the app it is inside
("<App> Helper (GPU).app"). The browser plugin is a submodule of OBS and
builds them as "OBS Helper...", so in WorshipStudio none of them started:
the GPU failed and Chromium closed the whole program, and the page of a
browser source never drew. Renaming them here keeps the fix in this repo,
where the submodule cannot carry it.

Use: python3 herramientas/renombrar-ayudantes-mac.py <ruta/WorshipStudio.app>
"""
import plistlib
import subprocess
import sys
from pathlib import Path


def sign(path):
    """Ad-hoc signature again, keeping the entitlements it already had."""
    subprocess.run(
        ["codesign", "--force", "--preserve-metadata=entitlements,flags", "--sign", "-", str(path)],
        check=True,
        capture_output=True,
    )


def rename_helper(helper, app_name):
    old_name = helper.name[: -len(".app")]
    suffix = old_name.split("Helper", 1)[1]  # "", " (GPU)", " (Plugin)"...
    new_name = f"{app_name} Helper{suffix}"
    if old_name == new_name:
        return False

    binary = helper / "Contents" / "MacOS" / old_name
    if not binary.is_file():
        raise SystemExit(f"No está el programa del ayudante: {binary}")
    binary.rename(helper / "Contents" / "MacOS" / new_name)

    info = helper / "Contents" / "Info.plist"
    with open(info, "rb") as f:
        plist = plistlib.load(f)
    plist["CFBundleExecutable"] = new_name
    plist["CFBundleName"] = new_name
    if "CFBundleDisplayName" in plist:
        plist["CFBundleDisplayName"] = new_name
    with open(info, "wb") as f:
        plistlib.dump(plist, f)

    renamed = helper.with_name(f"{new_name}.app")
    helper.rename(renamed)
    sign(renamed)
    print(f"  {old_name} -> {new_name}")
    return True


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    app = Path(sys.argv[1]).resolve()
    frameworks = app / "Contents" / "Frameworks"
    if not frameworks.is_dir():
        raise SystemExit(f"No parece una app de macOS: {app}")
    app_name = app.name[: -len(".app")]

    changed = 0
    for helper in sorted(frameworks.glob("* Helper*.app")):
        changed += rename_helper(helper, app_name)
    if changed:
        sign(app)
    print(f"{changed} ayudantes de Chromium renombrados a «{app_name} Helper»")


main()
