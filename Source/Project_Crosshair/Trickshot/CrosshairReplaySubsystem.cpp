#include "CrosshairReplaySubsystem.h"
#include "CrosshairCharacter.h"
#include "CrosshairPractice.h"
#include "CrosshairWeapon.h"
#include "CrosshairWindow.h"
#include "CrosshairMapLibrary.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "LocalFileNetworkReplayStreaming.h"
#include "NetworkReplayStreaming.h"
#include "Misc/PackageName.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	const TCHAR* ProfileSlot()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmoke")) || FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmokeSaved")) ? TEXT("CrosshairSmoke_v1") : TEXT("CrosshairProfile_v1");
	}
}

void UCrosshairReplaySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FParse::Param(FCommandLine::Get(), TEXT("CrosshairSmoke"))) Save = Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(ProfileSlot(), 0));
	if (!Save) Save = Cast<UCrosshairSaveGame>(UGameplayStatics::CreateSaveGameObject(UCrosshairSaveGame::StaticClass()));
 auto& Settings=Save->Settings;
 if (Settings.ControlPresetVersion<1)
 {
  if (FMath::IsNearlyEqual(Settings.StickYawSpeed,360.f)) Settings.StickYawSpeed=720;
  if (FMath::IsNearlyEqual(Settings.StickPitchSpeed,240.f)) Settings.StickPitchSpeed=540;
  Settings.ControlPresetVersion=1;
 }
 if (Settings.Classes.IsEmpty()) for (int32 i=0;i<5;++i)
 { FCrosshairClass C; C.Name=FString::Printf(TEXT("Custom %d"),i+1); C.Primary=i%3; C.Secondary=(i+1)%3; C.Lethal=Settings.LethalType; Settings.Classes.Add(C); }
 Settings.ActiveClass=FMath::Clamp(Settings.ActiveClass,0,Settings.Classes.Num()-1);
 for (auto& C:Settings.Classes) { C.Primary=FMath::Clamp(C.Primary,0,2); C.Secondary=FMath::Clamp(C.Secondary,0,2); if (C.Secondary==C.Primary) C.Secondary=(C.Primary+1)%3; }

}
void UCrosshairReplaySubsystem::Deinitialize()
{
	const bool Keep = IsFinishing();
	StopRecording(Keep);
	if (Keep && !Current.Name.IsEmpty())
	{
		Save->Replays.Add(Current);
		Current = FCrosshairReplayEntry();
	}
	SaveSettings();
	Super::Deinitialize();
}
void UCrosshairReplaySubsystem::SaveSettings()
{
	if (!Save) return;
	FCrosshairSettings& S = Save->Settings;
	if (S.LethalType!=ECrosshairLethalType::Tomahawk) S.LethalType=ECrosshairLethalType::Frag;
	S.ControllerAxisDirection.X = S.ControllerAxisDirection.X < 0 ? -1 : 1;
	S.ControllerAxisDirection.Y = S.ControllerAxisDirection.Y < 0 ? -1 : 1;
	S.StickYawSpeed = FMath::Clamp(S.StickYawSpeed, 60.f, 2160.f);
	S.StickPitchSpeed = FMath::Clamp(S.StickPitchSpeed, 60.f, 1440.f);
	S.StickDeadZone = FMath::Clamp(S.StickDeadZone, 0.f, 0.4f);
	S.StickExponent = FMath::Clamp(S.StickExponent, 0.5f, 3.f);
	S.MouseSensitivity = FMath::Clamp(S.MouseSensitivity, 0.01f, 1.f);
	S.AimSensitivity = FMath::Clamp(S.AimSensitivity, 0.05f, 1.f);
	S.FieldOfView = FMath::Clamp(S.FieldOfView, 65.f, 110.f);
	if (!UGameplayStatics::SaveGameToSlot(Save, ProfileSlot(), 0)) Report(TEXT("Unable to save settings/replay list. Check free disk space."));
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
	const bool RestoreWindows = Phase == ECrosshairReplayPhase::Returning && bHaveSession;
	if (Phase == ECrosshairReplayPhase::Returning && bHaveSession)
	{
		Player->Placement->RestoreLayout(ReturnTargets);
		Player->Attempt->StartTransform = ReturnStart;
		Player->Inventory->Equip(ReturnWeapon);
	}
	ReturnMap = UWorld::RemovePIEPrefix(Player->GetWorld()->GetOutermost()->GetName());
	Phase = ECrosshairReplayPhase::Idle;
	Player->Attempt->ResetAttempt();
	if (RestoreWindows)
		for (TActorIterator<ACrosshairWindow> It(Player->GetWorld());It;++It)
			It->RestoreBroken(ReturnBrokenWindows.FindRef(It->GetFName()));
}
void UCrosshairReplaySubsystem::CaptureSession()
{
	if (!LivePlayer.IsValid()) return;
	ReturnStart = LivePlayer->Attempt->StartTransform;
	ReturnTargets = LivePlayer->Placement->GetLayout();
	ReturnWeapon = LivePlayer->Inventory->ActiveIndex;
	ReturnBrokenWindows.Reset();
	for (TActorIterator<ACrosshairWindow> It(LivePlayer->GetWorld());It;++It) ReturnBrokenWindows.Add(It->GetFName(),It->bBroken);
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
	if (IsPlayback() || IsFinishing()) return;
	StopRecording(false);
	Phase = Save->Settings.bContinuousPractice ? ECrosshairReplayPhase::Idle : ECrosshairReplayPhase::StartPending;
	PhaseStarted = FPlatformTime::Seconds();
}
bool UCrosshairReplaySubsystem::ChangePracticeMap(FName Map)
{
	if (IsPlayback() || IsFinishing())
	{
		Report(TEXT("Return to practice and wait for replay saving before changing maps."));
		return false;
	}
	if (!FPackageName::DoesPackageExist(Map.ToString()))
	{
		Report(TEXT("Map unavailable: the environment has not been installed."));
		return false;
	}
	if (LivePlayer.IsValid()) LivePlayer->StopActions();
	StopRecording(false);
	SaveSettings();
	bHaveSession = false;
	ReturnTargets.Empty();
	LivePlayer.Reset();
	Phase = ECrosshairReplayPhase::Idle;
	ReturnMap = Map.ToString();
	Status.Empty();
	UGameplayStatics::OpenLevel(GetGameInstance(), Map);
	return true;
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
	bFinalizeWarning = false;
	Phase = ECrosshairReplayPhase::Tail;
	TailEndsAt = GetWorld()->GetTimeSeconds() + 1.0;
	Report(TEXT("Hit confirmed — saving replay"));
}
void UCrosshairReplaySubsystem::PlaySaved(int32 Index)
{
	if (!Save->Replays.IsValidIndex(Index) || IsFinishing() || IsPlayback()) return;
	const FCrosshairReplayEntry Entry = Save->Replays[Index];
	if (Entry.FormatVersion != ReplayFormatVersion || !FPackageName::DoesPackageExist(Entry.Map) || (!Entry.ImportedMapId.IsEmpty() && !GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->Find(Entry.ImportedMapId)))
	{
		Report(TEXT("Replay unavailable: incompatible version or missing map."));
		return;
	}
	CaptureSession();
	StopRecording(false);
	Viewing = Entry;
	Phase = ECrosshairReplayPhase::AwaitPlayback;
	PhaseStarted = FPlatformTime::Seconds();
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
		Current.ImportedMapId=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->ActiveMapId;
	Current.Map = ReturnMap;
		Current.RecordedAt = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
		Current.FormatVersion = ReplayFormatVersion;
		if (IConsoleVariable* Rate = IConsoleManager::Get().FindConsoleVariable(TEXT("demo.RecordHz"))) Rate->Set(60.f, ECVF_SetByCode);
		GetGameInstance()->StartRecordingReplay(Current.Name, Current.RecordedAt, {TEXT("ReplayStreamerOverride=LocalFileNetworkReplayStreaming")});
		Phase = ECrosshairReplayPhase::Starting;
		PhaseStarted = Now;
	}
	else if (Phase == ECrosshairReplayPhase::Starting)
	{
		if (World->GetDemoNetDriver() && World->GetDemoNetDriver()->IsRecording())
		{
			Phase = ECrosshairReplayPhase::Recording;
			Report(TEXT("Attempt ready"));
		}
		else if (Now - PhaseStarted > 10)
		{
			StopRecording(false);
			Phase = ECrosshairReplayPhase::Idle;
			Report(TEXT("Replay recording unavailable. Use Standalone Game or Windows build; reset to retry."));
		}
	}
	else if (Phase == ECrosshairReplayPhase::AwaitPlayback)
	{
		if (bWriterReady) StartPlayback(Viewing);
		else if (Now - PhaseStarted > 20) { Report(TEXT("Previous recording is still closing; returning to practice.")); ReturnToPractice(); }
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
	else if (Phase == ECrosshairReplayPhase::Finalizing && Now - PhaseStarted > 20 && !bFinalizeWarning)
	{
		Report(TEXT("Replay storage is slow. Recording retained; practice controls restored."));
		bFinalizeWarning = true;
		if (LivePlayer.IsValid()) LivePlayer->Attempt->bSucceeded = false;
	}
	else if (IsPlayback())
	{
		UDemoNetDriver* Demo = World->GetDemoNetDriver();
		if (Demo && Demo->IsPlaying())
		{
			if (!bSeekStarted && Demo->GetDemoTotalTime() > 0)
			{
				bSeekStarted = true;
				// UE can buffer an entire short stream before processing its first packet.
				// A small nonzero seek processes that initial frame instead of stalling at zero.
				const float SeekTime = FMath::Max(0.1f, Viewing.HitSeconds - 8.f);
				Demo->GotoTimeInSeconds(SeekTime, FOnGotoTimeDelegate::CreateWeakLambda(this, [this](bool Success)
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
