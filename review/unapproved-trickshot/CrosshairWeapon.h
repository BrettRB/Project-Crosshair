#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "CrosshairWeapon.generated.h"

class UCrosshairWeaponDefinition;
class USkeletalMeshComponent;
class ACrosshairCharacter;

UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairWeapon : public AActor
{
	GENERATED_BODY()
public:
	ACrosshairWeapon();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	void Initialize(UCrosshairWeaponDefinition* InDefinition);
	void SetEquipped(bool bEquipped);
	void StartFire();
	void StopFire();
	void Reload();
	void ResetWeapon();
	void UpdatePresentation(float AimAlpha);
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(ReplicatedUsing=OnRep_Definition, BlueprintReadOnly) TObjectPtr<UCrosshairWeaponDefinition> Definition;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Ammo = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bReloading = false;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bEquipped = false;
	UPROPERTY(Replicated) float ReloadStartedAt = 0;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_Definition();
	UFUNCTION() void OnRep_Shot();
	void TryFire();
	UPROPERTY(ReplicatedUsing=OnRep_Shot) int32 ShotSequence = 0;
	UPROPERTY(Replicated) FVector_NetQuantize LastImpact;
	int32 PresentedShot = 0;
	bool bTriggerHeld = false;
	double NextShotAt = 0;
	double ReloadEndsAt = 0;
	float Kick = 0;
};

/** Owns the loadout. The Character only forwards input to this component. */
UCLASS(ClassGroup=(Crosshair), meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairInventoryComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCrosshairInventoryComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UFUNCTION(BlueprintPure) ACrosshairWeapon* GetCurrent() const;
	UFUNCTION(BlueprintCallable) void Equip(int32 Index);
	void Cycle();
	void ResetWeapons();
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UCrosshairWeaponDefinition>> Loadout;
	UPROPERTY(Replicated, BlueprintReadOnly) TArray<TObjectPtr<ACrosshairWeapon>> Weapons;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 ActiveIndex = 0;
};
