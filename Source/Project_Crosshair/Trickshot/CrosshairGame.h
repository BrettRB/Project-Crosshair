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
 virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
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
	int32 GetFirstReplayRow() const;
	int32 GetLethalRow() const;
	int32 GetSkinCount() const;
 int32 GetClassRow() const { return GetLethalRow()+1; }
 int32 GetSandboxRow() const { return GetClassRow()+5; }
 int32 GetHomeRow() const { return GetClassRow()+6; }
 int32 GetImportRow() const { return GetClassRow()+7; }
 int32 GetBotMatchRow() const { return GetImportRow()+1; }
 /** Match rows from GetBotMatchRow(): difficulty, players, score, time, mode, map, start, host, find, then joinable LAN results. */
 static constexpr int32 MatchRowCount = 13;
 /** Graphics rows from GetGraphicsRow(): preset, ray tracing, resolution scale, detect for this PC. */
 int32 GetGraphicsRow() const { return GetBotMatchRow()+MatchRowCount; }
 static constexpr int32 GraphicsRowCount = 4;
 /** LAN setup rows from GetLanRow(): username, mode, map, bots, bot difficulty, bot count or team size, score, time, max players. */
 int32 GetLanRow() const { return GetGraphicsRow()+GraphicsRowCount; }
 static constexpr int32 LanRowCount = 9;
 /** Typing a username on the LAN tab (Enter starts and finishes). */
 bool bEditingName=false;
 /** Each player sends the username saved on their own machine; the server applies it. */
 UFUNCTION(Server,Reliable) void ServerSetPlayerName(const FString& Name);
 bool IsInMatch() const;
 bool bScoreboardHeld=false;
 void ShowHome();
 void StartPlaying();
 bool bHomeScreen=false,bFromHome=false;
 int32 EditingClass=0;
	int32 ControllerCalibrationStep = 0;
	int32 MenuSelection = 0;
	TArray<FString> GetMenuRows() const;
	TArray<int32> GetVisibleMenuRows() const;
	void SetMenuTab(int32 Tab);
	void ActivateMenuRow(int32 Row);
	void AdjustMenuRow(int32 Row, int32 Direction);
	int32 MenuTab = 0;
	static constexpr int32 MenuTabCount = 10;
	/** One-line explanation of a menu row, shown under the list. */
	FString GetRowHelp(int32 Row) const;
	/** Tabs whose practice tools are unavailable during a match. */
	bool IsTabLockedInMatch(int32 Tab) const { return IsInMatch() && (Tab==3 || Tab==4 || Tab==7); }
private:
	void ApplyGameplayInputMode();
	void CalibrateAxis(FKey Key, float Value);
	FVector2D CalibrationAxis = FVector2D::ZeroVector;
	FVector2D CalibrationDirection = FVector2D(1, 1);
	FVector2D PendingMouse = FVector2D::ZeroVector;
	FVector2D HeldStick = FVector2D::ZeroVector;
	void AdjustSelection(int32 Direction);
	void AdjustGraphics(int32 Offset,int32 Direction);
	void AdjustLan(int32 Offset,int32 Direction);
	void EditNameKey(const FKey& Key);
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
private:
	void DrawMatch(class ACrosshairMatchState* Match, float Scale);
	void DrawMenu(ACrosshairPlayerController* PC, class ACrosshairMatchState* Match);
};
