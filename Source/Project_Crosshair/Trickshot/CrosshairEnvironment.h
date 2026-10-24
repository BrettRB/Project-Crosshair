#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CrosshairEnvironment.generated.h"

class ADirectionalLight;
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
 bool IsApplied() const { return bApplied; }
 UPROPERTY(Transient) TObjectPtr<ADirectionalLight> Sun;
 UPROPERTY(Transient) TObjectPtr<APostProcessVolume> Grade;
private:
 bool bApplied=false;
};
