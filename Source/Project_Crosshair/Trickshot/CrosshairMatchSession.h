#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "CrosshairBotMatch.h"
#include "CrosshairMatchSession.generated.h"

class FOnlineSessionSearch;

/**
 * Starts offline matches and owns the same-network (LAN) session foundation.
 * LAN uses the engine's Null online subsystem: broadcast discovery on the local
 * network only, a listen-server host, and no external or paid online service.
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
 static FString TravelOptions(const FCrosshairBotMatchOptions& Options,bool bListen);
 bool HasLanSession() const;
 TArray<FString> LanResults;
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
 TSharedPtr<FOnlineSessionSearch> Search;
 /** LAN host: the map rolled when the session was advertised. */
 ECrosshairMatchMap HostedMap=ECrosshairMatchMap::Nuketown;
 /** The saved map choice HostedMap was rolled from (Random stays the same map until the choice changes). */
 ECrosshairMatchMap HostedChoice=ECrosshairMatchMap::Nuketown;
 /** Refreshes the LAN listing (map, mode, bots, player limit) after the host changes the setup. */
 void UpdateAdvertisement();
 FDelegateHandle CreateHandle,FindHandle,JoinHandle;
};
