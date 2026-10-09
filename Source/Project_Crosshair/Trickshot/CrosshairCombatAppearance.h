#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrosshairCombatAppearance.generated.h"
class UStaticMesh;
class UStaticMeshComponent;
/** Cosmetic bone-attached gear; no collision or gameplay modifiers. */
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairCombatAppearance : public UActorComponent
{
 GENERATED_BODY()
public:
 UCrosshairCombatAppearance();
 virtual void BeginPlay() override;
 UPROPERTY(EditDefaultsOnly,Category="Gear") TObjectPtr<UStaticMesh> ElbowPad;
 UPROPERTY(EditDefaultsOnly,Category="Gear") FTransform LeftPadTransform;
 UPROPERTY(EditDefaultsOnly,Category="Gear") FTransform RightPadTransform;
 UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Gear;
};
