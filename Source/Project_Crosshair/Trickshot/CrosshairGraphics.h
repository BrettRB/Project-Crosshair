#pragma once
#include "CoreMinimal.h"

struct FCrosshairSettings;
struct FCrosshairGraphicsOptions;

/**
 * Graphics presets sized for laptop power, the ray tracing switch and individual options
 * (FCrosshairGraphicsOptions) under each preset. Settings live in the Crosshair save
 * (FCrosshairSettings) and are applied with engine scalability and render console variables, so
 * they take effect immediately without a restart.
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
 /** The individual options each preset starts with. */
 PROJECT_CROSSHAIR_API FCrosshairGraphicsOptions PresetOptions(int32 Preset);
 /** Any individual option differs from the selected preset's. */
 PROJECT_CROSSHAIR_API bool IsCustom(const FCrosshairSettings& Settings);
 /** The GPU, driver and project support hardware ray tracing. */
 PROJECT_CROSSHAIR_API bool IsRayTracingSupported();
 /** Ray tracing is active right now (r.RayTracing.Enable and hardware Lumen both on). */
 PROJECT_CROSSHAIR_API bool IsRayTracingActive();
 /** Selects a preset and its default resolution, ray tracing and options. */
 PROJECT_CROSSHAIR_API void SelectPreset(FCrosshairSettings& Settings,int32 Preset);
 /** Measures this PC (engine benchmark) and selects the preset that suits it; integrated graphics stay at or below Balanced. */
 PROJECT_CROSSHAIR_API int32 Detect(FCrosshairSettings& Settings);
 /** First launch on a real display (not automated tests): measure the PC so weak laptops start on a light preset. */
 PROJECT_CROSSHAIR_API bool ShouldAutoDetect(const FCrosshairSettings& Settings);
 /** Fills options for saves made before individual options existed. */
 PROJECT_CROSSHAIR_API void Upgrade(FCrosshairSettings& Settings);
 /** Pushes the saved choice to the engine (no-op until graphics are configured). */
 PROJECT_CROSSHAIR_API void Apply(const FCrosshairSettings& Settings);
 /** Preset to show in the menu: the saved one, or the engine's current overall level before any choice. */
 PROJECT_CROSSHAIR_API int32 CurrentPreset(const FCrosshairSettings& Settings);
 /** Options to show in the menu: the saved ones, or the current preset's before any choice. */
 PROJECT_CROSSHAIR_API FCrosshairGraphicsOptions CurrentOptions(const FCrosshairSettings& Settings);

 /** Individual option rows, in menu order (after preset, ray tracing, resolution and detect). */
 enum EOption : int32 { AntiAliasing, FrameRateLimit, VSync, ViewDistance, Shadows, GlobalIllumination, Reflections, Textures, Effects,
  PostProcess, Foliage, Shading, VolumetricFog, VolumetricClouds, MotionBlur, FilmEffects, Brightness, Sharpening, OptionCount };
 PROJECT_CROSSHAIR_API const TCHAR* OptionName(int32 Option);
 PROJECT_CROSSHAIR_API FString OptionValue(const FCrosshairGraphicsOptions& Options,int32 Option);
 PROJECT_CROSSHAIR_API const TCHAR* OptionHelp(int32 Option);
 /** Left/Right on an option: quality levels and lists wrap, brightness stops at its ends. */
 PROJECT_CROSSHAIR_API void StepOption(FCrosshairGraphicsOptions& Options,int32 Option,int32 Direction);
 PROJECT_CROSSHAIR_API const TCHAR* QualityName(int32 Level);
}
