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
#include "Online/OnlineServices.h"
#include "Online/Auth.h"
#include "Online/OnlineServicesEOSGS.h"
#include "Online/OnlineIdEOSGS.h"
#include "IEOSSDKManager.h"
#include "eos_sdk.h"
#include "eos_connect.h"
#include "eos_sessions.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

namespace
{
 const FName CrosshairModeKey(TEXT("CROSSHAIR_MODE"));
 const FName CrosshairHostKey(TEXT("CROSSHAIR_HOST"));
 const FName CrosshairBotsKey(TEXT("CROSSHAIR_BOTS"));
 // Epic session attributes and bucket for Online code listings.
 const char* const CrosshairEpicCodeAttr="CROSSHAIRCODE";
 const char* const CrosshairEpicHostAttr="CROSSHAIRHOST";
 const char* const CrosshairEpicModeAttr="CROSSHAIRMODE";
 const char* const CrosshairEpicMapAttr="CROSSHAIRMAP";
 const char* const CrosshairEpicBucket="Crosshair:Online";
 const TCHAR* const CrosshairEpicPlatformName=TEXT("Crosshair");
 IOnlineSessionPtr GetMatchSessionInterface(const UObject* Context) { return Online::GetSessionInterface(Context ? Context->GetWorld() : nullptr); }
 /** Epic Game Services (created on first use, after IsOnlineConfigured registered the IDs). */
 TSharedPtr<UE::Online::FOnlineServicesEOSGS> CrosshairEpicServices()
 {
  if (!UCrosshairMatchSession::IsOnlineConfigured()) return nullptr;
  const TSharedPtr<UE::Online::IOnlineServices> Services=UE::Online::GetServices(UE::Online::EOnlineServices::Epic);
  return Services && Services->GetServicesProvider()==UE::Online::EOnlineServices::Epic ? StaticCastSharedPtr<UE::Online::FOnlineServicesEOSGS>(Services) : nullptr;
 }
 EOS_HPlatform CrosshairEpicPlatform()
 {
  const TSharedPtr<UE::Online::FOnlineServicesEOSGS> Services=CrosshairEpicServices();
  const IEOSPlatformHandlePtr Handle=Services ? Services->GetEOSPlatformHandle() : nullptr;
  return Handle ? static_cast<EOS_HPlatform>(*Handle) : nullptr;
 }
 /** This PC's signed-in Epic user (the first local player), or null. */
 EOS_ProductUserId CrosshairEpicUser()
 {
  const TSharedPtr<UE::Online::FOnlineServicesEOSGS> Services=CrosshairEpicServices();
  if (!Services) return nullptr;
  UE::Online::TOnlineResult<UE::Online::FAuthGetLocalOnlineUserByPlatformUserId> Result=Services->GetAuthInterface()->GetLocalOnlineUserByPlatformUserId({FPlatformMisc::GetPlatformUserForUserIndex(0)});
  return Result.IsOk() && Result.GetOkValue().AccountInfo->LoginStatus==UE::Online::ELoginStatus::LoggedIn ? UE::Online::GetProductUserId(Result.GetOkValue().AccountInfo->AccountId) : nullptr;
 }
 FString CrosshairEpicUserText(EOS_ProductUserId User)
 {
  char Buffer[EOS_PRODUCTUSERID_MAX_LENGTH+1]={};
  int32_t Length=sizeof(Buffer);
  return User && EOS_ProductUserId_ToString(User,Buffer,&Length)==EOS_EResult::EOS_Success ? FString(UTF8_TO_TCHAR(Buffer)) : FString();
 }
 /** Adds a text attribute that searches can match. */
 bool AddEpicAttribute(EOS_HSessionModification Modification,const char* Key,const FString& Value)
 {
  const FTCHARToUTF8 Text(*Value);
  EOS_Sessions_AttributeData Data={};
  Data.ApiVersion=EOS_SESSIONS_ATTRIBUTEDATA_API_LATEST; Data.Key=Key; Data.Value.AsUtf8=Text.Get(); Data.ValueType=EOS_EAttributeType::EOS_AT_STRING;
  EOS_SessionModification_AddAttributeOptions Options={};
  Options.ApiVersion=EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST; Options.SessionAttribute=&Data; Options.AdvertisementType=EOS_ESessionAttributeAdvertisementType::EOS_SAAT_Advertise;
  return EOS_SessionModification_AddAttribute(Modification,&Options)==EOS_EResult::EOS_Success;
 }
}
bool UCrosshairMatchSession::IsOnlineConfigured()
{
 // Read once, before Epic services first start: the secret stays out of the project's checked-in config.
 static bool bRead=false,bConfigured=false;
 if (bRead) return bConfigured;
 bRead=true;
 FConfigFile File;
 File.Read(FPaths::ProjectContentDir()/TEXT("Online/CrosshairOnline.local.ini"));
 auto Value=[&](const TCHAR* Key){ FString Text; File.GetString(TEXT("Epic"),Key,Text); return Text.TrimStartAndEnd(); };
 FEOSSDKPlatformConfig Platform;
 Platform.Name=CrosshairEpicPlatformName;
 Platform.ProductId=Value(TEXT("ProductId")); Platform.SandboxId=Value(TEXT("SandboxId")); Platform.DeploymentId=Value(TEXT("DeploymentId"));
 Platform.ClientId=Value(TEXT("ClientId")); Platform.ClientSecret=Value(TEXT("ClientSecret")); Platform.EncryptionKey=Value(TEXT("EncryptionKey"));
 if (Platform.ProductId.IsEmpty() || Platform.SandboxId.IsEmpty() || Platform.DeploymentId.IsEmpty() || Platform.ClientId.IsEmpty() || Platform.ClientSecret.IsEmpty() || Platform.EncryptionKey.Len()!=64) return false;
 IEOSSDKManager* SDK=IEOSSDKManager::Get();
 // EOSShared starts the SDK when the game launches.
 if (!SDK || !SDK->IsInitialized()) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: the Epic SDK did not start.")); return false; }
 // Game Services only: no Epic account, overlay or voice.
 Platform.CacheDirectory=SDK->GetCacheDirBase().IsEmpty() ? FString() : SDK->GetCacheDirBase()/TEXT("Crosshair");
 Platform.bDisableOverlay=true; Platform.bDisableSocialOverlay=true; Platform.bEnableRTC=false;
 Platform.bLoadingInEditor=!IsRunningGame();
 if (!SDK->AddPlatformConfig(Platform,true)) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: the Epic IDs were rejected.")); return false; }
 SDK->SetDefaultPlatformConfigName(Platform.Name);
 bConfigured=true;
 return true;
}
IOnlineSessionPtr UCrosshairMatchSession::SessionInterface() const { return GetMatchSessionInterface(this); }
FString UCrosshairMatchSession::OnlineUserId() const { return CrosshairEpicUserText(CrosshairEpicUser()); }
void UCrosshairMatchSession::SignInOnline(TFunction<void()> Then)
{
 using namespace UE::Online;
 const TSharedPtr<FOnlineServicesEOSGS> Services=CrosshairEpicServices();
 const EOS_HPlatform Platform=CrosshairEpicPlatform();
 if (!Services || !Platform) { LanStatus=TEXT("Online is unavailable: Epic services did not start."); return; }
 if (CrosshairEpicUser()) { Then(); return; }
 if (bSigningIn) return;
 bSigningIn=true; LanStatus=TEXT("Signing in...");
 struct FDeviceContext { TWeakObjectPtr<UCrosshairMatchSession> Self; TFunction<void()> Then; };
 auto Login=[](FDeviceContext* Raw,const EOS_EResult Created)
 {
  TUniquePtr<FDeviceContext> Context(Raw);
  UCrosshairMatchSession* Self=Context->Self.Get();
  if (!Self) return;
  // A PC that signed in before already has its device ID.
  if (Created!=EOS_EResult::EOS_Success && Created!=EOS_EResult::EOS_DuplicateNotAllowed)
  { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: device ID failed (%hs)"),EOS_EResult_ToString(Created)); Self->bSigningIn=false; Self->LanStatus=TEXT("Could not sign in to Epic services."); return; }
  const TSharedPtr<FOnlineServicesEOSGS> Epic=CrosshairEpicServices();
  if (!Epic) { Self->bSigningIn=false; Self->LanStatus=TEXT("Online is unavailable."); return; }
  FAuthLogin::Params Params;
  Params.PlatformUserId=FPlatformMisc::GetPlatformUserForUserIndex(0);
  Params.CredentialsType=LoginCredentialsType::ExternalAuth;
  UE::Online::FExternalAuthToken DeviceToken;
  DeviceToken.Type=ExternalLoginType::DeviceIdAccessToken;
  Params.CredentialsToken.Set<UE::Online::FExternalAuthToken>(MoveTemp(DeviceToken));
  Epic->GetAuthInterface()->Login(MoveTemp(Params)).OnComplete([Weak=Context->Self,Next=MoveTemp(Context->Then)](const TOnlineResult<FAuthLogin>& Result)
  {
   UCrosshairMatchSession* Session=Weak.Get();
   if (!Session) return;
   Session->bSigningIn=false;
   if (Result.IsError()) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online sign-in failed: %s"),*Result.GetErrorValue().GetLogString()); Session->LanStatus=TEXT("Could not sign in to Epic services."); return; }
   UE_LOG(LogTemp,Display,TEXT("Crosshair online: signed in to Epic as %s"),*Session->OnlineUserId());
   Next();
  });
 };
 const EOS_HConnect Connect=EOS_Platform_GetConnectInterface(Platform);
 auto CreateDevice=[Connect,Login](FDeviceContext* Context)
 {
  EOS_Connect_CreateDeviceIdOptions Options={};
  Options.ApiVersion=EOS_CONNECT_CREATEDEVICEID_API_LATEST;
  Options.DeviceModel="PC";
  // The SDK copies nothing back but the result; the context rides along as ClientData.
  struct FCreateData { FDeviceContext* Context; decltype(Login) Run; };
  EOS_Connect_CreateDeviceId(Connect,&Options,new FCreateData{Context,Login},[](const EOS_Connect_CreateDeviceIdCallbackInfo* Info)
  {
   TUniquePtr<FCreateData> Data(static_cast<FCreateData*>(Info->ClientData));
   Data->Run(Data->Context,Info->ResultCode);
  });
 };
 FDeviceContext* Context=new FDeviceContext{this,MoveTemp(Then)};
 // Test runs only: a second game on the same Windows account would share this PC's device ID (and so its Epic user)
 // with the first; a fresh device ID makes it a different player. Real players are on their own PCs.
 static bool bFreshDone=false;
 if (!bFreshDone && FParse::Param(FCommandLine::Get(),TEXT("CrosshairOnlineFreshDevice")))
 {
  bFreshDone=true;
  EOS_Connect_DeleteDeviceIdOptions Options={};
  Options.ApiVersion=EOS_CONNECT_DELETEDEVICEID_API_LATEST;
  struct FDeleteData { FDeviceContext* Context; decltype(CreateDevice) Create; };
  EOS_Connect_DeleteDeviceId(Connect,&Options,new FDeleteData{Context,CreateDevice},[](const EOS_Connect_DeleteDeviceIdCallbackInfo* Info)
  {
   TUniquePtr<FDeleteData> Data(static_cast<FDeleteData*>(Info->ClientData));
   UE_LOG(LogTemp,Display,TEXT("Crosshair online: fresh device ID requested (delete: %hs)"),EOS_EResult_ToString(Info->ResultCode));
   Data->Create(Data->Context);
  });
  return;
 }
 CreateDevice(Context);
}
bool UCrosshairMatchSession::HostOnlineMatch()
{
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 if (!IsOnlineConfigured()) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: fill in Content/Online/CrosshairOnline.local.ini")); LanStatus=TEXT("Online isn't available in this build."); return false; }
 if (bSigningIn || bSearching) return false;
 // Leaving a LAN session behind: Online and LAN never run together.
 if (const IOnlineSessionPtr Lan=SessionInterface(); Lan.IsValid() && Lan->GetNamedSession(NAME_GameSession)) Lan->DestroySession(NAME_GameSession);
 bOnlineSession=true; bOnlineJoined=false;
 SignInOnline([this](){ CreateOnlineSession(); });
 return true;
}
void UCrosshairMatchSession::CreateOnlineSession()
{
 const EOS_HPlatform Platform=CrosshairEpicPlatform();
 const EOS_ProductUserId Me=CrosshairEpicUser();
 if (!Platform || !Me) { LanStatus=TEXT("Online is unavailable."); return; }
 DestroyOnlineSession();
 auto& Saved=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 UCrosshairBotMatch::Validate(Saved.Lan);
 const auto& Options=Saved.Lan.Match;
 OnlineCode=CrosshairOnlineCode::Generate();
 HostedChoice=Options.Map; HostedMap=UCrosshairBotMatch::ResolveMap(Options.Map);
 // Each listing gets its own local name, so a new one never waits for an old one to be removed.
 OnlineSessionName=TEXT("Crosshair_")+OnlineCode;
 const FTCHARToUTF8 Name(*OnlineSessionName);
 const EOS_HSessions Sessions=EOS_Platform_GetSessionsInterface(Platform);
 EOS_Sessions_CreateSessionModificationOptions Create={};
 Create.ApiVersion=EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
 Create.SessionName=Name.Get(); Create.BucketId=CrosshairEpicBucket; Create.MaxPlayers=uint32(FMath::Max(2,Saved.Lan.MaxPlayers));
 Create.LocalUserId=Me; Create.bPresenceEnabled=EOS_FALSE; Create.bSanctionsEnabled=EOS_FALSE;
 EOS_HSessionModification Modification=nullptr;
 bool bReady=EOS_Sessions_CreateSessionModification(Sessions,&Create,&Modification)==EOS_EResult::EOS_Success;
 if (bReady)
 {
  EOS_SessionModification_SetPermissionLevelOptions Permission={};
  Permission.ApiVersion=EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST; Permission.PermissionLevel=EOS_EOnlineSessionPermissionLevel::EOS_OSPF_PublicAdvertised;
  EOS_SessionModification_SetJoinInProgressAllowedOptions Progress={};
  Progress.ApiVersion=EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST; Progress.bAllowJoinInProgress=EOS_TRUE;
  bReady=EOS_SessionModification_SetPermissionLevel(Modification,&Permission)==EOS_EResult::EOS_Success
   && EOS_SessionModification_SetJoinInProgressAllowed(Modification,&Progress)==EOS_EResult::EOS_Success
   && AddEpicAttribute(Modification,CrosshairEpicCodeAttr,OnlineCode)
   && AddEpicAttribute(Modification,CrosshairEpicHostAttr,CrosshairEpicUserText(Me))
   && AddEpicAttribute(Modification,CrosshairEpicModeAttr,UCrosshairBotMatch::ModeName(Options.Mode))
   && AddEpicAttribute(Modification,CrosshairEpicMapAttr,UCrosshairBotMatch::MapName(HostedMap));
 }
 if (!bReady)
 {
  if (Modification) EOS_SessionModification_Release(Modification);
  OnlineCode.Reset(); OnlineSessionName.Reset(); LanStatus=TEXT("Could not create the online match."); return;
 }
 LanStatus=TEXT("Creating online match...");
 EOS_Sessions_UpdateSessionOptions Update={};
 Update.ApiVersion=EOS_SESSIONS_UPDATESESSION_API_LATEST; Update.SessionModificationHandle=Modification;
 EOS_Sessions_UpdateSession(Sessions,&Update,new TWeakObjectPtr<UCrosshairMatchSession>(this),[](const EOS_Sessions_UpdateSessionCallbackInfo* Info)
 {
  TUniquePtr<TWeakObjectPtr<UCrosshairMatchSession>> Weak(static_cast<TWeakObjectPtr<UCrosshairMatchSession>*>(Info->ClientData));
  UCrosshairMatchSession* Self=Weak->Get();
  if (!Self) return;
  if (Info->ResultCode!=EOS_EResult::EOS_Success)
  {
   UE_LOG(LogTemp,Warning,TEXT("Crosshair online: creating the match listing failed (%hs)"),EOS_EResult_ToString(Info->ResultCode));
   Self->OnlineCode.Reset(); Self->OnlineSessionName.Reset(); Self->LanStatus=TEXT("Could not create the online match."); return;
  }
  Self->bOnlineHosting=true;
  Self->LanStatus=TEXT("Hosting online match");
  UE_LOG(LogTemp,Display,TEXT("Crosshair online: hosting with code %s"),*Self->OnlineCode);
  if (!Self->TravelToMatch(true)) Self->DestroyOnlineSession();
 });
 EOS_SessionModification_Release(Modification);
}
void UCrosshairMatchSession::DestroyOnlineSession()
{
 if (OnlineSessionName.IsEmpty()) { bOnlineHosting=false; return; }
 const EOS_HPlatform Platform=CrosshairEpicPlatform();
 if (Platform)
 {
  const FTCHARToUTF8 Name(*OnlineSessionName);
  EOS_Sessions_DestroySessionOptions Options={};
  Options.ApiVersion=EOS_SESSIONS_DESTROYSESSION_API_LATEST; Options.SessionName=Name.Get();
  EOS_Sessions_DestroySession(EOS_Platform_GetSessionsInterface(Platform),&Options,nullptr,[](const EOS_Sessions_DestroySessionCallbackInfo* Info)
  { UE_LOG(LogTemp,Display,TEXT("Crosshair online: match listing removed (%hs)"),EOS_EResult_ToString(Info->ResultCode)); });
 }
 OnlineSessionName.Reset(); bOnlineHosting=false;
}
bool UCrosshairMatchSession::JoinOnlineMatch(const FString& Typed)
{
 const FString Code=CrosshairOnlineCode::Normalize(Typed);
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 if (!CrosshairOnlineCode::IsValid(Code)) { LanStatus=FString::Printf(TEXT("Enter the %d-character code from the host."),CrosshairOnlineCode::Length); return false; }
 if (!IsOnlineConfigured()) { UE_LOG(LogTemp,Warning,TEXT("Crosshair online: fill in Content/Online/CrosshairOnline.local.ini")); LanStatus=TEXT("Online isn't available in this build."); return false; }
 if (bSearching || bSigningIn) return false;
 if (const IOnlineSessionPtr Lan=SessionInterface(); Lan.IsValid() && Lan->GetNamedSession(NAME_GameSession)) Lan->DestroySession(NAME_GameSession);
 DestroyOnlineSession();
 bOnlineSession=true; bOnlineJoined=false; OnlineCode.Reset();
 SignInOnline([this,Code](){ FindOnlineHost(Code); });
 return true;
}
void UCrosshairMatchSession::FindOnlineHost(const FString& Code,int32 Attempt)
{
 const EOS_HPlatform Platform=CrosshairEpicPlatform();
 const EOS_ProductUserId Me=CrosshairEpicUser();
 if (!Platform || !Me) { LanStatus=TEXT("Online is unavailable."); return; }
 const EOS_HSessions Sessions=EOS_Platform_GetSessionsInterface(Platform);
 EOS_Sessions_CreateSessionSearchOptions Create={};
 Create.ApiVersion=EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST; Create.MaxSearchResults=1;
 EOS_HSessionSearch SearchHandle=nullptr;
 if (EOS_Sessions_CreateSessionSearch(Sessions,&Create,&SearchHandle)!=EOS_EResult::EOS_Success) { LanStatus=TEXT("Could not search for that code."); return; }
 const FTCHARToUTF8 CodeText(*Code);
 EOS_Sessions_AttributeData Match={};
 Match.ApiVersion=EOS_SESSIONS_ATTRIBUTEDATA_API_LATEST; Match.Key=CrosshairEpicCodeAttr; Match.Value.AsUtf8=CodeText.Get(); Match.ValueType=EOS_EAttributeType::EOS_AT_STRING;
 EOS_SessionSearch_SetParameterOptions Parameter={};
 Parameter.ApiVersion=EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST; Parameter.Parameter=&Match; Parameter.ComparisonOp=EOS_EOnlineComparisonOp::EOS_CO_EQUAL;
 // Only Crosshair's own listings (the bucket every Online host uses).
 EOS_Sessions_AttributeData Bucket={};
 Bucket.ApiVersion=EOS_SESSIONS_ATTRIBUTEDATA_API_LATEST; Bucket.Key=EOS_SESSIONS_SEARCH_BUCKET_ID; Bucket.Value.AsUtf8=CrosshairEpicBucket; Bucket.ValueType=EOS_EAttributeType::EOS_AT_STRING;
 EOS_SessionSearch_SetParameterOptions BucketParameter={};
 BucketParameter.ApiVersion=EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST; BucketParameter.Parameter=&Bucket; BucketParameter.ComparisonOp=EOS_EOnlineComparisonOp::EOS_CO_EQUAL;
 if (EOS_SessionSearch_SetParameter(SearchHandle,&BucketParameter)!=EOS_EResult::EOS_Success || EOS_SessionSearch_SetParameter(SearchHandle,&Parameter)!=EOS_EResult::EOS_Success)
 { EOS_SessionSearch_Release(SearchHandle); LanStatus=TEXT("Could not search for that code."); return; }
 bSearching=true; LanStatus=FString::Printf(TEXT("Looking for %s..."),*Code);
 struct FFindContext { TWeakObjectPtr<UCrosshairMatchSession> Self; EOS_HSessionSearch Handle; FString MyId; FString Code; int32 Attempt; };
 EOS_SessionSearch_FindOptions Find={};
 Find.ApiVersion=EOS_SESSIONSEARCH_FIND_API_LATEST; Find.LocalUserId=Me;
 EOS_SessionSearch_Find(SearchHandle,&Find,new FFindContext{this,SearchHandle,CrosshairEpicUserText(Me),Code,Attempt},[](const EOS_SessionSearch_FindCallbackInfo* Info)
 {
  TUniquePtr<FFindContext> Context(static_cast<FFindContext*>(Info->ClientData));
  ON_SCOPE_EXIT { EOS_SessionSearch_Release(Context->Handle); };
  UCrosshairMatchSession* Self=Context->Self.Get();
  if (!Self) return;
  Self->bSearching=false;
  EOS_SessionSearch_GetSearchResultCountOptions Count={};
  Count.ApiVersion=EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
  const bool bFound=Info->ResultCode==EOS_EResult::EOS_Success && EOS_SessionSearch_GetSearchResultCount(Context->Handle,&Count)>0;
  if (!bFound)
  {
   UE_LOG(LogTemp,Display,TEXT("Crosshair online: code search %d returned %hs"),Context->Attempt+1,EOS_EResult_ToString(Info->ResultCode));
   if ((Info->ResultCode==EOS_EResult::EOS_Success || Info->ResultCode==EOS_EResult::EOS_NotFound) && Context->Attempt+1<OnlineSearchAttempts)
   {
    // Still searching: try again shortly.
    Self->bSearching=true;
    FTimerHandle Retry;
    Self->GetGameInstance()->GetTimerManager().SetTimer(Retry,FTimerDelegate::CreateWeakLambda(Self,[Self,Code=Context->Code,Attempt=Context->Attempt]()
    { Self->bSearching=false; Self->FindOnlineHost(Code,Attempt+1); }),2.5f,false);
    return;
   }
   Self->LanStatus=Info->ResultCode==EOS_EResult::EOS_Success || Info->ResultCode==EOS_EResult::EOS_NotFound ? TEXT("No match found with that code.") : TEXT("Could not search for that code.");
   return;
  }
  FString Host;
  EOS_SessionSearch_CopySearchResultByIndexOptions Copy={};
  Copy.ApiVersion=EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST; Copy.SessionIndex=0;
  EOS_HSessionDetails Details=nullptr;
  if (EOS_SessionSearch_CopySearchResultByIndex(Context->Handle,&Copy,&Details)==EOS_EResult::EOS_Success)
  {
   EOS_SessionDetails_CopySessionAttributeByKeyOptions Key={};
   Key.ApiVersion=EOS_SESSIONDETAILS_COPYSESSIONATTRIBUTEBYKEY_API_LATEST; Key.AttrKey=CrosshairEpicHostAttr;
   EOS_SessionDetails_Attribute* Attribute=nullptr;
   if (EOS_SessionDetails_CopySessionAttributeByKey(Details,&Key,&Attribute)==EOS_EResult::EOS_Success && Attribute)
   {
    if (Attribute->Data && Attribute->Data->ValueType==EOS_EAttributeType::EOS_AT_STRING && Attribute->Data->Value.AsUtf8) Host=UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8);
    EOS_SessionDetails_Attribute_Release(Attribute);
   }
   EOS_SessionDetails_Release(Details);
  }
  if (Host.IsEmpty() || EOS_ProductUserId_IsValid(EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Host)))==EOS_FALSE) { Self->LanStatus=TEXT("Could not join that match."); return; }
  if (Host==Context->MyId) { Self->LanStatus=TEXT("That code is your own match."); return; }
  APlayerController* Controller=Self->GetGameInstance()->GetFirstLocalPlayerController();
  if (!Controller) { Self->LanStatus=TEXT("Could not join that match."); return; }
  Self->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->SaveSettings();
  Self->GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->bHomeVisited=true;
  Self->bOnlineJoined=true;
  Self->LanStatus=TEXT("Connecting...");
  // Epic's peer-to-peer address for the host, as the engine writes it: [EOS:<host's Epic user ID>].
  const FString Address=FString::Printf(TEXT("[EOS:%s]"),*Host);
  UE_LOG(LogTemp,Display,TEXT("Crosshair online: found the host, travelling to %s"),*Address);
  Controller->ClientTravel(Address,TRAVEL_Absolute);
 });
}

void UCrosshairMatchSession::Deinitialize()
{
 ClearHandles();
 DestroyOnlineSession();
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
 if (bOnlineSession) return bOnlineHosting || bOnlineJoined;
 const IOnlineSessionPtr Interface=SessionInterface();
 return Interface.IsValid() && Interface->GetNamedSession(NAME_GameSession)!=nullptr;
}
bool UCrosshairMatchSession::HostLanMatch()
{
 FString Reason;
 if (!CanTravel(Reason)) { LanStatus=Reason; return false; }
 DestroyOnlineSession();
 bOnlineSession=bOnlineJoined=false; OnlineCode.Reset();
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
 if (!bSuccess) { LanStatus=TEXT("Could not create LAN match."); return; }
 LanStatus=TEXT("Hosting LAN match");
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
 { LanStatus=TEXT("Could not join LAN match."); return; }
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
 DestroyOnlineSession();
 bOnlineSession=bOnlineJoined=false; OnlineCode.Reset();
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
 // An Online listing only carries the code and the host, which never change while it is up.
 if (bOnlineSession) return;
 const IOnlineSessionPtr Interface=SessionInterface();
 FOnlineSessionSettings* Settings=Interface.IsValid() ? Interface->GetSessionSettings(NAME_GameSession) : nullptr;
 if (!Settings) return;
 const auto& Saved=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 Settings->NumPublicConnections=Saved.Lan.MaxPlayers;
 Settings->Set(CrosshairBotsKey,FString(Saved.Lan.Match.bBots ? TEXT("bots on") : TEXT("no bots")),EOnlineDataAdvertisementType::ViaOnlineService);
 Settings->Set(SETTING_MAPNAME,UCrosshairBotMatch::MapName(HostedMap),EOnlineDataAdvertisementType::ViaOnlineService);
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
