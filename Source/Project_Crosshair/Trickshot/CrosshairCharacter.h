#pragma once
#include "CoreMinimal.h"
#include "Project_CrosshairCharacter.h"
#include "CrosshairCharacter.generated.h"

class UCrosshairCombatAppearance;
class UCrosshairInventoryComponent;
class UCrosshairLethalComponent;
class UCrosshairTraversalComponent;
class UCrosshairPlacementComponent;
class UCrosshairAttemptComponent;
class UCrosshairHealthComponent;
class UCrosshairSoldierAppearance;
class UCrosshairInputConfig;
class UCrosshairWeaponDefinition;
class UAnimSequence;
class UMaterialInterface;
class ACrosshairDummy;

USTRUCT()
struct FCrosshairViewState
{
	GENERATED_BODY()
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() float FOV = 90;
	UPROPERTY() float AimAlpha = 0;
	UPROPERTY() int32 HitSequence = 0;
	UPROPERTY() bool bHeadshot = false;
	UPROPERTY() bool bWallbang = false;
};

UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairCharacter : public AProject_CrosshairCharacter
{
	GENERATED_BODY()
public:
	ACrosshairCharacter();
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool CanJumpWhileFalling() const override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairInventoryComponent> Inventory;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly) TObjectPtr<UCrosshairCombatAppearance> CombatAppearance;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairLethalComponent> Lethals;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairTraversalComponent> Traversal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairPlacementComponent> Placement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairAttemptComponent> Attempt;
	/** Match-only health; practice never damages players. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairHealthComponent> Health;
	/** Match soldier uniform on the world body (bots and other players). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairSoldierAppearance> MatchAppearance;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UCrosshairInputConfig> Inputs;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float WalkSpeed = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float SprintSpeed = 680;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float CrouchSpeed = 220;
	/** Prototype tuning. Speeds use cm/s; jump and gravity remain in CharacterMovement. */
	UPROPERTY(EditDefaultsOnly, Category="Loadout") TArray<TObjectPtr<UCrosshairWeaponDefinition>> DefaultLoadout;
	UPROPERTY(EditDefaultsOnly, Category="Targets") TSubclassOf<ACrosshairDummy> TargetClass;
	UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<UAnimSequence> BodyIdleAnimation;
	UPROPERTY(EditDefaultsOnly, Category="Presentation") TObjectPtr<UAnimSequence> ReloadAnimation;
	/** Arms-only masks preserve the mannequin materials without showing its torso. */
	UPROPERTY(EditDefaultsOnly, Category="Presentation") TArray<TObjectPtr<UMaterialInterface>> FirstPersonArmMaterials;
	UFUNCTION(BlueprintPure) bool CanAct() const;
	bool IsReplayPlayback() const;
	float GetAimAlpha() const { return AimAlpha; }
	void ApplyRecoil(float Degrees);
	void TargetHit(ACrosshairDummy* Target, bool bHeadshot = false, bool bWallbang = false);
	void StopActions();
	virtual void PossessedBy(AController* NewController) override;
	bool IsBot() const;
	bool IsInMatch() const;
	/** Bots and remote players present a third-person soldier carrying the weapon. */
	bool UsesWorldPresentation() const;
	void HandleDeath();
	/** Local player's match kill: labels it (tricks, weapon, distance) and saves it to Replays. */
	void RecordMatchKill(const ACrosshairCharacter* Victim, bool bHeadshot, bool bWallbang, const FString& Weapon, bool bScopedWeapon, bool bLethal);
	/** Absolute yaw turned within the last Window seconds (spin detection for 360s). */
	float GetRecentSpin(double Window = 1.5) const;
	/** Marks the recording player so replay playback follows them rather than a bot. */
	UPROPERTY(Replicated) bool bReplayFocus = false;
	FString TrickshotBanner;
	float TrickshotUntil = 0;
	void SetBotInput(bool bAim, bool bSprint) { bAimHeld = bAim; bSprintHeld = bSprint; }
	/** LAN client -> host: sprint and aim change movement speed, so the host must simulate the same intent. */
	UFUNCTION(Server, Reliable) void ServerSetMoveIntent(bool bSprint, bool bAim);
 void SelectWeaponSlot(int32 Slot);
 void ControllerSprintPressed();
	void ApplyLookInput(FVector2D MouseDelta, FVector2D StickAxis, float DeltaSeconds);
	void Notify(const FString& Text);
	FString Notice;
	float NoticeUntil = 0;
	float HitMarkerUntil = 0;
	bool bLastHitHeadshot = false;
	bool bLastHitWallbang = false;
private:
	void Move(const FInputActionValue& Value);
	void JumpPressed();
	void JumpReleased();
	void SprintPressed();
	void SprintReleased();
	void CrouchPressed();
	void FirePressed();
	void FireReleased();
	void AimPressed();
	void AimReleased();
	void ReloadPressed();
	void SwitchPressed();
	void PlacementPressed();
	void RotatePressed();
	void RemovePressed();
	void ClearPressed();
	void SavePressed();
	void ResetPressed();
	void MenuPressed();
	UPROPERTY(Replicated) FCrosshairViewState RecordedView;
	bool bAimHeld = false;
	bool bSprintHeld = false;
 bool bControllerSprint=false;
	bool bArmsReloading = false;
	FTransform ArmsFromGrip = FTransform::Identity;
	FDelegateHandle ArmsPoseHandle;
	void UpdateArmsPresentation();
	void ConfigureMatchBody();
	bool bMatchBodyConfigured = false;
	TArray<TPair<double, float>> YawHistory;
	float LastYaw = 0;
	bool bHasLastYaw = false;
	bool bSentSprint = false, bSentAim = false;
	float AimAlpha = 0;
	int32 PresentedHit = 0;
};
