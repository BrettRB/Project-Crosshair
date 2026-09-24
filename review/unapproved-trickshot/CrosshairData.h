#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "CrosshairData.generated.h"

class UInputAction;
class UInputMappingContext;
class USkeletalMesh;
class USoundBase;

/** A weapon's editable design, separate from the changing state of an equipped weapon. */
UCLASS(BlueprintType)
class PROJECT_CROSSHAIR_API UCrosshairWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAutomatic = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MagazineSize = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.03")) float ShotInterval = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1")) float ReloadSeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float EquipSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float Range = 50000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float HipSpreadDegrees = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float AimSpreadDegrees = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoilDegrees = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="10", ClampMax="110")) float AimFOV = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float AimSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> FireSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector HipOffset = FVector(45, 16, -16);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector AimOffset = FVector(45, 0, -9);
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator MeshRotation = FRotator(0, -90, 0);
};

/** Assets that connect physical buttons to gameplay intentions. */
UCLASS(BlueprintType)
class PROJECT_CROSSHAIR_API UCrosshairInputConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere) TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Move;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> MouseLook;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> StickLook;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Jump;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Sprint;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Crouch;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Fire;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Aim;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Reload;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> SwitchWeapon;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Placement;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> RotateTarget;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> RemoveTarget;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> ClearTargets;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> SaveStart;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Reset;
	UPROPERTY(EditAnywhere) TObjectPtr<UInputAction> Menu;
};

USTRUCT(BlueprintType)
struct FCrosshairSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickYawSpeed = 360.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickPitchSpeed = 240.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickDeadZone = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickExponent = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MouseSensitivity = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimSensitivity = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FieldOfView = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bContinuousPractice = false;
};

USTRUCT(BlueprintType)
struct FCrosshairReplayEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Map;
	UPROPERTY(BlueprintReadOnly) FString RecordedAt;
	UPROPERTY(BlueprintReadOnly) float HitSeconds = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 FormatVersion = 1;
};

UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY() FCrosshairSettings Settings;
	UPROPERTY() TArray<FCrosshairReplayEntry> Replays;
};

namespace CrosshairRules
{
	// Radial dead zone: preserve stick direction and rescale the remaining usable travel.
	inline FVector2D FilterStick(FVector2D Input, float DeadZone, float Exponent)
	{
		const float Length = Input.Size();
		DeadZone = FMath::Clamp(DeadZone, 0.f, 0.9f);
		if (Length <= DeadZone) return FVector2D::ZeroVector;
		return Input / Length * FMath::Pow(FMath::Clamp((Length - DeadZone) / (1.f - DeadZone), 0.f, 1.f), FMath::Max(0.1f, Exponent));
	}
	inline bool CanFire(int32 Ammo, bool bReloading, double Now, double NextShot)
	{
		return Ammo > 0 && !bReloading && Now + UE_SMALL_NUMBER >= NextShot;
	}
}
