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
	UFUNCTION(BlueprintCallable) void ToggleMenu();
	UPROPERTY(BlueprintReadOnly) bool bMenuOpen = false;
	int32 MenuSelection = 0;
	TArray<FString> GetMenuRows() const;
private:
	void AdjustSelection(int32 Direction);
	void ActivateSelection();
};

/** Small native prototype HUD: packaged-build feedback without debug drawing. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairHUD : public AHUD
{
	GENERATED_BODY()
public:
	virtual void DrawHUD() override;
};
