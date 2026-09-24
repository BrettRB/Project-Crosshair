#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CrosshairPractice.generated.h"

class ACrosshairDummy;

UCLASS(ClassGroup=(Crosshair), meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairPlacementComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCrosshairPlacementComponent();
	virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
	UFUNCTION(BlueprintCallable) void Toggle();
	UFUNCTION(BlueprintCallable) void Confirm();
	UFUNCTION(BlueprintCallable) void Rotate();
	UFUNCTION(BlueprintCallable) void RemoveAimedTarget();
	UFUNCTION(BlueprintCallable) void ClearTargets();
	UPROPERTY(BlueprintReadOnly) bool bPlacing = false;
	UPROPERTY(BlueprintReadOnly) bool bValidPlacement = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PlacementRange = 10000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaximumTargets = 20;
	FTransform Preview;
	TArray<FTransform> GetLayout() const;
	void RestoreLayout(const TArray<FTransform>& Layout);
	static bool IsSupportedSurface(const FVector& Normal) { return Normal.Z >= 0.7f; }
};

UCLASS(ClassGroup=(Crosshair), meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairAttemptComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	UFUNCTION(BlueprintCallable) void SaveStart();
	UFUNCTION(BlueprintCallable) void ResetAttempt();
	void HandleTargetHit(ACrosshairDummy* Target);
	UPROPERTY(BlueprintReadOnly) FTransform StartTransform;
	UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
};
