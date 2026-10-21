#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "CrosshairData.h"
#include "CrosshairPractice.h"
#include "CrosshairReplaySubsystem.generated.h"

class ACrosshairCharacter;
class FLocalFileNetworkReplayStreamer;

enum class ECrosshairReplayPhase : uint8 { Idle, StartPending, Starting, Recording, AwaitPlayback, Tail, Finalizing, Loading, Playing, Returning };

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
	/** Discards the active unsaved attempt and starts fresh on another map. Options are URL options such as a match game mode. */
	UFUNCTION(BlueprintCallable) bool ChangePracticeMap(FName Map, const FString& Options = TEXT(""));
	void CompleteAttempt();
	/** Starts continuous recording for the local player in a match (no-op if already recording). */
	void BeginMatchRecording(ACrosshairCharacter* Player);
	/** Saves the last moments up to this kill as a replay without interrupting play. Kills within the
	 *  one-second tail join the same clip. Returns false if the recorder was not ready. */
	bool SaveMatchKill(const FString& Label);
	bool IsMatchRecording() const { return bMatchSession; }
	UFUNCTION(BlueprintCallable) void PlaySaved(int32 Index);
	UFUNCTION(BlueprintCallable) void ReplayAgain();
	UFUNCTION(BlueprintCallable) void ReturnToPractice();
	UFUNCTION(BlueprintCallable) void DeleteSaved(int32 Index);
	UFUNCTION(BlueprintCallable) void SaveSettings();
	FCrosshairSettings& GetSettings() { return Save->Settings; }
	const TArray<FCrosshairReplayEntry>& GetReplays() const { return Save->Replays; }
	bool IsPlayback() const { return Phase == ECrosshairReplayPhase::AwaitPlayback || Phase == ECrosshairReplayPhase::Loading || Phase == ECrosshairReplayPhase::Playing; }
	bool IsFinishing() const { return Phase == ECrosshairReplayPhase::Tail || Phase == ECrosshairReplayPhase::Finalizing; }
	bool IsRecording() const { return Phase == ECrosshairReplayPhase::Recording; }
	FString Status;
private:
	TMap<FName,bool> ReturnBrokenWindows;
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
	TArray<FCrosshairTargetSnapshot> ReturnTargets;
	int32 ReturnWeapon = 0;
	bool bHaveSession = false;
	bool bMatchSession = false;
	bool bMatchClip = false;
	int32 ClipKills = 0;
	bool bFinalizeWarning = false;
	bool bSeekStarted = false;
	bool bSeekComplete = false;
	double PhaseStarted = 0;
	double TailEndsAt = 0;
	static constexpr int32 ReplayFormatVersion = 1;
};
