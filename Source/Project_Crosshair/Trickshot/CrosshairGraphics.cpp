#include "CrosshairGraphics.h"
#include "CrosshairData.h"
#include "CrosshairEnvironment.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "RenderUtils.h"
#include "RHIGlobals.h"
#include "Scalability.h"

namespace
{
 void SetCVar(const TCHAR* Name,int32 Value)
 {
  if (IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name)) Var->Set(Value,ECVF_SetByCode);
 }
 int32 GetCVar(const TCHAR* Name,int32 Fallback)
 {
  const IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name);
  return Var ? Var->GetInt() : Fallback;
 }
}

const TCHAR* CrosshairGraphics::PresetName(int32 Preset)
{
 switch (Preset)
 {
 case 0: return TEXT("Battery saver (integrated graphics)");
 case 1: return TEXT("Balanced laptop");
 case 2: return TEXT("Gaming laptop");
 default: return TEXT("High-end laptop / desktop");
 }
}
const TCHAR* CrosshairGraphics::PresetHelp(int32 Preset)
{
 switch (Preset)
 {
 case 0: return TEXT("Lowest shadows, effects and global illumination at 67% resolution. Best for integrated graphics or battery.");
 case 1: return TEXT("Medium quality at 80% resolution for thin laptops and integrated Radeon/Intel Arc graphics.");
 case 2: return TEXT("High quality at full resolution with ray tracing for laptops with a dedicated GPU.");
 default: return TEXT("Epic quality, full resolution and ray tracing for high-end laptops and desktops.");
 }
}
int32 CrosshairGraphics::PresetResolution(int32 Preset) { return Preset<=0 ? 67 : Preset==1 ? 80 : 100; }
bool CrosshairGraphics::PresetRayTracing(int32 Preset) { return Preset>=2; }

bool CrosshairGraphics::IsRayTracingSupported() { return IsRayTracingAllowed(); }
bool CrosshairGraphics::IsRayTracingActive()
{
 return IsRayTracingSupported() && GetCVar(TEXT("r.RayTracing.Enable"),0)!=0 && GetCVar(TEXT("r.Lumen.HardwareRayTracing"),0)!=0;
}

void CrosshairGraphics::SelectPreset(FCrosshairSettings& Settings,int32 Preset)
{
 Settings.GraphicsPreset=FMath::Clamp(Preset,0,PresetCount-1);
 Settings.ResolutionScale=PresetResolution(Settings.GraphicsPreset);
 Settings.bRayTracing=PresetRayTracing(Settings.GraphicsPreset) && IsRayTracingSupported();
 Settings.bGraphicsConfigured=true;
}

int32 CrosshairGraphics::Detect(FCrosshairSettings& Settings)
{
 // The engine benchmark rates CPU and GPU per quality group; the weakest group decides the preset.
 const Scalability::FQualityLevels Measured=Scalability::BenchmarkQualityLevels();
 int32 Preset=FMath::Clamp(Measured.GetMinQualityLevel(),0,PresetCount-1);
 if (GRHIDeviceIsIntegrated) Preset=FMath::Min(Preset,1);
 SelectPreset(Settings,Preset);
 return Preset;
}

int32 CrosshairGraphics::CurrentPreset(const FCrosshairSettings& Settings)
{
 if (Settings.bGraphicsConfigured) return Settings.GraphicsPreset;
 return FMath::Clamp(Scalability::GetQualityLevels().GetMinQualityLevel(),0,PresetCount-1);
}

void CrosshairGraphics::Apply(const FCrosshairSettings& Settings)
{
 if (!Settings.bGraphicsConfigured) return;
 Scalability::FQualityLevels Levels=Scalability::GetQualityLevels();
 Levels.SetFromSingleQualityLevel(FMath::Clamp(Settings.GraphicsPreset,0,PresetCount-1));
 Levels.ResolutionQuality=FMath::Clamp(float(Settings.ResolutionScale),50.f,100.f);
 Scalability::SetQualityLevels(Levels,true);
 // Ray tracing toggles at runtime (r.RayTracing.EnableOnDemand); hardware Lumen falls back to software Lumen when off.
 const bool bRayTracing=Settings.bRayTracing && IsRayTracingSupported();
 SetCVar(TEXT("r.RayTracing.Enable"),bRayTracing ? 1 : 0);
 SetCVar(TEXT("r.Lumen.HardwareRayTracing"),bRayTracing ? 1 : 0);
 if (GEngine) for (const FWorldContext& Context:GEngine->GetWorldContexts())
  if (UWorld* World=Context.World()) if (auto* Environment=World->GetSubsystem<UCrosshairEnvironmentSubsystem>()) Environment->ApplyQuality(Settings.GraphicsPreset,bRayTracing);
 UE_LOG(LogTemp,Display,TEXT("Crosshair graphics: %s, %d%% resolution, ray tracing %s"),PresetName(Settings.GraphicsPreset),Settings.ResolutionScale,bRayTracing ? TEXT("on") : TEXT("off"));
}
