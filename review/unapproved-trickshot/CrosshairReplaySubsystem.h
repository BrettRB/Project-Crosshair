#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairData.h"
#include "CrosshairReplaySubsystem.generated.h"

class ACrosshairCharacter;
class FLocalFileNetworkReplayStreamer;

enum class ECrosshairReplayPhase : uint8 { Idle, StartPending, Recording, Tail, Finalizing, Loading, Playing, Returning };

/** Persists across map travel. Uses Unreal replay files; these are not video files. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairReplaySubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UCrosshairReplaySubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate() && IsValid(Save); }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	void PracticeReady(ACrosshairCharacter* Player);
	void BeginAttempt();
	void CompleteAttempt();
	UFUNCTION(BlueprintCallable) void PlaySaved(int32 Index);
	UFUNCTION(BlueprintCallable) void ReplayAgain();
	UFUNCTION(BlueprintCallable) void ReturnToPractice();
	UFUNCTION(BlueprintCallable) void DeleteSaved(int32 Index);
	UFUNCTION(BlueprintCallable) void SaveSettings();
	FCrosshairSettings& GetSettings() { return Save->Settings; }
	const TArray<FCrosshairReplayEntry>& GetReplays() const { return Save->Replays; }
	bool IsPlayback() const { return Phase == ECrosshairReplayPhase::Loading || Phase == ECrosshairReplayPhase::Playing; }
	bool IsFinishing() const { return Phase == ECrosshairReplayPhase::Tail || Phase == ECrosshairReplayPhase::Finalizing; }
	bool IsRecording() const { return Phase == ECrosshairReplayPhase::Recording; }
	FString Status;
private:
	void CaptureSession();
	void StopRecording(bool bKeep);
	void StartPlayback(const FCrosshairReplayEntry& Entry);
	void Report(const FString& Message);
	UPROPERTY() TObjectPtr<UCrosshairSaveGame> Save;
	UPROPERTY() TWeakObjectPtr<ACrosshairCharacter> LivePlayer;
	ECrosshairReplayPhase Phase = ECrosshairReplayPhase::Idle;
	FCrosshairReplayEntry Current;
	FCrosshairReplayEntry Viewing;
	TSharedPtr<FLocalFileNetworkReplayStreamer> FinishingStreamer;
	TArray<FString> PendingDeletes;
	FString ReturnMap = TEXT("/Game/Crosshair/Maps/L_Practice");
	FTransform ReturnStart;
	TArray<FTransform> ReturnTargets;
	int32 ReturnWeapon = 0;
	bool bHaveSession = false;
	bool bSeekStarted = false;
	bool bSeekComplete = false;
	double PhaseStarted = 0;
	double TailEndsAt = 0;
	static constexpr int32 ReplayFormatVersion = 1;
};
