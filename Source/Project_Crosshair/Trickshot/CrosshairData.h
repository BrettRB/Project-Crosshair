#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "CrosshairBotMatch.h"
#include "CrosshairData.generated.h"

class UInputAction;
class UInputMappingContext;
class USkeletalMesh;
class UStaticMesh;
class USoundBase;
class UMaterialInterface;

/** Cosmetic-only material overrides; an empty list keeps the authored mesh materials. */
USTRUCT(BlueprintType)
struct FCrosshairWeaponSkin
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UMaterialInterface>> Materials;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor StockColor = FLinearColor(.055f,.075f,.045f);
};

UENUM(BlueprintType)
enum class ECrosshairLethalType : uint8 { Frag, Tomahawk };

/** A weapon's editable design, separate from the changing state of an equipped weapon. */
UENUM(BlueprintType)
enum class ECrosshairAimStyle : uint8 { IronSights, Scope };

UCLASS(BlueprintType)
class PROJECT_CROSSHAIR_API UCrosshairWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming") ECrosshairAimStyle AimStyle = ECrosshairAimStyle::IronSights;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float BodyDamage = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Damage", meta=(ClampMin="0")) float HeadDamage = 100.f;
	/** Thin-wall penetration measures each solid's entry-to-exit distance in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Penetration", meta=(ClampMin="0")) float PenetrationDepth = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Penetration", meta=(ClampMin="0", ClampMax="4")) int32 PenetrationLayers = 2;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Penetration", meta=(ClampMin="0", ClampMax="1")) float PenetrationDamageScale = .75f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bAutomatic = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MagazineSize = 5;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.03")) float ShotInterval = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.1")) float ReloadSeconds = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float EquipSeconds = 0.25f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0.05",ClampMax="1")) float StowSeconds=.18f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Animation",meta=(ClampMin="0.05",ClampMax="1")) float DrawSeconds=.22f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="100")) float Range = 50000.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float HipSpreadDegrees = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) float AimSpreadDegrees = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) float RecoilDegrees = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="10", ClampMax="110")) float AimFOV = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.01")) float AimSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMesh> Mesh;
	/** Optional rigid model, in camera axes (X forward, Z up), with the grip at the origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TObjectPtr<UStaticMesh> PresentationMesh;
	/** Disable when an authored model includes its own sights, scope and stock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") bool bUsePrototypeGeometry = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Appearance") TArray<FCrosshairWeaponSkin> Skins;
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
/** Required actions are checked before any bindings are installed. */
	bool IsComplete() const
	{
		return Mapping && Move && MouseLook && StickLook && Jump && Sprint && Crouch && Fire && Aim && Reload && SwitchWeapon && Placement && RotateTarget && RemoveTarget && ClearTargets && SaveStart && Reset;
	}
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
struct FCrosshairClass
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FString Name=TEXT("Custom class");
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Primary=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 Secondary=1;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairLethalType Lethal=ECrosshairLethalType::Frag;
};

USTRUCT(BlueprintType)
struct FCrosshairSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickYawSpeed = 720.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickPitchSpeed = 540.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickDeadZone = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickExponent = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MouseSensitivity = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimSensitivity = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FieldOfView = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bContinuousPractice = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInvertControllerHorizontal = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInvertControllerVertical = false;
	/** Hardware axis direction learned by the explicit Controls calibration. */
	UPROPERTY() FVector2D ControllerAxisDirection = FVector2D(1, 1);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<FName, FName> WeaponSkins;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ECrosshairLethalType LethalType = ECrosshairLethalType::Frag;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCrosshairClass> Classes;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 ActiveClass=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bSandboxTargets=false;
 UPROPERTY() int32 ControlPresetVersion=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FCrosshairBotMatchOptions BotMatch;
};

USTRUCT(BlueprintType)
struct FCrosshairReplayEntry
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Map;
	UPROPERTY(BlueprintReadOnly) FString RecordedAt;
 UPROPERTY() FString ImportedMapId;
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
	/** Stick rate integrates over time, so the same input turns equally at different frame rates. */
	inline FVector2D StickDelta(FVector2D Input, const FCrosshairSettings& Settings, float AimAlpha, float DeltaSeconds)
	{
		const FVector2D Axis = FilterStick(Input * Settings.ControllerAxisDirection, Settings.StickDeadZone, Settings.StickExponent);
		const float Scale = DeltaSeconds * FMath::Lerp(1.f, Settings.AimSensitivity, AimAlpha);
		return FVector2D(Axis.X * Settings.StickYawSpeed * (Settings.bInvertControllerHorizontal ? -1.f : 1.f),
			Axis.Y * Settings.StickPitchSpeed * (Settings.bInvertControllerVertical ? -1.f : 1.f)) * Scale;
	}
	/** Raw mouse displacement is already integrated; never multiply by frame time. */
	inline FVector2D MouseDelta(FVector2D Input, const FCrosshairSettings& Settings, float AimAlpha)
	{
		return Input * Settings.MouseSensitivity * FMath::Lerp(1.f, Settings.AimSensitivity, AimAlpha);
	}
	inline float RemainingHealth(float Health, float Damage)
	{
		const float Remaining = FMath::Max(0.f, Health - Damage);
		return Remaining <= .001f ? 0.f : Remaining;
	}
	inline bool CanFire(int32 Ammo, bool bReloading, double Now, double NextShot)
	{
		return Ammo > 0 && !bReloading && Now + UE_SMALL_NUMBER >= NextShot;
	}
}
