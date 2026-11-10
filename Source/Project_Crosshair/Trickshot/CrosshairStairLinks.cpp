#include "CrosshairStairLinks.h"
#include "CrosshairCharacter.h"
#include "EngineUtils.h"
#include "Algo/Reverse.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

namespace
{
 const float StairSampleCellSize=50.f;          // Fine enough for a 40 cm rise between samples on ordinary stairs.
 const float StairStepLimit=45.f;     // UCharacterMovementComponent::MaxStepHeight.
 const float StairHeadroom=130.f;     // A soldier can at least crouch through.
 const float StairMinRise=100.f;      // Smaller changes are kerbs and clutter, not another level.
 const float StairMaxRun=1500.f;      // Longest single flight.
 const int32 StairMinLevelSamples=40; // About 10 square metres of connected navmesh.
 const int32 StairMaxLinks=48;

 struct FStairFloorSample { float Z=0; bool bOnNav=false; int32 Source=INDEX_NONE,Depth=0,Parent=INDEX_NONE; };
 struct FStairLinkCandidate { int32 A=0,B=0,Depth=0,MeetA=0,MeetB=0; };
}

ACrosshairStairLinks::ACrosshairStairLinks()
{
 bReplicates=false;
 SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
void ACrosshairStairLinks::Build()
{
 UWorld* World=GetWorld();
 UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
 const ANavigationData* NavData=Navigation ? Navigation->GetDefaultNavDataInstance() : nullptr;
 FBox Area(ForceInit);
 for (TActorIterator<ANavMeshBoundsVolume> It(World);It;++It) Area+=It->GetComponentsBoundingBox(true);
 if (!NavData || !Area.IsValid) return;
 const double Started=FPlatformTime::Seconds();
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairStairLinks),false);
 for (TActorIterator<ACrosshairCharacter> It(World);It;++It) Params.AddIgnoredActor(*It);
 const int32 NX=FMath::CeilToInt32((Area.Max.X-Area.Min.X)/StairSampleCellSize),NY=FMath::CeilToInt32((Area.Max.Y-Area.Min.Y)/StairSampleCellSize);
 // Every walkable floor in every column, top down. A column's floors are contiguous in Floors.
 TArray<int32> First; First.SetNumUninitialized(NX*NY+1);
 TArray<FStairFloorSample> Floors; Floors.Reserve(NX*NY);
 TArray<FIntPoint> Column;
 Column.Reserve(NX*NY);
 const FVector NavExtent(18,18,35);
 for (int32 X=0;X<NX;++X)
  for (int32 Y=0;Y<NY;++Y)
  {
   First[X*NY+Y]=Floors.Num();
   const double WorldX=Area.Min.X+X*StairSampleCellSize,WorldY=Area.Min.Y+Y*StairSampleCellSize;
   FVector From(WorldX,WorldY,Area.Max.Z); double Ceiling=TNumericLimits<double>::Max();
   for (int32 Layer=0;Layer<6;++Layer)
   {
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit,From,FVector(WorldX,WorldY,Area.Min.Z),ECC_Pawn,Params)) break;
    if (Hit.ImpactNormal.Z>.7f && Ceiling-Hit.ImpactPoint.Z>StairHeadroom)
    {
     FStairFloorSample Floor; Floor.Z=float(Hit.ImpactPoint.Z);
     FNavLocation On;
     Floor.bOnNav=Navigation->ProjectPointToNavigation(Hit.ImpactPoint,On,NavExtent);
     // Off the navmesh there is no guarantee a soldier fits: railings, sills and ledges must not count as a way up.
     if (!Floor.bOnNav && World->OverlapBlockingTestByChannel(Hit.ImpactPoint+FVector(0,0,105),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(25.f,45.f),Params)) { Ceiling=Hit.ImpactPoint.Z; From=Hit.ImpactPoint-FVector(0,0,15); continue; }
     Floors.Add(Floor); Column.Add(FIntPoint(X,Y));
    }
    Ceiling=Hit.ImpactPoint.Z; From=Hit.ImpactPoint-FVector(0,0,15);
   }
  }
 First[NX*NY]=Floors.Num();
 auto ForEachNeighbor=[&](int32 Index,auto&& Visit)
 {
  const FIntPoint At=Column[Index]; const float Z=Floors[Index].Z;
  for (const FIntPoint Step:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
  {
   const int32 X=At.X+Step.X,Y=At.Y+Step.Y;
   if (X<0 || Y<0 || X>=NX || Y>=NY) continue;
   for (int32 Other=First[X*NY+Y];Other<First[X*NY+Y+1];++Other) if (FMath::Abs(Floors[Other].Z-Z)<=StairStepLimit) Visit(Other);
  }
 };
 auto WorldAt=[&](int32 Index){ return FVector(Area.Min.X+Column[Index].X*StairSampleCellSize,Area.Min.Y+Column[Index].Y*StairSampleCellSize,Floors[Index].Z); };
 // Scraps of navmesh (part of a stair, a car roof) are not levels: count them as off-navmesh so routes run through them.
 TBitArray<> Seen(false,Floors.Num());
 TArray<int32> Group;
 for (int32 Index=0;Index<Floors.Num();++Index)
 {
  if (!Floors[Index].bOnNav || Seen[Index]) continue;
  Group.Reset(); Group.Add(Index); Seen[Index]=true;
  for (int32 Head=0;Head<Group.Num();++Head)
   ForEachNeighbor(Group[Head],[&](int32 Other){ if (Floors[Other].bOnNav && !Seen[Other]) { Seen[Other]=true; Group.Add(Other); } });
  if (Group.Num()<StairMinLevelSamples) for (const int32 Member:Group) Floors[Member].bOnNav=false;
 }
 // Flood the off-navmesh floors outward from every navmesh edge. Where fronts from different levels meet, a walkable
 // route joins those levels.
 TArray<int32> Queue;
 for (int32 Index=0;Index<Floors.Num();++Index)
 {
  if (!Floors[Index].bOnNav) continue;
  bool bEdge=false;
  ForEachNeighbor(Index,[&](int32 Other){ bEdge|=!Floors[Other].bOnNav; });
  if (bEdge) { Floors[Index].Source=Index; Queue.Add(Index); }
 }
 TArray<FStairLinkCandidate> Candidates;
 for (int32 Head=0;Head<Queue.Num();++Head)
 {
  const int32 Index=Queue[Head];
  ForEachNeighbor(Index,[&](int32 Other)
  {
   FStairFloorSample& Next=Floors[Other];
   if (Next.bOnNav) return;
   if (Next.Source==INDEX_NONE) { Next.Source=Floors[Index].Source; Next.Depth=Floors[Index].Depth+1; Next.Parent=Index; Queue.Add(Other); return; }
   if (Next.Source!=Floors[Index].Source && FMath::Abs(Floors[Next.Source].Z-Floors[Floors[Index].Source].Z)>=StairMinRise)
    Candidates.Add({Floors[Index].Source,Next.Source,Floors[Index].Depth+Next.Depth+1,Index,Other});
  });
 }
 Candidates.Sort([](const FStairLinkCandidate& L,const FStairLinkCandidate& R){ return L.Depth<R.Depth; });
 for (const FStairLinkCandidate& Candidate:Candidates)
 {
  if (Links.Num()>=StairMaxLinks) break;
  FVector Low=WorldAt(Candidate.A),High=WorldAt(Candidate.B);
  if (Low.Z>High.Z) Swap(Low,High);
  if (FVector::Dist2D(Low,High)>StairMaxRun || Candidate.Depth*StairSampleCellSize>StairMaxRun*1.5f) continue;
  if (Links.ContainsByPredicate([&](const FLink& L){ return FVector::Dist(L.Lower,Low)<300.f && FVector::Dist(L.Upper,High)<300.f; })) continue;
  FNavLocation LowNav,HighNav;
  if (!Navigation->ProjectPointToNavigation(Low,LowNav,NavExtent) || !Navigation->ProjectPointToNavigation(High,HighNav,NavExtent)) continue;
  // Levels the navmesh already joins nearby need no link.
  FPathFindingQuery Query(nullptr,*NavData,LowNav.Location,HighNav.Location);
  Query.SetAllowPartialPaths(false);
  const FPathFindingResult Existing=Navigation->FindPathSync(Query);
  if (Existing.IsSuccessful() && !Existing.IsPartial() && Existing.Path->GetLength()<=FVector::Dist(Low,High)*2.5) continue;
  // The walked route, low end first: back from the meeting point to each source. Every third cell is enough to steer by.
  TArray<FVector> Route;
  for (int32 At=Candidate.MeetA;At!=INDEX_NONE;At=Floors[At].Parent) Route.Insert(WorldAt(At),0);
  for (int32 At=Candidate.MeetB;At!=INDEX_NONE;At=Floors[At].Parent) Route.Add(WorldAt(At));
  if (Route[0].Z>Route.Last().Z) Algo::Reverse(Route);
  FLink Link; Link.Lower=LowNav.Location; Link.Upper=HighNav.Location;
  for (int32 i=0;i<Route.Num();i+=3) Link.Route.Add(Route[i]);
  Link.Route.Add(HighNav.Location);
  Links.Add(MoveTemp(Link));
 }
 // Remember some floors above the main level: random roaming goes by floor area, so bots would rarely pick them.
 TMap<int32,int32> Bands;
 for (const FStairFloorSample& Floor:Floors) if (Floor.bOnNav) ++Bands.FindOrAdd(FMath::FloorToInt32(Floor.Z/100.f));
 int32 MainBand=0,MainCount=0;
 for (const TPair<int32,int32>& Band:Bands) if (Band.Value>MainCount) { MainCount=Band.Value; MainBand=Band.Key; }
 TArray<FVector> Elevated;
 for (int32 Index=0;Index<Floors.Num();++Index) if (Floors[Index].bOnNav && Floors[Index].Z>=MainBand*100.f+200.f) Elevated.Add(WorldAt(Index));
 while (Elevated.Num() && UpperFloors.Num()<200) { const int32 Pick=FMath::RandRange(0,Elevated.Num()-1); UpperFloors.Add(Elevated[Pick]); Elevated.RemoveAtSwap(Pick); }
 for (const FLink& Link:Links) UpperFloors.Add(Link.Upper);
 UE_LOG(LogTemp,Display,TEXT("Crosshair stair links: %d from %d candidates across %d floors in %.0f ms"),Links.Num(),Candidates.Num(),Floors.Num(),(FPlatformTime::Seconds()-Started)*1000.0);
}
bool ACrosshairStairLinks::PickUpperGoal(const FVector& From,FVector& OutGoal) const
{
 UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 const ANavigationData* NavData=Navigation ? Navigation->GetDefaultNavDataInstance() : nullptr;
 if (!NavData || UpperFloors.IsEmpty()) return false;
 FNavLocation Start;
 if (!Navigation->ProjectPointToNavigation(From,Start,FVector(60,60,250))) return false;
 // The candidates are unverified because links register after they are chosen; this is where reachability counts.
 for (int32 Try=0;Try<12;++Try)
 {
  FNavLocation Goal;
  if (!Navigation->ProjectPointToNavigation(UpperFloors[FMath::RandRange(0,UpperFloors.Num()-1)],Goal,FVector(30,30,60))) continue;
  FPathFindingQuery Query(nullptr,*NavData,Start.Location,Goal.Location);
  Query.SetAllowPartialPaths(false);
  if (Navigation->TestPathSync(Query)) { OutGoal=Goal.Location; return true; }
 }
 return false;
}
bool ACrosshairStairLinks::PickTrip(const FVector& From,TArray<FVector>& OutRoute) const
{
 UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
 const ANavigationData* NavData=Navigation ? Navigation->GetDefaultNavDataInstance() : nullptr;
 FNavLocation Start;
 if (!NavData || Links.IsEmpty() || !Navigation->ProjectPointToNavigation(From,Start,FVector(60,60,250))) return false;
 auto CanWalkTo=[&](const FVector& End)
 {
  FPathFindingQuery Query(nullptr,*NavData,Start.Location,End);
  Query.SetAllowPartialPaths(false);
  return Navigation->TestPathSync(Query);
 };
 for (int32 Try=0;Try<8;++Try)
 {
  const FLink& Link=Links[FMath::RandRange(0,Links.Num()-1)];
  if (CanWalkTo(Link.Lower)) { OutRoute=Link.Route; return true; }
  if (CanWalkTo(Link.Upper)) { OutRoute=Link.Route; Algo::Reverse(OutRoute); return true; }
 }
 return false;
}
