#include "CrosshairEnvironment.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInterface.h"
#include "CrosshairData.h"
#include "CrosshairGraphics.h"
#include "CrosshairReplaySubsystem.h"
#include "Engine/GameInstance.h"

const FName UCrosshairEnvironmentSubsystem::EnvironmentTag(TEXT("CrosshairEnvironment"));
const FName UCrosshairEnvironmentSubsystem::OptOutTag(TEXT("CrosshairCustomLighting"));

namespace
{
 template<typename T> T* FindOrSpawn(UWorld& World,const FVector& Location,const FRotator& Rotation=FRotator::ZeroRotator)
 {
  for (TActorIterator<T> It(&World);It;++It) return *It;
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  T* Actor=World.SpawnActor<T>(Location,Rotation,Params);
  if (Actor) Actor->Tags.Add(UCrosshairEnvironmentSubsystem::EnvironmentTag);
  return Actor;
 }
}

void UCrosshairEnvironmentSubsystem::OnWorldBeginPlay(UWorld& World)
{
 Super::OnWorldBeginPlay(World);
 for (TActorIterator<AActor> It(&World);It;++It) if (It->ActorHasTag(OptOutTag)) return;
 // Ground reference for fog: the map's player start (imported maps place it above their geometry).
 float Ground=0.f;
 for (TActorIterator<APlayerStart> It(&World);It;++It) { Ground=It->GetActorLocation().Z-200.f; break; }

 // Sun: keep each map's direction, make it the physically based atmosphere sun.
 Sun=FindOrSpawn<ADirectionalLight>(World,FVector(0,0,Ground+5000.f),FRotator(-42.f,-35.f,0.f));
 if (UDirectionalLightComponent* Light=Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr)
 {
  Light->SetMobility(EComponentMobility::Movable);
  Light->SetAtmosphereSunLight(true);
  Light->SetAtmosphereSunLightIndex(0);
  Light->SetIntensity(FMath::Max(Light->Intensity,6.5f));
  Light->SetUseTemperature(true);
  Light->SetTemperature(5900.f);               // late-morning daylight
  Light->SetLightSourceAngle(.53f);            // the real sun's angular size: soft, realistic shadow edges
  Light->ContactShadowLength=.025f;            // grounds small details and feet
  Light->bCastCloudShadows=true;
  Light->SetCastShadows(true);
  Light->MarkRenderStateDirty();
 }
 for (TActorIterator<ADirectionalLight> It(&World);It;++It)
  if (*It!=Sun) if (auto* Extra=Cast<UDirectionalLightComponent>(It->GetLightComponent())) Extra->SetAtmosphereSunLight(false);

 FindOrSpawn<ASkyAtmosphere>(World,FVector::ZeroVector);

 // Sky light captures the atmosphere and clouds every frame so ambient light matches the sky.
 if (ASkyLight* Sky=FindOrSpawn<ASkyLight>(World,FVector(0,0,Ground+3000.f)))
  if (USkyLightComponent* Ambient=Sky->GetLightComponent())
  {
   Ambient->SetMobility(EComponentMobility::Movable);
   Ambient->SourceType=SLS_CapturedScene;
   Ambient->bRealTimeCapture=true;
   Ambient->SetIntensity(1.f);
   Ambient->bLowerHemisphereIsBlack=false;
   Ambient->SetLowerHemisphereColor(FLinearColor(.022f,.019f,.015f));
   Ambient->MarkRenderStateDirty();
   Ambient->RecaptureSky();
  }

 // Height fog with volumetric scattering: distance haze and light shafts rather than a flat backdrop.
 if (AExponentialHeightFog* Fog=FindOrSpawn<AExponentialHeightFog>(World,FVector(0,0,Ground)))
  if (UExponentialHeightFogComponent* Haze=Fog->GetComponent())
  {
   Haze->SetFogDensity(.006f);
   Haze->SetFogHeightFalloff(.12f);
   Haze->SetStartDistance(1200.f);
   Haze->SetVolumetricFog(true);
   Haze->SetVolumetricFogScatteringDistribution(.65f);
   Haze->SetVolumetricFogExtinctionScale(.6f);
   Haze->SetFogInscatteringColor(FLinearColor(.18f,.22f,.28f));
  }

 // Volumetric clouds lit by the sun, casting soft cloud shadows.
 Clouds=FindOrSpawn<AVolumetricCloud>(World,FVector::ZeroVector);
 if (Clouds)
  if (UVolumetricCloudComponent* Cloud=Clouds->FindComponentByClass<UVolumetricCloudComponent>())
   if (!Cloud->GetMaterial())
    if (UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst")))
     Cloud->SetMaterial(Material);

 // Film-like grade at low priority: any map post-process volume still overrides these.
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 Grade=World.SpawnActor<APostProcessVolume>(FVector::ZeroVector,FRotator::ZeroRotator,Params);
 if (Grade)
 {
  Grade->Tags.Add(EnvironmentTag);
  Grade->bUnbound=true; Grade->Priority=-10.f; Grade->BlendWeight=1.f;
  FPostProcessSettings& S=Grade->Settings;
  S.bOverride_AutoExposureMethod=true; S.AutoExposureMethod=AEM_Histogram;
  S.bOverride_AutoExposureBias=true; S.AutoExposureBias=-.35f;          // keep snow, sky and concrete from blowing out
  S.bOverride_AutoExposureSpeedUp=true; S.AutoExposureSpeedUp=2.5f;
  S.bOverride_AutoExposureSpeedDown=true; S.AutoExposureSpeedDown=1.2f;
  S.bOverride_BloomIntensity=true; S.BloomIntensity=.3f;
  S.bOverride_AmbientOcclusionIntensity=true; S.AmbientOcclusionIntensity=.6f;
  S.bOverride_AmbientOcclusionRadius=true; S.AmbientOcclusionRadius=140.f;
  S.bOverride_ColorSaturation=true; S.ColorSaturation=FVector4(.92f,.92f,.94f,1.f);
  S.bOverride_ColorContrast=true; S.ColorContrast=FVector4(1.06f,1.06f,1.06f,1.f);
  S.bOverride_WhiteTemp=true; S.WhiteTemp=6300.f;
  S.bOverride_VignetteIntensity=true; S.VignetteIntensity=.28f;
  S.bOverride_FilmGrainIntensity=true; S.FilmGrainIntensity=.06f;
  S.bOverride_SceneFringeIntensity=true; S.SceneFringeIntensity=0.f;
  S.bOverride_MotionBlurAmount=true; S.MotionBlurAmount=.2f;           // readable trickshot spins
 }
 bApplied=true;
 // Until the player picks a graphics preset, keep the full-quality look.
 const FCrosshairSettings& Settings=World.GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 ApplyQuality(Settings.bGraphicsConfigured ? Settings.GraphicsPreset : 3,Settings.bGraphicsConfigured ? CrosshairGraphics::IsRayTracingActive() : true);
 UE_LOG(LogTemp,Display,TEXT("Crosshair environment applied to %s"),*World.GetMapName());
}

void UCrosshairEnvironmentSubsystem::ApplyQuality(int32 Preset,bool bRayTracing)
{
 if (Clouds) Clouds->SetActorHiddenInGame(Preset<=0);   // volumetric clouds are the costliest sky feature on integrated GPUs
 if (!Grade) return;
 FPostProcessSettings& S=Grade->Settings;
 const float Quality=Preset>=3 ? 1.5f : 1.f;
 S.bOverride_LumenSceneLightingQuality=true; S.LumenSceneLightingQuality=Quality;
 S.bOverride_LumenFinalGatherQuality=true; S.LumenFinalGatherQuality=Preset>=3 ? 2.f : 1.f;
 S.bOverride_LumenReflectionQuality=true; S.LumenReflectionQuality=Quality;
 // Hit-lit reflections need hardware ray tracing; otherwise use the default (surface cache) lighting.
 S.bOverride_LumenRayLightingMode=bRayTracing && Preset>=2;
 S.LumenRayLightingMode=ELumenRayLightingModeOverride::HitLightingForReflections;
 S.bOverride_AmbientOcclusionIntensity=true; S.AmbientOcclusionIntensity=Preset<=0 ? .4f : .6f;
}
