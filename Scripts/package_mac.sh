#!/bin/sh
# Builds, cooks and packages Crosshair as a Mac app (run on a Mac; Unreal cannot build Mac games from Windows).
# Needs Unreal Engine 5.8 for Mac and Xcode 15.2 or newer (26.x recommended) with its command line tools.
#   sh Scripts/package_mac.sh                 -> Packaged/Mac/Project_Crosshair.app (Development)
#   sh Scripts/package_mac.sh Shipping        -> Shipping configuration instead
# Override the engine location with UE_ROOT=/path/to/UE_5.8 if it is not in the launcher's default folder.
set -e
CONFIG="${1:-Development}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"

if [ ! -f "$UAT" ]; then
  echo "RunUAT.sh not found at $UAT. Install Unreal Engine 5.8 or set UE_ROOT." >&2
  exit 1
fi
if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode command line tools are missing. Install Xcode, open it once, then run: xcode-select --install" >&2
  exit 1
fi

"$UAT" BuildCookRun \
  -project="$PROJECT_DIR/Project_Crosshair.uproject" \
  -platform=Mac -clientconfig="$CONFIG" \
  -build -cook -stage -pak -archive -archivedirectory="$PROJECT_DIR/Packaged" \
  -nop4 -utf8output -unattended

echo "CROSSHAIR_MAC_PACKAGE_OK $PROJECT_DIR/Packaged/Mac"
