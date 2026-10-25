#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CrosshairEnvironment.generated.h"

class ADirectionalLight;
class UMaterialInstanceDynamic;

/** A map's sky: time of day, atmosphere, haze and cloud weather. */
struct FCrosshairSkyProfile
{
 const TCHAR* Name=TEXT("Clear day");
 float SunPitch=0.f;            // degrees below the horizon the light points; 0 keeps the map's own
 float SunKelvin=5900.f, SunLux=6.5f;
 float Rayleigh=.0331f, Mie=.004f, MieAnisotropy=.8f, AerialDistance=1.f;
 float FogDensity=.006f; FLinearColor FogColor=FLinearColor(.18f,.22f,.28f);
 float CloudCoverage=-.2f, CloudDensity=.008f; FLinearColor CloudAlbedo=FLinearColor(.9f,.9f,.9f);
 float CloudBottomKm=5.f, CloudThicknessKm=10.f;
};
class APostProcessVolume;

/**
 * Consistent, physically based outdoor lighting for every game world: built-in maps, imported
 * user maps and replays. Adds whatever a map lacks (sun, sky atmosphere, real-time sky light,
 * height fog, volumetric clouds) and tunes what it has, plus a low-priority unbound post-process
 * grade (exposure, ambient occlusion, Lumen quality, film-like colour). Map post-process volumes
 * still take precedence; tag any actor "CrosshairCustomLighting" to opt a map out entirely.
 */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairEnvironmentSubsystem : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override { return WorldType==EWorldType::Game || WorldType==EWorldType::PIE; }
 virtual void OnWorldBeginPlay(UWorld& World) override;
 static const FName EnvironmentTag;
 static const FName OptOutTag;
 /** Sky for a map: Testing clear midday, Nuketown desert late afternoon, Grid cool broken-cloud morning; others a clear day. */
 static FCrosshairSkyProfile SkyProfileFor(const FString& MapName);
 FCrosshairSkyProfile Profile;
 bool IsApplied() const { return bApplied; }
 /** Scales the grade to a graphics preset (0..3): Lumen quality, hit-lit reflections only with ray tracing, clouds off on battery saver. */
 void ApplyQuality(int32 Preset,bool bRayTracing);
 UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Sun;
 UPROPERTY(Transient) TObjectPtr<APostProcessVolume> Grade;
 UPROPERTY(Transient) TObjectPtr<AActor> Clouds;
private:
 bool bApplied=false;
};
