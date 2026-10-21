# Startup preparation

Launching a practice game now shows **PREPARING GAME** before play is enabled. The screen reports content-loading progress and outstanding shader/graphics pipeline work. Preparation runs once per game instance, including Standalone Game and packaged clients. It does not run in commandlets or dedicated servers.

`UCrosshairStartup` discovers assets under `/Game/Crosshair` and recursively follows their runtime package dependencies, including imported Nuketown meshes/materials outside that directory. It asynchronously loads the shipped map packages and shared weapons, soldiers, animations, materials and other referenced content. Missing requested assets or a 15-minute timeout leave a visible failure message rather than enabling play with an incomplete cache.

In uncooked editor builds, the preparation gate waits for asset and shader compilation. Runtime resource/component PSO precaching is enabled in DefaultEngine.ini. The loader explicitly requests authored map component/material combinations and runtime weapon camo and soldier uniform/equipment combinations, without initializing the preview worlds or running their gameplay. It waits for component jobs, the combined pipeline counter and a one-second quiet interval before unlocking play. See [Epic's PSO precaching documentation](https://dev.epicgames.com/documentation/unreal-engine/pso-precaching-for-unreal-engine).

The game instance keeps strong references to the loaded non-world assets across map changes and garbage collection. Temporary map worlds and warmup components are released after preparation so map travel can destroy worlds normally. Play, map changes, look/movement/actions and replay recording progression are gated while preparation is active. The normal home screen becomes available after preparation.

DefaultGame.ini includes `/Game/Crosshair` in the cook so runtime discovery also includes cosmetic options that no placed actor currently uses. Packaged games still require a normal successful cook; they use cooked shader bytecode and prepare GPU pipelines at startup rather than compiling shader source on the player's machine.

## Practical limits

This trades longer startup and greater session memory use for less repeated asset loading and first-use shader/pipeline work. It does not make OpenLevel instantaneous: world/actor creation, collision, texture/mesh streaming, GPU uploads and renderer state changes still occur. It does not force every texture mip into VRAM or initialize every map's gameplay at startup. Driver/GPU/settings/content changes can require different pipelines. Additional maps/cosmetics must be cooked and reachable from the content/dependency catalog.

Local OBJ imports still parse geometry, build collision and decode their optional images when instantiated; new imports selected after launch cannot be preloaded by this startup pass. Their shared base material is preloaded. Existing invalid imported source materials may compile to fallback materials; warmup does not repair those material graphs or missing textures.

## Local verification (2026-10-20)

- `Build.bat Project_CrosshairEditor Win64 Development -Project=<project> -WaitMutex -NoHotReloadFromIDE`: succeeded.
- `Build.bat Project_Crosshair Win64 Development -Project=<project> -WaitMutex -NoHotReloadFromIDE`: succeeded, including the non-editor shader preparation code path.
- Headless frontend smoke: `-game -CrosshairSmoke -CrosshairFrontendSmoke -NullRHI -unattended -nosound -ExecCmds="t.MaxFPS 60"`: CROSSHAIR_FRONTEND_OK, exit 0.
- Final rendered frontend: `-game -CrosshairSmoke -CrosshairFrontendSmoke -CrosshairFrontendVisual -RenderOffscreen -unattended -nosound -ResX=1280 -ResY=720 -windowed -ExecCmds="t.MaxFPS 60" -abslog=<project>/Saved/Logs/StartupFrontendRendered.log`: CROSSHAIR_FRONTEND_OK, exit 0. Final runtime dependency filter loaded 4,316 assets and three map packages; preparation completed in 29.67 seconds while the game target compiled concurrently. This is an observation on this machine, not a performance guarantee.
- Frontend checks cover blocked play/input/travel, cached imported-map material, one preparation per session, and cache retention through garbage collection, imported-map replay, Nuketown and return to practice. Existing frontend/class/soldier assertions remain enabled. `Saved/Screenshots/StartupPreparation.png` was visually inspected.
- No fresh-driver-cache benchmark, packaged cook/runtime verification, physical controller validation or claim of zero map-loading time.

- `UnrealEditor-Cmd <project> -unattended -NullRHI -nosound -ExecCmds="Automation RunTests Crosshair" -TestExit="Automation Test Queue Empty" -abslog=<project>/Saved/Logs/StartupAutomation.log`: all nine tests succeeded, exit 0.
- `UnrealEditor-Cmd <project> /Game/Crosshair/Maps/L_Practice -game -CrosshairSmoke -NullRHI -unattended -nosound -ExecCmds="t.MaxFPS 60" -abslog=<project>/Saved/Logs/StartupCore.log`: CROSSHAIR_SMOKE_OK, exit 0, no smoke failures.
