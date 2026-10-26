# Playing on Mac

Crosshair runs on macOS through Unreal's Metal renderer. The gameplay code has no Windows-only parts, so the same project builds for Mac. **A Mac build has to be made on a Mac**: Unreal compiles Mac games and Metal shaders with Xcode, which doesn't run on Windows.

## What you need

- A Mac with **Apple Silicon** (M1 or newer) running **macOS 14 Sonoma** or later. The project targets Metal SM6 (Nanite, Lumen, virtual shadow maps and Substrate), which needs Apple Silicon. Intel Macs aren't supported.
- **Xcode** 15.2 or newer (Unreal 5.8 is built against Xcode 26.1). Open it once to accept the licence, then run `xcode-select --install`.
- **Unreal Engine 5.8 for Mac** from the Epic Games Launcher. It installs to `/Users/Shared/Epic Games/UE_5.8` by default.
- **Git LFS**: `brew install git-lfs && git lfs install`. Without it, the maps and assets download as small pointer files and the game opens to empty levels.

## Play in the editor

```
git clone git@github.com:BrettRB/Project-Crosshair.git
cd Project-Crosshair
git lfs pull
```

Open `Project_Crosshair.uproject` in Unreal Editor 5.8 and click **Yes** when it asks to build the C++ modules. The first launch compiles Metal shaders and takes a while. Choose **Standalone Game** from the Play menu; replays need Standalone or a packaged build.

## Package a Mac app

```
sh Scripts/package_mac.sh            # Development build
sh Scripts/package_mac.sh Shipping   # Shipping build
```

The script runs Unreal's `BuildCookRun` for the `Mac` platform and writes `Packaged/Mac/Project_Crosshair.app` (the `Packaged/` folder is git-ignored). If the engine is somewhere else, set `UE_ROOT=/path/to/UE_5.8` first. You can also package from the editor with **Platforms → Mac → Package Project**.

The app uses the Crosshair logo as its Dock icon: `Scripts/create_logo.ps1` generates the `Build/Mac/Resources/Assets.xcassets` AppIcon set as well as the Windows icon.

### Running a copied app

A packaged app isn't signed or notarised (that needs a paid Apple Developer account). If you copy it to another Mac, Gatekeeper may say it is damaged or from an unidentified developer. Either right-click the app and choose **Open**, or clear the quarantine flag:

```
xattr -dr com.apple.quarantine Project_Crosshair.app
```

## Differences from Windows

- **Ray tracing.** Metal ray tracing support depends on the Mac's GPU. The Graphics menu only offers ray tracing when the engine reports it is supported, and lighting otherwise uses software Lumen. **Auto-detect** picks a preset that suits the Mac.
- **Controllers.** Controllers that macOS supports natively (Xbox, PlayStation, MFi) work through Apple's Game Controller framework. The Windows-only GameInput plugin is turned off on Mac; button layout and the Controls menu settings are the same.
- **Mouse.** A trackpad works, but a mouse is much better for trickshots. Right-click (aim) is a two-finger click on a trackpad. Turn off **System Settings → Mouse → Pointer acceleration** for consistent look speed.
- **Keyboard.** Controls are the same as on Windows (see the [README](../README.md#controls)). Cmd+Q quits.
- **Saved data.** Settings, replays and imported maps are saved under `~/Library/Application Support/Epic/Project_Crosshair/`. In the map import dialog, paste a Mac path such as `/Users/you/Maps/MyMap/map.json`. Dragging a file from Finder into Terminal shows its full path.
- **LAN.** LAN uses the same local session system on both platforms, so Mac and Windows players on the same network should be able to join each other if both run builds of the same version. Cross-platform joining hasn't been tested yet. If the macOS firewall is on, it asks to allow incoming connections; allow them, or other players can't find a match you host.

## Verification status

The Mac support was prepared on Windows. The C++ module, automation tests and icon generation were checked there. Compiling, cooking and running on an actual Mac haven't been verified yet. The first person to build on a Mac should report back any compiler or cook errors.
