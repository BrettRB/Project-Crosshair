#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrosshairStairLinks.generated.h"


/**
 * Finds the staircases the navmesh leaves out. Recast leaves flights made of separate step blocks without navmesh, so
 * bots could never leave the ground floor. Once a match's navmesh is built, this walks the real collision with the
 * character's step limit and records each off-navmesh route joining two levels; bots then run those flights directly.
 * It only reads collision and navmesh, so it needs no per-map setup (a map whose bots spawn in a pit works too).
 */
UCLASS(NotPlaceable,Transient)
class PROJECT_CROSSHAIR_API ACrosshairStairLinks : public AActor
{
 GENERATED_BODY()
public:
 struct FLink { FVector Lower=FVector::ZeroVector,Upper=FVector::ZeroVector; TArray<FVector> Route; };
 ACrosshairStairLinks();
 /** Scans the map's NavMeshBoundsVolumes. Host only; call once the navmesh has finished building. */
 void Build();
 const TArray<FLink>& GetLinks() const { return Links; }
 /** Waypoints along a staircase whose near end the bot can walk to. Goes either way, so bots also come back down. */
 bool PickTrip(const FVector& From,TArray<FVector>& OutRoute) const;
 /** A reachable floor above the main level, for sending bots upstairs. False when the map has none in reach of From. */
 bool PickUpperGoal(const FVector& From,FVector& OutGoal) const;
private:
 TArray<FLink> Links;
 TArray<FVector> UpperFloors;
};
