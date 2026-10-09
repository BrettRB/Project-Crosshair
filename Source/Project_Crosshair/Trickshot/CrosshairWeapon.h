#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "CrosshairWeapon.generated.h"

class UCrosshairWeaponDefinition;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class ACrosshairCharacter;

UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairWeapon : public AActor
{
	GENERATED_BODY()
public:
	ACrosshairWeapon();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void Initialize(UCrosshairWeaponDefinition* InDefinition);
	void SetEquipped(bool bEquipped);
 void BeginStow();
 void BeginDraw(float Delay);
 bool IsSwitching() const;
 void UpdateCarry();
	void StartFire();
	void StopFire();
	void Reload();
	void ResetWeapon();
	void UpdatePresentation(float AimAlpha);
	UFUNCTION(BlueprintCallable) bool SetSkin(FName Id);
	void CycleSkin(int32 Direction);
	UFUNCTION(BlueprintPure) FText GetSkinName() const;
	UPROPERTY(ReplicatedUsing=OnRep_Skin, BlueprintReadOnly) FName SkinId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> PresentationMesh;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Details;
	UPROPERTY(ReplicatedUsing=OnRep_Definition, BlueprintReadOnly) TObjectPtr<UCrosshairWeaponDefinition> Definition;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Ammo = 0;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bReloading = false;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bEquipped = false;
	UPROPERTY(Replicated) float ReloadStartedAt = 0;
 UPROPERTY(Replicated,BlueprintReadOnly) bool bStowing=false;
 UPROPERTY(Replicated,BlueprintReadOnly) bool bHolstered=false;
 UPROPERTY(Replicated) float StowStartedAt=-1000;
 UPROPERTY(Replicated) float DrawStartedAt=-1000;
protected:
	virtual void BeginPlay() override;
	UFUNCTION() void OnRep_Definition();
	UFUNCTION() void OnRep_Skin();
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
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UFUNCTION(BlueprintPure) ACrosshairWeapon* GetCurrent() const;
 ACrosshairWeapon* GetPresentationWeapon() const;
	UFUNCTION(BlueprintCallable) void Equip(int32 Index);
	void Cycle();
 void EquipSlot(int32 Slot);
 void ApplyClass();
 int32 PrimaryIndex=0,SecondaryIndex=1;
	void ResetWeapons();
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UCrosshairWeaponDefinition>> Loadout;
	UPROPERTY(Replicated, BlueprintReadOnly) TArray<TObjectPtr<ACrosshairWeapon>> Weapons;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 ActiveIndex = 0;
};
