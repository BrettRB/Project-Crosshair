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
 static FString TravelOptions(const FCrosshairBotMatchOptions& Options,bool bListen);
 bool HasLanSession() const;
 TArray<FString> LanResults;
 FString LanStatus;
 bool bSearching=false;
 static constexpr int32 MaxLanResults=4;
private:
 bool CanTravel(FString& Reason) const;
 bool TravelToMatch(bool bListen);
 void OnCreateComplete(FName Session,bool bSuccess);
 void OnFindComplete(bool bSuccess);
 void OnJoinComplete(FName Session,EOnJoinSessionCompleteResult::Type Result);
 void ClearHandles();
 TSharedPtr<FOnlineSessionSearch> Search;
 FDelegateHandle CreateHandle,FindHandle,JoinHandle;
};
