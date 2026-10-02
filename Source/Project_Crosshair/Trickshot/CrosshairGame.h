#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "CrosshairGame.generated.h"

/** Practice-only rules. Template game modes and maps stay independent. */
UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ACrosshairGameMode();
};

/** Owns menus even when replay playback has no possessed character. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	ACrosshairPlayerController();
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;
	virtual void BeginPlay() override;
	virtual void UpdateRotation(float DeltaTime) override;
	UFUNCTION(BlueprintCallable) void ToggleMenu();
	UPROPERTY(BlueprintReadOnly) bool bMenuOpen = false;
	static constexpr int32 WeaponRow = 13;
	static constexpr int32 MapRow = 14;
	static constexpr int32 QuitRow = 15;
	static constexpr int32 InvertHorizontalRow = 16;
	static constexpr int32 InvertVerticalRow = 17;
	static constexpr int32 SkinRow = 18;
	static constexpr int32 WoodlandRow = 19;
	static constexpr int32 DesertRow = 20;
	static constexpr int32 CalibrationRow = 21;
	static constexpr int32 FirstReplayRow = 22;
	int32 ControllerCalibrationStep = 0;
	int32 MenuSelection = 0;
	TArray<FString> GetMenuRows() const;
	TArray<int32> GetVisibleMenuRows() const;
	void SetMenuTab(int32 Tab);
	void ActivateMenuRow(int32 Row);
	void AdjustMenuRow(int32 Row, int32 Direction);
	int32 MenuTab = 0;
	static constexpr int32 MenuTabCount = 6;
private:
	void ApplyGameplayInputMode();
	void CalibrateAxis(FKey Key, float Value);
	FVector2D CalibrationAxis = FVector2D::ZeroVector;
	FVector2D CalibrationDirection = FVector2D(1, 1);
	FVector2D PendingMouse = FVector2D::ZeroVector;
	FVector2D HeldStick = FVector2D::ZeroVector;
	void AdjustSelection(int32 Direction);
	void ActivateSelection();
	int32 MapChoice = 0;
};

/** Small native prototype HUD: packaged-build feedback without debug drawing. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
};
