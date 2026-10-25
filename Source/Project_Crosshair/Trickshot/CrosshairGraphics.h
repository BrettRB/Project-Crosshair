#pragma once
#include "CoreMinimal.h"

struct FCrosshairSettings;

/**
 * Graphics presets sized for laptop power, plus the ray tracing switch. Settings live in the Crosshair
 * save (FCrosshairSettings) and are applied with engine scalability and render console variables, so
 * they take effect immediately without a restart. Nothing is applied until the player chooses.
 */
namespace CrosshairGraphics
{
 constexpr int32 PresetCount=4;
 /** 0 battery saver/integrated, 1 balanced laptop, 2 gaming laptop, 3 high-end/desktop (engine scalability 0..3). */
 PROJECT_CROSSHAIR_API const TCHAR* PresetName(int32 Preset);
 PROJECT_CROSSHAIR_API const TCHAR* PresetHelp(int32 Preset);
 /** Resolution scale (percent) and ray tracing each preset starts with. */
 PROJECT_CROSSHAIR_API int32 PresetResolution(int32 Preset);
 PROJECT_CROSSHAIR_API bool PresetRayTracing(int32 Preset);
 /** The GPU, driver and project support hardware ray tracing. */
 PROJECT_CROSSHAIR_API bool IsRayTracingSupported();
 /** Ray tracing is active right now (r.RayTracing.Enable and hardware Lumen both on). */
 PROJECT_CROSSHAIR_API bool IsRayTracingActive();
 /** Selects a preset and its default resolution and ray tracing. */
 PROJECT_CROSSHAIR_API void SelectPreset(FCrosshairSettings& Settings,int32 Preset);
 /** Measures this PC (engine benchmark) and selects the preset that suits it; integrated graphics stay at or below Balanced. */
 PROJECT_CROSSHAIR_API int32 Detect(FCrosshairSettings& Settings);
 /** Pushes the saved choice to the engine (no-op until the player has configured graphics). */
 PROJECT_CROSSHAIR_API void Apply(const FCrosshairSettings& Settings);
 /** Preset to show in the menu: the saved one, or the engine's current overall level before any choice. */
 PROJECT_CROSSHAIR_API int32 CurrentPreset(const FCrosshairSettings& Settings);
}
