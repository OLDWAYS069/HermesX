import configparser
import os
import re
import subprocess

run_number = os.getenv("GITHUB_RUN_NUMBER", "0")
build_location = os.getenv("BUILD_LOCATION", "local")


def readProps(prefsLoc):
    """Read the version of our project as a string"""

    config = configparser.RawConfigParser()
    config.read(prefsLoc)
    version = dict(config.items("VERSION"))
    verObj = dict(
        short="{}.{}.{}".format(version["major"], version["minor"], version["build"]),
        long="unset",
        deb="unset",
        display="unset",
    )

    try:
        sha = (
            subprocess.check_output(["git", "rev-parse", "--short", "HEAD"])
            .decode("utf-8")
            .strip()
        )
        isDirty = subprocess.check_output(["git", "diff", "HEAD"]).decode("utf-8").strip()
        suffix = sha  # keep clean; if you need dirty suffix, append here

        # Core firmware version (kept standard for compatibility)
        verObj["long"] = "{}.{}".format(verObj["short"], suffix)
        verObj["deb"] = "{}.{}~{}{}".format(verObj["short"], run_number, build_location, sha)

        # Display version: try to derive semantic tag from branch name (e.g., hermesX_b0.2.6)
        display_short = verObj["short"]
        try:
            branch = (
                subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"])
                .decode("utf-8")
                .strip()
            )
            m = re.search(r"(\d+\.\d+\.\d+)", branch)
            if m:
                display_short = m.group(1)
        except Exception:
            pass

        verObj["display"] = "HXB_{}{}".format(display_short, suffix)
    except Exception:
        verObj["long"] = verObj["short"]
        verObj["deb"] = "{}.{}~{}".format(verObj["short"], run_number, build_location)
        verObj["display"] = "HXB_{}".format(verObj["short"])

    return verObj


# print("path is" + ','.join(sys.path))
