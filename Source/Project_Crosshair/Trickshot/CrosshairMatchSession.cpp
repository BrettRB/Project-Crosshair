#include "CrosshairMatchSession.h"
#include "CrosshairMapLibrary.h"
#include "CrosshairReplaySubsystem.h"
#include "CrosshairStartup.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

namespace
{
 const FName CrosshairModeKey(TEXT("CROSSHAIR_MODE"));
 const FName CrosshairHostKey(TEXT("CROSSHAIR_HOST"));
 const FName CrosshairBotsKey(TEXT("CROSSHAIR_BOTS"));
 IOnlineSessionPtr Sessions(const UObject* Context) { return Online::GetSessionInterface(Context ? Context->GetWorld() : nullptr); }
}

void UCrosshairMatchSession::Deinitialize()
{
 ClearHandles();
 Super::Deinitialize();
}
FString UCrosshairMatchSession::TravelOptions(const FCrosshairBotMatchOptions& Options,bool bListen)
{
 // A LAN host flags the match so the game mode uses the LAN setup (bots, max players) instead of the offline one.
 return FString::Printf(TEXT("game=/Script/Project_Crosshair.CrosshairMatchGameMode?CrosshairMode=%s%s"),
  Options.Mode==ECrosshairMatchMode::TeamDeathmatch ? TEXT("TDM") : TEXT("FFA"),bListen ? TEXT("?listen?CrosshairLan") : TEXT(""));
}
bool UCrosshairMatchSession::CanTravel(FString& Reason) const
{
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 if (GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing()) { Reason=TEXT("Wait for game preparation to finish."); return false; }
 if (Replay->IsPlayback() || Replay->IsFinishing()) { Reason=TEXT("Return to practice and wait for replay saving first."); return false; }
 return true;
}
bool UCrosshairMatchSession::TravelToMatch(bool bListen)
{
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 auto& Settings=Replay->GetSettings();
 UCrosshairBotMatch::Validate(Settings.BotMatch); UCrosshairBotMatch::Validate(Settings.Lan);
 const FCrosshairBotMatchOptions& Options=bListen ? Settings.Lan.Match : Settings.BotMatch;
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 Library->bHomeVisited=true; Library->ActiveMapId.Reset();
 return Replay->ChangePracticeMap(UCrosshairBotMatch::MapPackage(Options.Map),TravelOptions(Options,bListen));
}
bool UCrosshairMatchSession::StartOfflineMatch() { return TravelToMatch(false); }
bool UCrosshairMatchSession::HasLanSession() const
{
 const IOnlineSessionPtr Interface=Sessions(this);
 return Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession)!=nullptr;
}
bool UCrosshairMatchSession::HostLanMatch()
{
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 const IOnlineSessionPtr Interface=Sessions(this);
 if (!Interface.IsValid()) { LanStatus=TEXT("LAN unavailable: online subsystem not loaded."); return false; }
 if (Interface->GetNamedSession(NAME_GameSession)) Interface->DestroySession(NAME_GameSession);
 auto& Saved=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 UCrosshairBotMatch::Validate(Saved.Lan);
 const auto& Options=Saved.Lan.Match;
 FOnlineSessionSettings Settings;
 Settings.bIsLANMatch=true; Settings.bShouldAdvertise=true; Settings.bAllowJoinInProgress=true;
 Settings.bUsesPresence=false; Settings.bUseLobbiesIfAvailable=false;
 Settings.NumPublicConnections=Saved.Lan.MaxPlayers;   // human players, including the host
 Settings.Set(CrosshairHostKey,UCrosshairBotMatch::CleanPlayerName(Saved.PlayerName),EOnlineDataAdvertisementType::ViaOnlineService);
 Settings.Set(CrosshairBotsKey,FString(Options.bBots ? TEXT("bots on") : TEXT("no bots")),EOnlineDataAdvertisementType::ViaOnlineService);
 Settings.Set(SETTING_MAPNAME,UCrosshairBotMatch::MapName(Options.Map),EOnlineDataAdvertisementType::ViaOnlineService);
 Settings.Set(CrosshairModeKey,UCrosshairBotMatch::ModeName(Options.Mode),EOnlineDataAdvertisementType::ViaOnlineService);
 ClearHandles();
 CreateHandle=Interface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this,&UCrosshairMatchSession::OnCreateComplete));
 const ULocalPlayer* Player=GetGameInstance()->GetFirstGamePlayer();
 LanStatus=TEXT("Creating LAN match...");
 if (!Player || !Interface->CreateSession(*Player->GetPreferredUniqueNetId(),NAME_GameSession,Settings))
 { ClearHandles(); LanStatus=TEXT("Could not create LAN match."); return false; }
 return true;
}
void UCrosshairMatchSession::OnCreateComplete(FName Session,bool bSuccess)
{
 ClearHandles();
 if (!bSuccess) { LanStatus=TEXT("Could not create LAN match."); return; }
 LanStatus=TEXT("Hosting LAN match");
 if (!TravelToMatch(true)) if (const IOnlineSessionPtr Interface=Sessions(this)) Interface->DestroySession(NAME_GameSession);
}
bool UCrosshairMatchSession::FindLanMatches()
{
 const IOnlineSessionPtr Interface=Sessions(this);
 if (!Interface.IsValid() || bSearching) return false;
 Search=MakeShared<FOnlineSessionSearch>();
 Search->bIsLanQuery=true; Search->MaxSearchResults=20; Search->TimeoutInSeconds=4.f;
 ClearHandles();
 FindHandle=Interface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this,&UCrosshairMatchSession::OnFindComplete));
 const ULocalPlayer* Player=GetGameInstance()->GetFirstGamePlayer();
 bSearching=true; LanResults.Reset(); LanStatus=TEXT("Searching this network...");
 if (!Player || !Interface->FindSessions(*Player->GetPreferredUniqueNetId(),Search.ToSharedRef()))
 { ClearHandles(); bSearching=false; LanStatus=TEXT("LAN search failed to start."); return false; }
 return true;
}
void UCrosshairMatchSession::OnFindComplete(bool bSuccess)
{
 ClearHandles(); bSearching=false; LanResults.Reset();
 if (!bSuccess || !Search.IsValid()) { LanStatus=TEXT("LAN search failed."); return; }
 for (const FOnlineSessionSearchResult& Result:Search->SearchResults)
 {
  if (LanResults.Num()>=MaxLanResults) break;
  FString Map,Mode,Host,Bots;
  Result.Session.SessionSettings.Get(SETTING_MAPNAME,Map); Result.Session.SessionSettings.Get(CrosshairModeKey,Mode);
  if (!Result.Session.SessionSettings.Get(CrosshairHostKey,Host)) Host=Result.Session.OwningUserName;
  Result.Session.SessionSettings.Get(CrosshairBotsKey,Bots);
  const int32 Max=Result.Session.SessionSettings.NumPublicConnections;
  LanResults.Add(FString::Printf(TEXT("%s - %s on %s%s (%d/%d)"),*Host,*Mode,*Map,Bots.IsEmpty() ? TEXT("") : *(TEXT(", ")+Bots),Max-Result.Session.NumOpenPublicConnections,Max));
 }
 LanStatus=LanResults.IsEmpty() ? TEXT("No LAN matches found on this network.") : FString::Printf(TEXT("Found %d LAN match(es)."),LanResults.Num());
}
bool UCrosshairMatchSession::JoinLanMatch(int32 Index)
{
 const IOnlineSessionPtr Interface=Sessions(this);
 FString Reason;
 if (!Interface.IsValid() || !Search.IsValid() || !Search->SearchResults.IsValidIndex(Index) || !LanResults.IsValidIndex(Index)) return false;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 if (Interface->GetNamedSession(NAME_GameSession)) Interface->DestroySession(NAME_GameSession);
 ClearHandles();
 JoinHandle=Interface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this,&UCrosshairMatchSession::OnJoinComplete));
 const ULocalPlayer* Player=GetGameInstance()->GetFirstGamePlayer();
 LanStatus=TEXT("Joining LAN match...");
 if (!Player || !Interface->JoinSession(*Player->GetPreferredUniqueNetId(),NAME_GameSession,Search->SearchResults[Index]))
 { ClearHandles(); LanStatus=TEXT("Could not join LAN match."); return false; }
 return true;
}
void UCrosshairMatchSession::OnJoinComplete(FName Session,EOnJoinSessionCompleteResult::Type Result)
{
 ClearHandles();
 const IOnlineSessionPtr Interface=Sessions(this);
 FString Address;
 APlayerController* Controller=GetGameInstance()->GetFirstLocalPlayerController();
 if (Result!=EOnJoinSessionCompleteResult::Success || !Interface.IsValid() || !Interface->GetResolvedConnectString(Session,Address) || !Controller)
 { LanStatus=TEXT("Could not join LAN match."); return; }
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 Replay->SaveSettings();
 GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->bHomeVisited=true;
 LanStatus=TEXT("Connecting to ")+Address;
 Controller->ClientTravel(Address,TRAVEL_Absolute);
}
void UCrosshairMatchSession::LeaveMatch()
{
 if (const IOnlineSessionPtr Interface=Sessions(this); Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession))
  Interface->DestroySession(NAME_GameSession);
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 // Arrive on Home, as when the game first opens.
 Library->bHomeVisited=false; Library->ActiveMapId.Reset();
 GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/L_Practice"));
}
void UCrosshairMatchSession::ClearHandles()
{
 if (const IOnlineSessionPtr Interface=Sessions(this); Interface.IsValid())
 {
  Interface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
  Interface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
  Interface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
 }
}
