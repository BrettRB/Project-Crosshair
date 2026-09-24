#pragma once
#include "CoreMinimal.h"
#include "Project_CrosshairCharacter.h"
#include "CrosshairCharacter.generated.h"

class UCrosshairInventoryComponent;
class UCrosshairPlacementComponent;
class UCrosshairAttemptComponent;
class UCrosshairInputConfig;
class UAnimSequence;
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
};

UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairCharacter : public AProject_CrosshairCharacter
{
	GENERATED_BODY()
public:
	ACrosshairCharacter();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairPlacementComponent> Placement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCrosshairAttemptComponent> Attempt;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TObjectPtr<UCrosshairInputConfig> Inputs;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float WalkSpeed = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float SprintSpeed = 680;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") float CrouchSpeed = 220;
	UFUNCTION(BlueprintPure) bool CanAct() const;
	bool IsReplayPlayback() const;
	float GetAimAlpha() const { return AimAlpha; }
	void ApplyRecoil(float Degrees);
	void TargetHit(ACrosshairDummy* Target);
	void StopActions();
	void Notify(const FString& Text);
	FString Notice;
	float NoticeUntil = 0;
	float HitMarkerUntil = 0;
private:
	void Move(const FInputActionValue& Value);
	void MouseAim(const FInputActionValue& Value);
	void StickAim(const FInputActionValue& Value);
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
	UPROPERTY() TObjectPtr<UAnimSequence> IdleAnimation;
	UPROPERTY() TObjectPtr<UAnimSequence> ReloadAnimation;
	bool bAimHeld = false;
	bool bSprintHeld = false;
	bool bArmsReloading = false;
	float AimAlpha = 0;
	int32 PresentedHit = 0;
};
