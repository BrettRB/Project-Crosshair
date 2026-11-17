#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "CrosshairBotMatch.h"
#include "CrosshairMatchSession.generated.h"

class FOnlineSessionSearch;

/**
 * Friend codes for Online matches: six characters, leaving out ones easily misread (0/O, 1/I).
 * The code is the key to the lobby: the host checks it on every connection (FCrosshairJoinGate), and Epic's
 * public listing carries only a slow salted hash of it (ListingKey), never the code itself.
 */
namespace CrosshairOnlineCode
{
 constexpr int32 Length=6;
 inline const TCHAR* Alphabet() { return TEXT("ABCDEFGHJKLMNPQRSTUVWXYZ23456789"); }
 /**
  * Uses the OS's cryptographic random source, not the game's RNG: a new GUID is a random (version 4) UUID on
  * Windows (CoCreateGuid) and Mac (uuid_generate), and its first 32 bits are all random. 6 x 5 bits = 30 of them.
  */
 inline FString Generate()
 {
  static_assert(Length*5<=32,"One GUID word holds the whole code");
  const uint32 Random=FGuid::NewGuid().A;
  FString Code;
  for (int32 i=0;i<Length;++i) Code.AppendChar(Alphabet()[(Random>>(i*5))&31]);
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
 /** Travel option a joining player sends the code in; the host's PreLogin reads it. */
 inline const TCHAR* TravelOption() { return TEXT("CrosshairCode"); }
 /** Hash rounds for ListingKey: a few milliseconds per lookup, but hours of computing to try every code against a listing. */
 constexpr int32 ListingHashRounds=20000;
 /** What the Epic listing advertises and is searched by instead of the code: an iterated, salted SHA-1 (hex). */
 PROJECT_CROSSHAIR_API FString ListingKey(const FString& Code);
 /** Compares the whole code without stopping at the first difference. */
 inline bool Matches(const FString& Expected,const FString& Offered)
 {
  if (Expected.IsEmpty() || Expected.Len()!=Offered.Len()) return false;
  uint32 Difference=0;
  for (int32 i=0;i<Expected.Len();++i) Difference|=uint32(Expected[i])^uint32(Offered[i]);
  return Difference==0;
 }
}

/**
 * The host's door: an Online player must send the lobby's code. Wrong codes count against the player's network
 * address, which is locked out after MaxFailures. Players the host removes stay out for the rest of the session.
 * LAN lobbies have no code (Code empty) and only use the removed list.
 */
struct FCrosshairJoinGate
{
 static constexpr int32 MaxFailures=5;
 /** The hosted Online code; empty for LAN. */
 FString Code;
 TMap<FString,int32> Failures;
 TSet<FString> Removed;
 /** Empty when the player may join; otherwise the reason sent back to them. */
 FString Check(const FString& Address,const FString& OfferedCode)
 {
  if (Removed.Contains(Address)) return TEXT("The host removed you from this match.");
  if (Code.IsEmpty()) return FString();
  int32& Failed=Failures.FindOrAdd(Address);
  if (Failed>=MaxFailures) return TEXT("Too many wrong codes for this match.");
  if (CrosshairOnlineCode::Matches(Code,OfferedCode)) return FString();
  ++Failed;
  return TEXT("Wrong match code.");
 }
 void Reset(const FString& NewCode=FString()) { Code=NewCode; Failures.Reset(); Removed.Reset(); }
};

/**
 * Starts offline matches and owns the hosted sessions. LAN uses the engine's Null online subsystem: broadcast
 * discovery on the local network only. Online uses Epic Online Services (Game Services only, through the engine's
 * OnlineServicesEOSGS plugin): each PC signs in anonymously with a device ID, a host publishes an Epic session
 * carrying a short code and its Epic user ID, friends look the code up, and they connect peer-to-peer
 * (Epic's relay carries the traffic when a direct path fails).
 */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairMatchSession : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 /** Why the host turned this player away or removed them (shown once back in practice); empty otherwise. */
 FString DisconnectReason;
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
 /** Host: who may connect (the Online code, wrong-code lockouts, removed players). Kept across lobby reloads. */
 FCrosshairJoinGate JoinGate;
 /** Host: removes a joined player; they cannot rejoin this session. */
 bool RemovePlayer(APlayerController* Player);
 /** Online smoke test only: sent to the host instead of the looked-up code, to check that the host refuses it. */
 FString TestSentCode;
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
 /** A joined or joining player lost the host: keeps the host's reason (wrong code, removed) for the player. */
 void OnNetworkFailure(UWorld* World,class UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error);
 FDelegateHandle FailureHandle;
 /** The LAN (Null) session interface. Online sessions go straight to Epic's SDK. */
 IOnlineSessionPtr SessionInterface() const;
 bool bOnlineSession=false;
 /** Online host: this machine's Epic session (the code listing) is up. */
 bool bOnlineHosting=false;
 /** Online player: connected or connecting to a host found by code. */
 bool bOnlineJoined=false;
 bool bSigningIn=false;
 /** Online host: the local name of this machine's Epic session. */
 FString OnlineSessionName;
 /** Runs Then once this player is signed in to Epic, with a device ID: no Epic account needed. */
 void SignInOnline(TFunction<void()> Then);
 void CreateOnlineSession();
 /** Looks the code up; a listing made seconds ago can take a moment to be searchable, so misses retry a few times. */
 void FindOnlineHost(const FString& Code,int32 Attempt=0);
 static constexpr int32 OnlineSearchAttempts=4;
 /** Removes this machine's Epic code listing, if it has one. */
 void DestroyOnlineSession();
 /** This player's Epic user ID as text, once signed in. */
 FString OnlineUserId() const;
 TSharedPtr<FOnlineSessionSearch> Search;
 /** LAN host: the map rolled when the session was advertised. */
 ECrosshairMatchMap HostedMap=ECrosshairMatchMap::Nuketown;
 /** The saved map choice HostedMap was rolled from (Random stays the same map until the choice changes). */
 ECrosshairMatchMap HostedChoice=ECrosshairMatchMap::Nuketown;
 /** Refreshes the LAN listing (map, mode, bots, player limit) after the host changes the setup. */
 void UpdateAdvertisement();
 FDelegateHandle CreateHandle,FindHandle,JoinHandle;
};
