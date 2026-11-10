#include "CrosshairMatchSession.h"
#include "CrosshairMapLibrary.h"
#include "CrosshairMatch.h"
#include "CrosshairReplaySubsystem.h"
#include "CrosshairStartup.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "IOnlineSubsystemEOS.h"
#include "IEOSSDKManager.h"
#include "EOSSettings.h"
#include "eos_sdk.h"
#include "eos_connect.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace
{
 const FName CrosshairModeKey(TEXT("CROSSHAIR_MODE"));
 const FName CrosshairHostKey(TEXT("CROSSHAIR_HOST"));
 const FName CrosshairBotsKey(TEXT("CROSSHAIR_BOTS"));
 const FName CrosshairCodeKey(TEXT("CROSSHAIRCODE"));
 IOnlineSessionPtr GetMatchSessionInterface(const UObject* Context) { return Online::GetSessionInterface(Context ? Context->GetWorld() : nullptr); }
 IOnlineSubsystem* EpicSubsystem() { return UCrosshairMatchSession::IsOnlineConfigured() ? IOnlineSubsystem::Get(TEXT("EOS")) : nullptr; }
}
bool UCrosshairMatchSession::IsOnlineConfigured()
{
 // Read once, before the Epic subsystem first starts: the secret stays out of the project's checked-in config.
 static bool bRead=false,bConfigured=false;
 if (bRead) return bConfigured;
 bRead=true;
 FConfigFile File;
 File.Read(FPaths::ProjectContentDir()/TEXT("Online/CrosshairOnline.local.ini"));
 auto Value=[&](const TCHAR* Key){ FString Text; File.GetString(TEXT("Epic"),Key,Text); return Text.TrimStartAndEnd(); };
 FArtifactSettings Artifact;
 Artifact.ArtifactName=TEXT("Crosshair");
 Artifact.ProductId=Value(TEXT("ProductId")); Artifact.SandboxId=Value(TEXT("SandboxId")); Artifact.DeploymentId=Value(TEXT("DeploymentId"));
 Artifact.ClientId=Value(TEXT("ClientId")); Artifact.ClientSecret=Value(TEXT("ClientSecret")); Artifact.ClientEncryptionKey=Value(TEXT("EncryptionKey"));
 bConfigured=!Artifact.ProductId.IsEmpty() && !Artifact.SandboxId.IsEmpty() && !Artifact.DeploymentId.IsEmpty() && !Artifact.ClientId.IsEmpty() && !Artifact.ClientSecret.IsEmpty() && Artifact.ClientEncryptionKey.Len()==64;
 if (!bConfigured) return false;
 UEOSSettings* Settings=GetMutableDefault<UEOSSettings>();
 Settings->Artifacts={Artifact};
 Settings->DefaultArtifactName=Artifact.ArtifactName;
 Settings->bUseEAS=false; Settings->bUseEOSConnect=true; Settings->bUseEOSRTC=false; Settings->bEnableOverlay=false; Settings->bEnableSocialOverlay=false;
 return true;
}
IOnlineSessionPtr UCrosshairMatchSession::SessionInterface() const
{
 if (!bOnlineSession) return GetMatchSessionInterface(this);
 IOnlineSubsystem* Epic=EpicSubsystem();
 return Epic ? Epic->GetSessionInterface() : nullptr;
}
void UCrosshairMatchSession::SignInOnline(TFunction<void()> Then)
{
 IOnlineSubsystem* Epic=EpicSubsystem();
 const IOnlineIdentityPtr Identity=Epic ? Epic->GetIdentityInterface() : nullptr;
 auto* EpicInternal=static_cast<IOnlineSubsystemEOS*>(Epic);
 if (!Identity.IsValid() || !EpicInternal || !EpicInternal->GetEOSPlatformHandle().IsValid()) { LanStatus=TEXT("Online is unavailable: Epic services did not start."); return; }
 if (Identity->GetLoginStatus(0)==ELoginStatus::LoggedIn) { Then(); return; }
 LanStatus=TEXT("Signing in...");
 struct FDeviceContext { TWeakObjectPtr<UCrosshairMatchSession> Self; TFunction<void()> Then; };
 EOS_Connect_CreateDeviceIdOptions Options={};
 Options.ApiVersion=EOS_CONNECT_CREATEDEVICEID_API_LATEST;
 Options.DeviceModel="PC";
 EOS_Connect_CreateDeviceId(EOS_Platform_GetConnectInterface(*EpicInternal->GetEOSPlatformHandle()),&Options,new FDeviceContext{this,MoveTemp(Then)},[](const EOS_Connect_CreateDeviceIdCallbackInfo* Info)
 {
  TUniquePtr<FDeviceContext> Context(static_cast<FDeviceContext*>(Info->ClientData));
  UCrosshairMatchSession* Self=Context->Self.Get();
  if (!Self) return;
  // A device that signed in before already has its ID.
  if (Info->ResultCode!=EOS_EResult::EOS_Success && Info->ResultCode!=EOS_EResult::EOS_DuplicateNotAllowed) { Self->LanStatus=TEXT("Could not sign in to Epic services."); return; }
  const IOnlineIdentityPtr Identity=EpicSubsystem()->GetIdentityInterface();
  TFunction<void()> Next=MoveTemp(Context->Then);
  Self->LoginHandle=Identity->AddOnLoginCompleteDelegate_Handle(0,FOnLoginCompleteDelegate::CreateWeakLambda(Self,[Self,Identity,Next=MoveTemp(Next)](int32,bool bSuccess,const FUniqueNetId&,const FString& Error)
  {
   Identity->ClearOnLoginCompleteDelegate_Handle(0,Self->LoginHandle);
   if (!bSuccess) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online sign-in failed: %s"),*Error); Self->LanStatus=TEXT("Could not sign in to Epic services."); return; }
   Next();
  }));
  Identity->Login(0,FOnlineAccountCredentials(TEXT("externalauth:DeviceIdAccessToken"),TEXT(""),TEXT("")));
 });
}
bool UCrosshairMatchSession::HostOnlineMatch()
{
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 if (!IsOnlineConfigured()) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: fill in Content/Online/CrosshairOnline.local.ini")); LanStatus=TEXT("Online isn't available in this build."); return false; }
 bOnlineSession=true;
 SignInOnline([this](){ CreateOnlineSession(); });
 return true;
}
void UCrosshairMatchSession::CreateOnlineSession()
{
 const IOnlineSessionPtr Interface=SessionInterface();
 IOnlineSubsystem* Epic=EpicSubsystem();
 if (!Interface.IsValid() || !Epic) { LanStatus=TEXT("Online is unavailable."); return; }
 if (Interface->GetNamedSession(NAME_GameSession)) Interface->DestroySession(NAME_GameSession);
 auto& Saved=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 UCrosshairBotMatch::Validate(Saved.Lan);
 const auto& Options=Saved.Lan.Match;
 OnlineCode=CrosshairOnlineCode::Generate();
 FOnlineSessionSettings Settings;
 Settings.bIsLANMatch=false; Settings.bShouldAdvertise=true; Settings.bAllowJoinInProgress=true;
 Settings.bUsesPresence=false; Settings.bUseLobbiesIfAvailable=false;
 Settings.NumPublicConnections=Saved.Lan.MaxPlayers;
 Settings.Set(CrosshairCodeKey,OnlineCode,EOnlineDataAdvertisementType::ViaOnlineService);
 HostedChoice=Options.Map; HostedMap=UCrosshairBotMatch::ResolveMap(Options.Map);
 Settings.Set(SETTING_MAPNAME,UCrosshairBotMatch::MapName(HostedMap),EOnlineDataAdvertisementType::ViaOnlineService);
 Settings.Set(CrosshairModeKey,UCrosshairBotMatch::ModeName(Options.Mode),EOnlineDataAdvertisementType::ViaOnlineService);
 ClearHandles();
 CreateHandle=Interface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this,&UCrosshairMatchSession::OnCreateComplete));
 LanStatus=TEXT("Creating online match...");
 const FUniqueNetIdPtr Me=Epic->GetIdentityInterface()->GetUniquePlayerId(0);
 if (!Me.IsValid() || !Interface->CreateSession(*Me,NAME_GameSession,Settings)) { ClearHandles(); OnlineCode.Reset(); LanStatus=TEXT("Could not create the online match."); }
}
bool UCrosshairMatchSession::JoinOnlineMatch(const FString& Typed)
{
 const FString Code=CrosshairOnlineCode::Normalize(Typed);
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 if (!CrosshairOnlineCode::IsValid(Code)) { LanStatus=FString::Printf(TEXT("Enter the %d-character code from the host."),CrosshairOnlineCode::Length); return false; }
 if (!IsOnlineConfigured()) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: fill in Content/Online/CrosshairOnline.local.ini")); LanStatus=TEXT("Online isn't available in this build."); return false; }
 if (bSearching) return false;
 bOnlineSession=true; OnlineCode.Reset();
 SignInOnline([this,Code]()
 {
  const IOnlineSessionPtr Interface=SessionInterface();
  IOnlineSubsystem* Epic=EpicSubsystem();
  const FUniqueNetIdPtr Me=Epic ? Epic->GetIdentityInterface()->GetUniquePlayerId(0) : nullptr;
  if (!Interface.IsValid() || !Me.IsValid()) { LanStatus=TEXT("Online is unavailable."); return; }
  Search=MakeShared<FOnlineSessionSearch>();
  Search->bIsLanQuery=false; Search->MaxSearchResults=1; Search->TimeoutInSeconds=10.f;
  Search->QuerySettings.Set(CrosshairCodeKey,Code,EOnlineComparisonOp::Equals);
  ClearHandles();
  FindHandle=Interface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this,&UCrosshairMatchSession::OnFindOnlineComplete));
  bSearching=true; LanStatus=FString::Printf(TEXT("Looking for %s..."),*Code);
  if (!Interface->FindSessions(*Me,Search.ToSharedRef())) { ClearHandles(); bSearching=false; LanStatus=TEXT("Could not search for that code."); }
 });
 return true;
}
void UCrosshairMatchSession::OnFindOnlineComplete(bool bSuccess)
{
 ClearHandles(); bSearching=false;
 const IOnlineSessionPtr Interface=SessionInterface();
 IOnlineSubsystem* Epic=EpicSubsystem();
 const FUniqueNetIdPtr Me=Epic ? Epic->GetIdentityInterface()->GetUniquePlayerId(0) : nullptr;
 if (!bSuccess || !Search.IsValid() || !Interface.IsValid() || !Me.IsValid()) { LanStatus=TEXT("Could not search for that code."); return; }
 if (Search->SearchResults.IsEmpty()) { LanStatus=TEXT("No match found with that code."); return; }
 if (Interface->GetNamedSession(NAME_GameSession)) Interface->DestroySession(NAME_GameSession);
 JoinHandle=Interface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this,&UCrosshairMatchSession::OnJoinComplete));
 LanStatus=TEXT("Joining...");
 if (!Interface->JoinSession(*Me,NAME_GameSession,Search->SearchResults[0])) { ClearHandles(); LanStatus=TEXT("Could not join that match."); }
}

void UCrosshairMatchSession::Deinitialize()
{
 ClearHandles();
 Super::Deinitialize();
}
FString UCrosshairMatchSession::TravelOptions(const FCrosshairBotMatchOptions& Options,bool bListen,bool bOnline)
{
 // A LAN or Online host flags the match so the game mode uses that setup (bots, max players). bIsLanMatch keeps LAN on IP sockets.
 return FString::Printf(TEXT("game=/Script/Project_Crosshair.CrosshairMatchGameMode?CrosshairMode=%s%s"),
  Options.Mode==ECrosshairMatchMode::KillConfirmed ? TEXT("KC") : Options.Mode==ECrosshairMatchMode::TeamDeathmatch ? TEXT("TDM") : TEXT("FFA"),!bListen ? TEXT("") : bOnline ? TEXT("?listen?CrosshairLan?CrosshairOnline") : TEXT("?listen?CrosshairLan?bIsLanMatch"));
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
 // Random is rolled here for offline matches; a LAN host rolled it when advertising, so players see the real map.
 LastMatchMap=bListen ? HostedMap : UCrosshairBotMatch::ResolveMap(Options.Map);
 return Replay->ChangePracticeMap(UCrosshairBotMatch::MapPackage(LastMatchMap),TravelOptions(Options,bListen,bOnlineSession));
}
bool UCrosshairMatchSession::StartOfflineMatch() { return TravelToMatch(false); }
bool UCrosshairMatchSession::HasLanSession() const
{
 const IOnlineSessionPtr Interface=SessionInterface();
 return Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession)!=nullptr;
}
bool UCrosshairMatchSession::HostLanMatch()
{
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 bOnlineSession=false; OnlineCode.Reset();
 const IOnlineSessionPtr Interface=SessionInterface();
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
 HostedChoice=Options.Map; HostedMap=UCrosshairBotMatch::ResolveMap(Options.Map);
 Settings.Set(SETTING_MAPNAME,UCrosshairBotMatch::MapName(HostedMap),EOnlineDataAdvertisementType::ViaOnlineService);
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
 if (!bSuccess) { OnlineCode.Reset(); LanStatus=bOnlineSession ? TEXT("Could not create the online match.") : TEXT("Could not create LAN match."); return; }
 LanStatus=bOnlineSession ? TEXT("Hosting online match") : TEXT("Hosting LAN match");
 if (!TravelToMatch(true)) if (const IOnlineSessionPtr Interface=SessionInterface()) Interface->DestroySession(NAME_GameSession);
}
bool UCrosshairMatchSession::FindLanMatches()
{
 bOnlineSession=false;
 const IOnlineSessionPtr Interface=SessionInterface();
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
 const IOnlineSessionPtr Interface=SessionInterface();
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
 const IOnlineSessionPtr Interface=SessionInterface();
 FString Address;
 APlayerController* Controller=GetGameInstance()->GetFirstLocalPlayerController();
 if (Result!=EOnJoinSessionCompleteResult::Success || !Interface.IsValid() || !Interface->GetResolvedConnectString(Session,Address) || !Controller)
 { LanStatus=bOnlineSession ? TEXT("Could not join that match.") : TEXT("Could not join LAN match."); return; }
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 Replay->SaveSettings();
 GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->bHomeVisited=true;
 LanStatus=TEXT("Connecting to ")+Address;
 Controller->ClientTravel(Address,TRAVEL_Absolute);
}
void UCrosshairMatchSession::LeaveMatch()
{
 if (const IOnlineSessionPtr Interface=SessionInterface(); Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession))
  Interface->DestroySession(NAME_GameSession);
 bOnlineSession=false; OnlineCode.Reset();
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 // Arrive on Home, as when the game first opens.
 Library->bHomeVisited=false; Library->ActiveMapId.Reset();
 GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/L_Practice"));
}
bool UCrosshairMatchSession::ReturnToLobby()
{
 UWorld* World=GetWorld();
 FString Reason;
 if (!World || World->GetNetMode()!=NM_ListenServer || !HasLanSession() || !CanTravel(Reason)) return false;
 auto& Settings=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 UCrosshairBotMatch::Validate(Settings.Lan);
 if (Settings.Lan.Match.Map!=HostedChoice) { HostedChoice=Settings.Lan.Match.Map; HostedMap=UCrosshairBotMatch::ResolveMap(HostedChoice); }
 UpdateAdvertisement();
 LastMatchMap=HostedMap;
 UE_LOG(LogTemp,Display,TEXT("Crosshair LAN: returning everyone to the lobby on %s"),*UCrosshairBotMatch::MapName(HostedMap));
 return GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->ChangePracticeMap(UCrosshairBotMatch::MapPackage(HostedMap),TravelOptions(Settings.Lan.Match,true,bOnlineSession),true);
}
void UCrosshairMatchSession::ApplyLobbySettings()
{
 auto* Mode=GetWorld() ? GetWorld()->GetAuthGameMode<ACrosshairMatchGameMode>() : nullptr;
 if (!Mode || !Mode->IsLanMatch() || !Mode->IsInLobby()) return;
 auto& Settings=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 UCrosshairBotMatch::Validate(Settings.Lan);
 // A different map needs the map loaded; everything else applies to this lobby as it is.
 if (Settings.Lan.Match.Map!=HostedChoice && ReturnToLobby()) return;
 Mode->RefreshLobbySetup();
 UpdateAdvertisement();
}
void UCrosshairMatchSession::UpdateAdvertisement()
{
 const IOnlineSessionPtr Interface=SessionInterface();
 FOnlineSessionSettings* Settings=Interface.IsValid() ? Interface->GetSessionSettings(NAME_GameSession) : nullptr;
 if (!Settings) return;
 const auto& Saved=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 Settings->NumPublicConnections=Saved.Lan.MaxPlayers;
 Settings->Set(CrosshairBotsKey,FString(Saved.Lan.Match.bBots ? TEXT("bots on") : TEXT("no bots")),EOnlineDataAdvertisementType::ViaOnlineService);
 Settings->Set(SETTING_MAPNAME,UCrosshairBotMatch::MapName(HostedMap),EOnlineDataAdvertisementType::ViaOnlineService);
 if (bOnlineSession) Settings->Set(CrosshairCodeKey,OnlineCode,EOnlineDataAdvertisementType::ViaOnlineService);
 Settings->Set(CrosshairModeKey,UCrosshairBotMatch::ModeName(Saved.Lan.Match.Mode),EOnlineDataAdvertisementType::ViaOnlineService);
 Interface->UpdateSession(NAME_GameSession,*Settings,true);
}
void UCrosshairMatchSession::ClearHandles()
{
 if (const IOnlineSessionPtr Interface=SessionInterface(); Interface.IsValid())
 {
  Interface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
  Interface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
  Interface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
 }
}
