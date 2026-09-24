#include "CrosshairReplaySubsystem.h"
#include "CrosshairCharacter.h"
#include "CrosshairPractice.h"
#include "CrosshairWeapon.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LocalFileNetworkReplayStreaming.h"
#include "NetworkReplayStreaming.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

namespace { const TCHAR* SaveSlot = TEXT("CrosshairProfile_v1"); }

void UCrosshairReplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Save = Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0));
	if (!Save) Save = Cast<UCrosshairSaveGame>(UGameplayStatics::CreateSaveGameObject(UCrosshairSaveGame::StaticClass()));
}
void UCrosshairReplaySubsystem::Deinitialize()
{
	StopRecording(false);
	SaveSettings();
	Super::Deinitialize();
}
void UCrosshairReplaySubsystem::SaveSettings()
{
	if (!Save) return;
	FCrosshairSettings& S = Save->Settings;
	S.StickYawSpeed = FMath::Clamp(S.StickYawSpeed, 60.f, 1080.f);
	S.StickPitchSpeed = FMath::Clamp(S.StickPitchSpeed, 60.f, 720.f);
	S.StickDeadZone = FMath::Clamp(S.StickDeadZone, 0.f, 0.4f);
	S.StickExponent = FMath::Clamp(S.StickExponent, 0.5f, 3.f);
	S.MouseSensitivity = FMath::Clamp(S.MouseSensitivity, 0.01f, 1.f);
	S.AimSensitivity = FMath::Clamp(S.AimSensitivity, 0.05f, 1.f);
	S.FieldOfView = FMath::Clamp(S.FieldOfView, 65.f, 110.f);
	if (!UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0)) Report(TEXT("Unable to save settings/replay list. Check free disk space."));
}
void UCrosshairReplaySubsystem::Report(const FString& Message)
{
	Status = Message;
	UE_LOG(LogTemp, Display, TEXT("Crosshair: %s"), *Message);
	if (LivePlayer.IsValid()) LivePlayer->Notify(Message);
}
void UCrosshairReplaySubsystem::PracticeReady(ACrosshairCharacter* Player)
{
	if (IsPlayback() || Player->IsReplayPlayback()) return;
	LivePlayer = Player;
	if (Phase == ECrosshairReplayPhase::Returning && bHaveSession)
	{
		Player->Placement->RestoreLayout(ReturnTargets);
		Player->Attempt->StartTransform = ReturnStart;
		Player->Inventory->Equip(ReturnWeapon);
	}
	ReturnMap = Player->GetWorld()->GetOutermost()->GetName();
	Phase = ECrosshairReplayPhase::Idle;
	Player->Attempt->ResetAttempt();
}
void UCrosshairReplaySubsystem::CaptureSession()
{
	if (!LivePlayer.IsValid()) return;
	ReturnStart = LivePlayer->Attempt->StartTransform;
	ReturnTargets = LivePlayer->Placement->GetLayout();
	ReturnWeapon = LivePlayer->Inventory->ActiveIndex;
	bHaveSession = true;
}
void UCrosshairReplaySubsystem::StopRecording(bool bKeep)
{
	UWorld* World = GetWorld();
	if (World && World->GetDemoNetDriver() && World->GetDemoNetDriver()->IsRecording())
	{
		FinishingStreamer = StaticCastSharedPtr<FLocalFileNetworkReplayStreamer>(World->GetDemoNetDriver()->GetReplayStreamer());
		GetGameInstance()->StopRecordingReplay();
	}
	if (!bKeep && !Current.Name.IsEmpty()) PendingDeletes.AddUnique(Current.Name);
	if (!bKeep) Current = FCrosshairReplayEntry();
}
void UCrosshairReplaySubsystem::BeginAttempt()
{
	if (IsPlayback()) return;
	StopRecording(false);
	Phase = Save->Settings.bContinuousPractice ? ECrosshairReplayPhase::Idle : ECrosshairReplayPhase::StartPending;
	PhaseStarted = FPlatformTime::Seconds();
}
void UCrosshairReplaySubsystem::CompleteAttempt()
{
	if (Phase != ECrosshairReplayPhase::Recording || !GetWorld()->GetDemoNetDriver())
	{
		Report(TEXT("Hit confirmed. Replay is not ready; reset to start another attempt."));
		return;
	}
	CaptureSession();
	Current.HitSeconds = GetWorld()->GetDemoNetDriver()->GetDemoCurrentTime();
	Phase = ECrosshairReplayPhase::Tail;
	TailEndsAt = GetWorld()->GetTimeSeconds() + 1.0;
	Report(TEXT("Hit confirmed — saving replay"));
}
void UCrosshairReplaySubsystem::PlaySaved(int32 Index)
{
	if (!Save->Replays.IsValidIndex(Index) || IsFinishing()) return;
	const FCrosshairReplayEntry Entry = Save->Replays[Index];
	if (Entry.FormatVersion != ReplayFormatVersion || !FPackageName::DoesPackageExist(Entry.Map))
	{
		Report(TEXT("Replay unavailable: incompatible version or missing map."));
		return;
	}
	CaptureSession();
	StopRecording(false);
	StartPlayback(Entry);
}
void UCrosshairReplaySubsystem::StartPlayback(const FCrosshairReplayEntry& Entry)
{
	Viewing = Entry;
	Phase = ECrosshairReplayPhase::Loading;
	PhaseStarted = FPlatformTime::Seconds();
	bSeekStarted = false;
	bSeekComplete = false;
	if (LivePlayer.IsValid()) LivePlayer->StopActions();
	if (!GetGameInstance()->PlayReplay(Entry.Name, nullptr, {TEXT("ReplayStreamerOverride=LocalFileNetworkReplayStreaming")}))
	{
		Report(TEXT("Replay could not be opened. Returning to practice."));
		ReturnToPractice();
	}
}
void UCrosshairReplaySubsystem::ReplayAgain() { if (IsPlayback()) StartPlayback(Viewing); }
void UCrosshairReplaySubsystem::ReturnToPractice()
{
	if (!IsPlayback()) return;
	Phase = ECrosshairReplayPhase::Returning;
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*ReturnMap));
}
void UCrosshairReplaySubsystem::DeleteSaved(int32 Index)
{
	if (!Save->Replays.IsValidIndex(Index) || IsPlayback() || IsFinishing()) return;
	const FString Name = Save->Replays[Index].Name;
	auto Streamer = FNetworkReplayStreaming::Get().GetFactory(TEXT("LocalFileNetworkReplayStreaming")).CreateReplayStreamer();
	Streamer->DeleteFinishedStream(Name, FDeleteFinishedStreamCallback::CreateWeakLambda(this, [this, Name, Streamer](const FDeleteFinishedStreamResult& Result)
	{
		if (Result.WasSuccessful())
		{
			Save->Replays.RemoveAll([&Name](const FCrosshairReplayEntry& E) { return E.Name == Name; });
			SaveSettings();
			Report(TEXT("Replay deleted"));
		}
		else Report(TEXT("Could not delete replay. It may still be in use."));
	}));
}
void UCrosshairReplaySubsystem::Tick(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld()) return;
	const double Now = FPlatformTime::Seconds();
	// StopStreaming flushes asynchronously. Never play or delete a file until its writer is done.
	const bool bWriterReady = !FinishingStreamer || (!FinishingStreamer->IsStreaming() && !FinishingStreamer->HasPendingFileRequests());
	if (bWriterReady)
	{
		FinishingStreamer.Reset();
		for (const FString& Name : PendingDeletes)
		{
			auto Streamer = FNetworkReplayStreaming::Get().GetFactory(TEXT("LocalFileNetworkReplayStreaming")).CreateReplayStreamer();
			Streamer->DeleteFinishedStream(Name, FDeleteFinishedStreamCallback::CreateLambda([Streamer](const FDeleteFinishedStreamResult&) {}));
		}
		PendingDeletes.Empty();
	}
	if (Phase == ECrosshairReplayPhase::StartPending && bWriterReady && Now - PhaseStarted > 0.3)
	{
		Current.Name = TEXT("Crosshair_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		Current.Map = ReturnMap;
		Current.RecordedAt = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
		Current.FormatVersion = ReplayFormatVersion;
		if (IConsoleVariable* Rate = IConsoleManager::Get().FindConsoleVariable(TEXT("demo.RecordHz"))) Rate->Set(60.f, ECVF_SetByCode);
		GetGameInstance()->StartRecordingReplay(Current.Name, Current.RecordedAt, {TEXT("ReplayStreamerOverride=LocalFileNetworkReplayStreaming")});
		Phase = World->GetDemoNetDriver() && World->GetDemoNetDriver()->IsRecording() ? ECrosshairReplayPhase::Recording : ECrosshairReplayPhase::Idle;
		Report(Phase == ECrosshairReplayPhase::Recording ? TEXT("Attempt ready") : TEXT("Replay recording unavailable. Try Standalone Game or the Windows build."));
	}
	else if (Phase == ECrosshairReplayPhase::Recording && World->GetDemoNetDriver() && World->GetDemoNetDriver()->GetDemoCurrentTime() > 120)
	{
		// Bound temporary recording size for long idle/practice sessions without moving the player.
		BeginAttempt();
	}
	else if (Phase == ECrosshairReplayPhase::Tail && World->GetTimeSeconds() >= TailEndsAt)
	{
		StopRecording(true);
		Phase = ECrosshairReplayPhase::Finalizing;
		PhaseStarted = Now;
	}
	else if (Phase == ECrosshairReplayPhase::Finalizing && bWriterReady)
	{
		Save->Replays.Insert(Current, 0);
		SaveSettings();
		const FCrosshairReplayEntry Entry = Current;
		Current = FCrosshairReplayEntry();
		StartPlayback(Entry);
	}
	else if (Phase == ECrosshairReplayPhase::Finalizing && Now - PhaseStarted > 20)
	{
		Report(TEXT("Replay storage is taking too long. Check disk space, then reset."));
		Phase = ECrosshairReplayPhase::Idle;
	}
	else if (IsPlayback())
	{
		UDemoNetDriver* Demo = World->GetDemoNetDriver();
		if (Demo && Demo->IsPlaying())
		{
			if (!bSeekStarted && Demo->GetDemoTotalTime() > 0)
			{
				bSeekStarted = true;
				Demo->GotoTimeInSeconds(FMath::Max(0.f, Viewing.HitSeconds - 8.f), FOnGotoTimeDelegate::CreateWeakLambda(this, [this](bool Success)
				{
					bSeekComplete = Success;
					if (!Success) { Report(TEXT("Replay seek failed")); ReturnToPractice(); }
				}));
			}
			if (bSeekComplete)
			{
				Phase = ECrosshairReplayPhase::Playing;
				for (TActorIterator<ACrosshairCharacter> It(World); It; ++It)
				{
					if (APlayerController* PC = World->GetFirstPlayerController()) PC->SetViewTarget(*It);
					break;
				}
				if (Demo->GetDemoCurrentTime() >= FMath::Min(Viewing.HitSeconds + 0.85f, Demo->GetDemoTotalTime() - 0.05f)) ReturnToPractice();
			}
		}
		if (Phase == ECrosshairReplayPhase::Loading && Now - PhaseStarted > 20)
		{
			Report(TEXT("Replay failed to load; the recording may be incompatible or damaged."));
			ReturnToPractice();
		}
	}
}
