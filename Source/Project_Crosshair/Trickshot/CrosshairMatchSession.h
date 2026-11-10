#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "CrosshairBotMatch.h"
#include "CrosshairMatchSession.generated.h"

class FOnlineSessionSearch;

/** Friend codes for Online matches: five characters, leaving out ones easily misread (0/O, 1/I). */
namespace CrosshairOnlineCode
{
 constexpr int32 Length=5;
 inline const TCHAR* Alphabet() { return TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"); }
 inline FString Generate()
 {
  FString Code;
  for (int32 i=0;i<Length;++i) Code.AppendChar(Alphabet()[FMath::RandRange(0,31)]);
  return Code;
 }
 /** Uppercases what a player typed and drops anything that cannot be in a code. */
 inline FString Normalize(const FString& Typed)
 {
  FString Code;
  for (const TCHAR Char:Typed.ToUpper()) { int32 Found; if (FString(Alphabet()).FindChar(Char,Found) && Code.Len()<Length) Code.AppendChar(Char); }
  return Code;
 }
 inline bool IsValid(const FString& Code) { return Code.Len()==Length && Normalize(Code)==Code; }
}

/**
 * Starts offline matches and owns the hosted sessions. LAN uses the engine's Null online subsystem: broadcast
 * discovery on the local network only. Online uses Epic Online Services: a host publishes a short code, friends
 * look it up, and they connect peer-to-peer (Epic's relay carries the traffic when a direct path fails).
 */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairMatchSession : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Deinitialize() override;
 /** Travels to the saved COD map with the saved mode. Rejects non-match maps. */
 bool StartOfflineMatch();
 /** Advertises a LAN session, then opens the match map as a listen server. */
 bool HostLanMatch();
 bool FindLanMatches();
 bool JoinLanMatch(int32 Index);
 /** Ends any LAN session and returns to the practice map's Home screen. */
 void LeaveMatch();
 /**
  * LAN host: reloads the match map as a fresh lobby and brings every joined player along (the session stays open).
  * The map is re-rolled only when the host's saved map choice changed since it was last rolled.
  */
 bool ReturnToLobby();
 /** LAN host back on the lobby screen: applies the saved LAN setup (a new map reloads the lobby). */
 void ApplyLobbySettings();
 static FString TravelOptions(const FCrosshairBotMatchOptions& Options,bool bListen,bool bOnline=false);
 /** True when a LAN or Online session is open. */
 bool HasLanSession() const;
 /** True once Content/Online/CrosshairOnline.local.ini holds all the Epic IDs. */
 static bool IsOnlineConfigured();
 /** Signs in with Epic, publishes a new code and opens the match lobby. */
 bool HostOnlineMatch();
 /** Finds the match with this code and joins it. */
 bool JoinOnlineMatch(const FString& Code);
 /** The hosted Online match's friend code; empty otherwise. */
 FString OnlineCode;
 /** What the player has typed on the Online page. */
 FString TypedCode;
 TArray<FString> LanResults;
 /** Progress and errors for the LAN and Online pages. */
 FString LanStatus;
 bool bSearching=false;
 /** The map the last match travelled to (Random already resolved). */
 ECrosshairMatchMap LastMatchMap=ECrosshairMatchMap::Nuketown;
 static constexpr int32 MaxLanResults=4;
private:
 bool CanTravel(FString& Reason) const;
 bool TravelToMatch(bool bListen);
 void OnCreateComplete(FName Session,bool bSuccess);
 void OnFindComplete(bool bSuccess);
 void OnJoinComplete(FName Session,EOnJoinSessionCompleteResult::Type Result);
 void ClearHandles();
 /** The session interface for the current mode: Epic for Online, Null for LAN. */
 IOnlineSessionPtr SessionInterface() const;
 bool bOnlineSession=false;
 /** Runs Then once this player is signed in to Epic, with a device ID: no Epic account needed. */
 void SignInOnline(TFunction<void()> Then);
 void CreateOnlineSession();
 void OnFindOnlineComplete(bool bSuccess);
 FDelegateHandle LoginHandle;
 TSharedPtr<FOnlineSessionSearch> Search;
 /** LAN host: the map rolled when the session was advertised. */
 ECrosshairMatchMap HostedMap=ECrosshairMatchMap::Nuketown;
 /** The saved map choice HostedMap was rolled from (Random stays the same map until the choice changes). */
 ECrosshairMatchMap HostedChoice=ECrosshairMatchMap::Nuketown;
 /** Refreshes the LAN listing (map, mode, bots, player limit) after the host changes the setup. */
 void UpdateAdvertisement();
 FDelegateHandle CreateHandle,FindHandle,JoinHandle;
};
