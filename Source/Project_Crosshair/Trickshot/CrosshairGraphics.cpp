#include "CrosshairGraphics.h"
#include "CrosshairData.h"
#include "CrosshairEnvironment.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "RenderUtils.h"
#include "RHIGlobals.h"
#include "Scalability.h"

namespace
{
 void SetGraphicsCVar(const TCHAR* Name,int32 Value)
 {
  if (IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name)) Var->Set(Value,ECVF_SetByCode);
 }
 void SetGraphicsCVar(const TCHAR* Name,float Value)
 {
  if (IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name)) Var->Set(Value,ECVF_SetByCode);
 }
 int32 GetGraphicsCVar(const TCHAR* Name,int32 Fallback)
 {
  const IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name);
  return Var ? Var->GetInt() : Fallback;
 }
 constexpr int32 GraphicsFrameRates[]={0,30,60,75,90,120,144,165,240};
 constexpr int32 GraphicsFrameRateCount=UE_ARRAY_COUNT(GraphicsFrameRates);
 int32 WrapGraphicsIndex(int32 Value,int32 Count) { return ((Value%Count)+Count)%Count; }
 const TCHAR* GraphicsOnOffText(bool b) { return b ? TEXT("On") : TEXT("Off"); }
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
 case 0: return TEXT("Low quality at 67% resolution, 60 FPS cap, no volumetric fog or clouds. Best for integrated graphics or battery.");
 case 1: return TEXT("Medium quality at 80% resolution for thin laptops and integrated Radeon/Intel Arc graphics.");
 case 2: return TEXT("High quality at full resolution with ray tracing for laptops with a dedicated GPU.");
 default: return TEXT("Epic quality, full resolution and ray tracing for high-end laptops and desktops.");
 }
}
int32 CrosshairGraphics::PresetResolution(int32 Preset) { return Preset<=0 ? 67 : Preset==1 ? 80 : 100; }
bool CrosshairGraphics::PresetRayTracing(int32 Preset) { return Preset>=2; }

FCrosshairGraphicsOptions CrosshairGraphics::PresetOptions(int32 Preset)
{
 const int32 Level=FMath::Clamp(Preset,0,PresetCount-1);
 FCrosshairGraphicsOptions O;
 O.ViewDistance=O.Shadows=O.GlobalIllumination=O.Reflections=O.Textures=O.Effects=O.PostProcess=O.Foliage=O.Shading=Level;
 O.AntiAliasing=Level==0 ? 2 : 3;          // TAA is the cheaper temporal upscaler on integrated graphics
 O.FrameRateLimit=Level==0 ? 60 : 0;       // saves battery and heat on the weakest laptops
 O.bVSync=false;
 O.bVolumetricFog=Level>=1;
 O.bVolumetricClouds=Level>=1;
 O.MotionBlur=Level==0 ? 0 : 1;
 O.bFilmEffects=true;
 O.Brightness=0;
 O.Sharpening=1;
 return O;
}

bool CrosshairGraphics::IsCustom(const FCrosshairSettings& Settings)
{
 return Settings.bGraphicsConfigured && !(Settings.GraphicsOptions==PresetOptions(Settings.GraphicsPreset));
}

bool CrosshairGraphics::IsRayTracingSupported() { return IsRayTracingAllowed(); }
bool CrosshairGraphics::IsRayTracingActive()
{
 return IsRayTracingSupported() && GetGraphicsCVar(TEXT("r.RayTracing.Enable"),0)!=0 && GetGraphicsCVar(TEXT("r.Lumen.HardwareRayTracing"),0)!=0;
}

void CrosshairGraphics::SelectPreset(FCrosshairSettings& Settings,int32 Preset)
{
 Settings.GraphicsPreset=FMath::Clamp(Preset,0,PresetCount-1);
 Settings.ResolutionScale=PresetResolution(Settings.GraphicsPreset);
 Settings.bRayTracing=PresetRayTracing(Settings.GraphicsPreset) && IsRayTracingSupported();
 Settings.GraphicsOptions=PresetOptions(Settings.GraphicsPreset);
 Settings.GraphicsVersion=1;
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

bool CrosshairGraphics::ShouldAutoDetect(const FCrosshairSettings& Settings)
{
 // Automated runs (every smoke flag contains "Smoke") and headless runs keep fixed engine defaults.
 return !Settings.bGraphicsConfigured && FApp::CanEverRender() && !FCString::Stristr(FCommandLine::Get(),TEXT("Smoke")) && !GIsAutomationTesting;
}

void CrosshairGraphics::Upgrade(FCrosshairSettings& Settings)
{
 Settings.GraphicsPreset=FMath::Clamp(Settings.GraphicsPreset,0,PresetCount-1);
 Settings.ResolutionScale=FMath::Clamp(Settings.ResolutionScale,50,100);
 if (Settings.GraphicsVersion<1)
 {
  // Older saves: keep the chosen preset, resolution and ray tracing; add that preset's options.
  if (Settings.bGraphicsConfigured) Settings.GraphicsOptions=PresetOptions(Settings.GraphicsPreset);
  Settings.GraphicsVersion=1;
 }
 FCrosshairGraphicsOptions& O=Settings.GraphicsOptions;
 for (int32* Level:{&O.ViewDistance,&O.Shadows,&O.GlobalIllumination,&O.Reflections,&O.Textures,&O.Effects,&O.PostProcess,&O.Foliage,&O.Shading}) *Level=FMath::Clamp(*Level,0,3);
 O.AntiAliasing=FMath::Clamp(O.AntiAliasing,0,3);
 O.FrameRateLimit=FMath::Clamp(O.FrameRateLimit,0,240);
 O.MotionBlur=FMath::Clamp(O.MotionBlur,0,2);
 O.Brightness=FMath::Clamp(O.Brightness,-5,5);
 O.Sharpening=FMath::Clamp(O.Sharpening,0,3);
}

int32 CrosshairGraphics::CurrentPreset(const FCrosshairSettings& Settings)
{
 if (Settings.bGraphicsConfigured) return Settings.GraphicsPreset;
 return FMath::Clamp(Scalability::GetQualityLevels().GetMinQualityLevel(),0,PresetCount-1);
}
FCrosshairGraphicsOptions CrosshairGraphics::CurrentOptions(const FCrosshairSettings& Settings)
{
 return Settings.bGraphicsConfigured ? Settings.GraphicsOptions : PresetOptions(CurrentPreset(Settings));
}

void CrosshairGraphics::Apply(const FCrosshairSettings& Settings)
{
 if (!Settings.bGraphicsConfigured) return;
 const FCrosshairGraphicsOptions& O=Settings.GraphicsOptions;
 const int32 Preset=FMath::Clamp(Settings.GraphicsPreset,0,PresetCount-1);
 Scalability::FQualityLevels Levels=Scalability::GetQualityLevels();
 Levels.SetFromSingleQualityLevel(Preset);
 Levels.ViewDistanceQuality=O.ViewDistance; Levels.LandscapeQuality=O.ViewDistance;
 Levels.ShadowQuality=O.Shadows;
 Levels.GlobalIlluminationQuality=O.GlobalIllumination;
 Levels.ReflectionQuality=O.Reflections;
 Levels.TextureQuality=O.Textures;
 Levels.EffectsQuality=O.Effects;
 Levels.PostProcessQuality=O.PostProcess;
 Levels.FoliageQuality=O.Foliage;
 Levels.ShadingQuality=O.Shading;
 Levels.ResolutionQuality=FMath::Clamp(float(Settings.ResolutionScale),50.f,100.f);
 Scalability::SetQualityLevels(Levels,true);
 // Ray tracing toggles at runtime (r.RayTracing.EnableOnDemand); hardware Lumen falls back to software Lumen when off.
 const bool bRayTracing=Settings.bRayTracing && IsRayTracingSupported();
 SetGraphicsCVar(TEXT("r.RayTracing.Enable"),bRayTracing ? 1 : 0);
 SetGraphicsCVar(TEXT("r.Lumen.HardwareRayTracing"),bRayTracing ? 1 : 0);
 // Options set after scalability so they win over the quality groups' own values.
 const int32 AAMethods[]={0,1,2,4};
 SetGraphicsCVar(TEXT("r.AntiAliasingMethod"),AAMethods[FMath::Clamp(O.AntiAliasing,0,3)]);
 SetGraphicsCVar(TEXT("t.MaxFPS"),float(O.FrameRateLimit));
 SetGraphicsCVar(TEXT("r.VSync"),O.bVSync ? 1 : 0);
 SetGraphicsCVar(TEXT("r.VolumetricFog"),O.bVolumetricFog ? 1 : 0);
 SetGraphicsCVar(TEXT("r.MotionBlurQuality"),O.MotionBlur==0 ? 0 : O.PostProcess>=3 ? 4 : 3);
 const float Sharpen[]={0.f,.5f,1.f,1.6f};
 SetGraphicsCVar(TEXT("r.Tonemapper.Sharpen"),Sharpen[FMath::Clamp(O.Sharpening,0,3)]);
 if (GEngine) for (const FWorldContext& Context:GEngine->GetWorldContexts())
  if (UWorld* World=Context.World()) if (auto* Environment=World->GetSubsystem<UCrosshairEnvironmentSubsystem>()) Environment->ApplyQuality(O,bRayTracing);
 UE_LOG(LogTemp,Display,TEXT("Crosshair graphics: %s%s, %d%% resolution, ray tracing %s, AA %s, FPS %s"),PresetName(Preset),IsCustom(Settings) ? TEXT(" (custom)") : TEXT(""),
  Settings.ResolutionScale,bRayTracing ? TEXT("on") : TEXT("off"),*OptionValue(O,AntiAliasing),*OptionValue(O,FrameRateLimit));
}

const TCHAR* CrosshairGraphics::QualityName(int32 Level)
{
 switch (Level) { case 0: return TEXT("Low"); case 1: return TEXT("Medium"); case 2: return TEXT("High"); default: return TEXT("Epic"); }
}

const TCHAR* CrosshairGraphics::OptionName(int32 Option)
{
 switch (Option)
 {
 case AntiAliasing: return TEXT("Anti-aliasing");
 case FrameRateLimit: return TEXT("Frame rate limit");
 case VSync: return TEXT("VSync");
 case ViewDistance: return TEXT("View distance");
 case Shadows: return TEXT("Shadows");
 case GlobalIllumination: return TEXT("Global illumination");
 case Reflections: return TEXT("Reflections");
 case Textures: return TEXT("Textures");
 case Effects: return TEXT("Effects");
 case PostProcess: return TEXT("Post-processing");
 case Foliage: return TEXT("Foliage");
 case Shading: return TEXT("Shading");
 case VolumetricFog: return TEXT("Volumetric fog");
 case VolumetricClouds: return TEXT("Volumetric clouds");
 case MotionBlur: return TEXT("Motion blur");
 case FilmEffects: return TEXT("Film grain and vignette");
 case Brightness: return TEXT("Brightness");
 case Sharpening: return TEXT("Sharpening");
 default: return TEXT("");
 }
}

FString CrosshairGraphics::OptionValue(const FCrosshairGraphicsOptions& O,int32 Option)
{
 static const TCHAR* AANames[]={TEXT("Off"),TEXT("FXAA"),TEXT("TAA"),TEXT("TSR")};
 static const TCHAR* Levels3[]={TEXT("Off"),TEXT("Low"),TEXT("High")};
 static const TCHAR* Levels4[]={TEXT("Off"),TEXT("Low"),TEXT("Medium"),TEXT("High")};
 switch (Option)
 {
 case AntiAliasing: return AANames[FMath::Clamp(O.AntiAliasing,0,3)];
 case FrameRateLimit: return O.FrameRateLimit<=0 ? FString(TEXT("Unlimited")) : FString::Printf(TEXT("%d FPS"),O.FrameRateLimit);
 case VSync: return GraphicsOnOffText(O.bVSync);
 case ViewDistance: return QualityName(O.ViewDistance);
 case Shadows: return QualityName(O.Shadows);
 case GlobalIllumination: return QualityName(O.GlobalIllumination);
 case Reflections: return QualityName(O.Reflections);
 case Textures: return QualityName(O.Textures);
 case Effects: return QualityName(O.Effects);
 case PostProcess: return QualityName(O.PostProcess);
 case Foliage: return QualityName(O.Foliage);
 case Shading: return QualityName(O.Shading);
 case VolumetricFog: return GraphicsOnOffText(O.bVolumetricFog);
 case VolumetricClouds: return GraphicsOnOffText(O.bVolumetricClouds);
 case MotionBlur: return Levels3[FMath::Clamp(O.MotionBlur,0,2)];
 case FilmEffects: return GraphicsOnOffText(O.bFilmEffects);
 case Brightness: return O.Brightness==0 ? FString(TEXT("Default")) : FString::Printf(TEXT("%+d"),O.Brightness);
 case Sharpening: return Levels4[FMath::Clamp(O.Sharpening,0,3)];
 default: return FString();
 }
}

const TCHAR* CrosshairGraphics::OptionHelp(int32 Option)
{
 switch (Option)
 {
 case AntiAliasing: return TEXT("Smooths jagged edges. TSR is sharpest and upscales best; TAA is cheaper; FXAA and Off are fastest.");
 case FrameRateLimit: return TEXT("Caps frames per second. A cap saves battery and keeps laptops cooler.");
 case VSync: return TEXT("Syncs frames to the display to stop tearing. Adds a little input delay.");
 case ViewDistance: return TEXT("How far away detail and small objects are drawn.");
 case Shadows: return TEXT("Shadow resolution and softness. One of the biggest costs on laptops.");
 case GlobalIllumination: return TEXT("Bounced light (Lumen). Low turns it off; Epic is the most accurate. A big cost on integrated graphics.");
 case Reflections: return TEXT("Quality of reflections on metal, glass and wet surfaces.");
 case Textures: return TEXT("Texture detail. Lower it if your GPU has little video memory.");
 case Effects: return TEXT("Particles, smoke, explosions and sky detail.");
 case PostProcess: return TEXT("Bloom, depth of field, exposure and other screen effects.");
 case Foliage: return TEXT("How much grass and plant detail is drawn.");
 case Shading: return TEXT("Material detail such as skin and fabric.");
 case VolumetricFog: return TEXT("Light shafts and haze lit by the sun. Off is faster.");
 case VolumetricClouds: return TEXT("Real 3D clouds that cast soft shadows. Off is faster on integrated graphics.");
 case MotionBlur: return TEXT("Blur when turning fast. Off keeps trickshot spins crisp.");
 case FilmEffects: return TEXT("Film grain, a light vignette and a faint lens flare from the sun.");
 case Brightness: return TEXT("Raises or lowers overall exposure in tenths of a stop.");
 case Sharpening: return TEXT("Sharpens the image, useful at a lower resolution scale.");
 default: return TEXT("");
 }
}

void CrosshairGraphics::StepOption(FCrosshairGraphicsOptions& O,int32 Option,int32 Direction)
{
 const int32 D=Direction>=0 ? 1 : -1;
 switch (Option)
 {
 case AntiAliasing: O.AntiAliasing=WrapGraphicsIndex(O.AntiAliasing+D,4); break;
 case FrameRateLimit:
 {
  int32 Index=0;
  for (int32 i=0;i<GraphicsFrameRateCount;++i) if (GraphicsFrameRates[i]==O.FrameRateLimit) Index=i;
  O.FrameRateLimit=GraphicsFrameRates[WrapGraphicsIndex(Index+D,GraphicsFrameRateCount)]; break;
 }
 case VSync: O.bVSync=!O.bVSync; break;
 case ViewDistance: O.ViewDistance=WrapGraphicsIndex(O.ViewDistance+D,4); break;
 case Shadows: O.Shadows=WrapGraphicsIndex(O.Shadows+D,4); break;
 case GlobalIllumination: O.GlobalIllumination=WrapGraphicsIndex(O.GlobalIllumination+D,4); break;
 case Reflections: O.Reflections=WrapGraphicsIndex(O.Reflections+D,4); break;
 case Textures: O.Textures=WrapGraphicsIndex(O.Textures+D,4); break;
 case Effects: O.Effects=WrapGraphicsIndex(O.Effects+D,4); break;
 case PostProcess: O.PostProcess=WrapGraphicsIndex(O.PostProcess+D,4); break;
 case Foliage: O.Foliage=WrapGraphicsIndex(O.Foliage+D,4); break;
 case Shading: O.Shading=WrapGraphicsIndex(O.Shading+D,4); break;
 case VolumetricFog: O.bVolumetricFog=!O.bVolumetricFog; break;
 case VolumetricClouds: O.bVolumetricClouds=!O.bVolumetricClouds; break;
 case MotionBlur: O.MotionBlur=WrapGraphicsIndex(O.MotionBlur+D,3); break;
 case FilmEffects: O.bFilmEffects=!O.bFilmEffects; break;
 case Brightness: O.Brightness=FMath::Clamp(O.Brightness+D,-5,5); break;
 case Sharpening: O.Sharpening=WrapGraphicsIndex(O.Sharpening+D,4); break;
 default: break;
 }
}
