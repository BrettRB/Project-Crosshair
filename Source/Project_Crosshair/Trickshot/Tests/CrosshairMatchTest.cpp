#include "CrosshairMatchTest.h"
#include "../CrosshairBot.h"
#include "../CrosshairCharacter.h"
#include "../CrosshairDummy.h"
#include "../CrosshairGame.h"
#include "../CrosshairMapLibrary.h"
#include "../CrosshairMatch.h"
#include "../CrosshairMatchSession.h"
#include "../CrosshairReplaySubsystem.h"
#include "../CrosshairStartup.h"
#include "../CrosshairWeapon.h"
#include "../CrosshairThrowable.h"
#include "../CrosshairLocomotion.h"
#include "../CrosshairPractice.h"
#include "AIController.h"
#include "../CrosshairEnvironment.h"
#include "../CrosshairGraphics.h"
#include "Scalability.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/SkeletalMesh.h"
#if WITH_EDITORONLY_DATA
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#endif
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

namespace { bool Flag(const TCHAR* Name) { return FParse::Param(FCommandLine::Get(),Name); } }

bool UCrosshairMatchTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
 return false;
#else
 return Flag(TEXT("CrosshairSmoke")) && (Flag(TEXT("CrosshairMatchSmoke")) || Flag(TEXT("CrosshairMatchLayoutProbe")) || Flag(TEXT("CrosshairLanHost")) || Flag(TEXT("CrosshairLanClient")) || Flag(TEXT("CrosshairSoldierProbe")) || Flag(TEXT("CrosshairGraphicsCheck")));
#endif
}
void UCrosshairMatchTest::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 Collection.InitializeDependency<UCrosshairReplaySubsystem>();
 Collection.InitializeDependency<UCrosshairMatchSession>();
 Started=StepStarted=FPlatformTime::Seconds();
}
bool UCrosshairMatchTest::Check(bool Value,const TCHAR* Message)
{
 UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_%s step=%d %s"),Value ? TEXT("PASS") : TEXT("FAIL"),Step,Message);
 if (!Value) { bDone=true; FPlatformMisc::RequestExitWithStatus(false,1); }
 return Value;
}
void UCrosshairMatchTest::Next(int32 NewStep) { Step=NewStep; StepStarted=FPlatformTime::Seconds(); }

void UCrosshairMatchTest::Probe(const TCHAR* Map,FVector2D Center,float HalfSize,float Cell)
{
 UWorld* World=GetWorld();
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairMatchProbe),true);
 for (TActorIterator<ACrosshairCharacter> It(World);It;++It) Params.AddIgnoredActor(*It);
 const int32 Cells=FMath::CeilToInt(HalfSize*2/Cell);
 UE_LOG(LogTemp,Display,TEXT("MATCH_LAYOUT %s GRID x_from=%.0f y_from=%.0f cell=%.0f cells=%d (rows descend in X, columns ascend in Y; metres; ~ steep)"),Map,Center.X+HalfSize,Center.Y-HalfSize,Cell,Cells);
 for (int32 Row=0;Row<Cells;++Row)
 {
  const float X=Center.X+HalfSize-(Row+.5f)*Cell;
  FString Line;
  for (int32 Col=0;Col<Cells;++Col)
  {
   const float Y=Center.Y-HalfSize+(Col+.5f)*Cell;
   FHitResult Hit;
   if (World->LineTraceSingleByChannel(Hit,FVector(X,Y,6000),FVector(X,Y,-6000),ECC_Visibility,Params))
    Line+=FString::Printf(TEXT("%5.1f%s"),Hit.ImpactPoint.Z/100.f,Hit.ImpactNormal.Z<.7f ? TEXT("~") : TEXT(" "));
   else Line+=TEXT("    . ");
  }
  UE_LOG(LogTemp,Display,TEXT("MATCH_LAYOUT %s ROW %7.0f %s"),Map,X,*Line);
 }
 int32 Count=0;
 for (TActorIterator<AStaticMeshActor> It(World);It && Count<250;++It)
 {
  const UStaticMeshComponent* Mesh=It->GetStaticMeshComponent();
  if (!Mesh || !Mesh->GetStaticMesh()) continue;
  const FBoxSphereBounds Bounds=Mesh->Bounds;
  const float Extent=FMath::Max(Bounds.BoxExtent.X,Bounds.BoxExtent.Y);
  if (Extent<250 || Extent>HalfSize || FMath::Abs(Bounds.Origin.X-Center.X)>HalfSize || FMath::Abs(Bounds.Origin.Y-Center.Y)>HalfSize) continue;
  UE_LOG(LogTemp,Display,TEXT("MATCH_LAYOUT %s MESH %s at=(%.0f,%.0f,%.0f) ext=(%.0f,%.0f,%.0f)"),Map,*Mesh->GetStaticMesh()->GetName(),Bounds.Origin.X,Bounds.Origin.Y,Bounds.Origin.Z,Bounds.BoxExtent.X,Bounds.BoxExtent.Y,Bounds.BoxExtent.Z);
  ++Count;
 }
}
void UCrosshairMatchTest::TickProbe()
{
 UWorld* World=GetWorld();
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 const double Elapsed=FPlatformTime::Seconds()-StepStarted;
 const FString MapName=World->GetMapName();
 const APawn* Pawn=World->GetFirstPlayerController() ? World->GetFirstPlayerController()->GetPawn() : nullptr;
 switch (Step)
 {
 case 0: if (Pawn && Elapsed>1) { Library->PlayMap(Flag(TEXT("CrosshairProbeGridOnly")) ? 2 : 1); Next(Flag(TEXT("CrosshairProbeGridOnly")) ? 2 : 1); } return;
 case 1:
  if (!Pawn || !MapName.Contains(TEXT("L_Nuketown")) || Elapsed<4) return;
  Probe(TEXT("Nuketown"),FVector2D(-600,1300),4800,200);
  Library->PlayMap(2); Next(2); return;
 case 2:
  if (!Pawn || !MapName.Contains(TEXT("L_Grid")) || Elapsed<4) return;
  // The built area sits inside a much larger snow plane.
  Probe(TEXT("Grid"),FVector2D(1000,-3600),6500,260);
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_LAYOUT_OK")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
 default: return;
 }
}

// Two-process same-network test: one instance runs -CrosshairLanHost, a second -CrosshairLanClient.
// Read-only body measurement for fitting soldier gear (-CrosshairSmoke -CrosshairSoldierProbe).
// Logs, per body region, mesh-space surface extents by height using each vertex's dominant bone.
void UCrosshairMatchTest::ProbeSoldier()
{
#if WITH_EDITORONLY_DATA
 const USkeletalMesh* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Crosshair/Soldiers/SKM_PracticeSoldier.SKM_PracticeSoldier"));
 const FSkeletalMeshModel* Model=Body ? Body->GetImportedModel() : nullptr;
 if (!Check(Model && Model->LODModels.Num()>0,TEXT("Soldier body mesh model available"))) return;
 const FReferenceSkeleton& Ref=Body->GetRefSkeleton();
 TArray<FTransform> Global; Global.SetNum(Ref.GetNum());
 for (int32 i=0;i<Ref.GetNum();++i) { const int32 Parent=Ref.GetParentIndex(i); Global[i]=Parent==INDEX_NONE ? Ref.GetRefBonePose()[i] : Ref.GetRefBonePose()[i]*Global[Parent]; }
 for (const TCHAR* Name:{TEXT("pelvis"),TEXT("spine_01"),TEXT("spine_02"),TEXT("spine_03"),TEXT("neck_01"),TEXT("head"),TEXT("clavicle_l"),TEXT("upperarm_l"),TEXT("lowerarm_l"),TEXT("hand_l"),TEXT("thigh_l"),TEXT("calf_l"),TEXT("foot_l")})
 {
  const int32 Index=Ref.FindBoneIndex(Name);
  if (Index!=INDEX_NONE) UE_LOG(LogTemp,Display,TEXT("SOLDIER_BONE %s pos=%s xaxis=%s"),Name,*Global[Index].GetLocation().ToCompactString(),*Global[Index].GetUnitAxis(EAxis::X).ToCompactString());
 }
 struct FRegion { const TCHAR* Name; TArray<FName> Bones; TMap<int32,FBox> Slices; FBox All=FBox(ForceInit); };
 TArray<FRegion> Regions={
  {TEXT("torso"),{TEXT("spine_02"),TEXT("spine_03"),TEXT("spine_04"),TEXT("spine_05")}},
  {TEXT("belly"),{TEXT("pelvis"),TEXT("spine_01")}},
  {TEXT("head"),{TEXT("head")}},
  {TEXT("neck"),{TEXT("neck_01"),TEXT("neck_02")}},
  {TEXT("thigh_l"),{TEXT("thigh_l")}},
  {TEXT("calf_l"),{TEXT("calf_l")}},
  {TEXT("upperarm_l"),{TEXT("upperarm_l")}}};
 const FSkeletalMeshLODModel& Lod=Model->LODModels[0];
 for (const FSkelMeshSection& Section:Lod.Sections)
  for (const FSoftSkinVertex& Vertex:Section.SoftVertices)
  {
   int32 Best=0; for (int32 k=1;k<MAX_TOTAL_INFLUENCES;++k) if (Vertex.InfluenceWeights[k]>Vertex.InfluenceWeights[Best]) Best=k;
   const int32 BoneIndex=Section.BoneMap.IsValidIndex(Vertex.InfluenceBones[Best]) ? Section.BoneMap[Vertex.InfluenceBones[Best]] : INDEX_NONE;
   if (BoneIndex==INDEX_NONE) continue;
   const FName Bone=Ref.GetBoneName(BoneIndex);
   const FVector P=FVector(Vertex.Position);
   for (FRegion& Region:Regions) if (Region.Bones.Contains(Bone)) { Region.All+=P; Region.Slices.FindOrAdd(FMath::FloorToInt(P.Z/4.f))+=P; }
  }
 for (FRegion& Region:Regions)
 {
  UE_LOG(LogTemp,Display,TEXT("SOLDIER_REGION %s all min=%s max=%s"),Region.Name,*Region.All.Min.ToCompactString(),*Region.All.Max.ToCompactString());
  Region.Slices.KeySort([](int32 A,int32 B){ return A<B; });
  for (const TPair<int32,FBox>& Slice:Region.Slices)
   UE_LOG(LogTemp,Display,TEXT("SOLDIER_SLICE %s z=%d x=[%.1f,%.1f] y=[%.1f,%.1f]"),Region.Name,Slice.Key*4,Slice.Value.Min.X,Slice.Value.Max.X,Slice.Value.Min.Y,Slice.Value.Max.Y);
 }
 UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_SOLDIER_PROBE_OK"));
#endif
 bDone=true; FPlatformMisc::RequestExitWithStatus(false,0);
}
// Graphics check (-CrosshairSmoke -CrosshairGraphicsCheck): the shared lighting rig is applied on
// every built-in map and on a freshly imported user map, with a screenshot of each.
void UCrosshairMatchTest::TickGraphics()
{
 UWorld* World=GetWorld();
 auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 const double Elapsed=FPlatformTime::Seconds()-StepStarted;
 const FString MapName=World->GetMapName();
 APlayerController* PC=World->GetFirstPlayerController();
 if (FPlatformTime::Seconds()-Started>600) { Check(false,TEXT("Graphics check timeout")); return; }
 auto Capture=[&](const TCHAR* Name){ FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,false,false); };
 auto CheckRig=[&](const TCHAR* Map)->bool
 {
  const auto* Environment=World->GetSubsystem<UCrosshairEnvironmentSubsystem>();
  int32 Suns=0,Skies=0,Fogs=0,Clouds=0,Grades=0;
  for (TActorIterator<ADirectionalLight> It(World);It;++It) if (const auto* Light=Cast<UDirectionalLightComponent>(It->GetLightComponent()); Light && Light->IsUsedAsAtmosphereSunLight()) ++Suns;
  for (TActorIterator<ASkyLight> It(World);It;++It) if (It->GetLightComponent() && It->GetLightComponent()->bRealTimeCapture) ++Skies;
  for (TActorIterator<AExponentialHeightFog> It(World);It;++It) if (It->GetComponent() && It->GetComponent()->bEnableVolumetricFog) ++Fogs;
  for (TActorIterator<AVolumetricCloud> It(World);It;++It) ++Clouds;
  for (TActorIterator<APostProcessVolume> It(World);It;++It) if (It->ActorHasTag(UCrosshairEnvironmentSubsystem::EnvironmentTag) && It->bUnbound) ++Grades;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_GRAPHICS_INFO %s suns=%d realtimeSky=%d volumetricFog=%d clouds=%d grade=%d"),Map,Suns,Skies,Fogs,Clouds,Grades);
  return Check(Environment && Environment->IsApplied() && Suns==1 && Skies>=1 && Fogs>=1 && Clouds>=1 && Grades==1,
   *FString::Printf(TEXT("%s gets the shared realistic lighting rig"),Map));
 };
 auto ProbeArms=[&](const TCHAR* Map)
 {
  auto* Hero=PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
  if (!Hero) return;
  USkeletalMeshComponent* Arms=Hero->GetFirstPersonMesh();
  const FTransform Eye=Hero->GetFirstPersonCameraComponent()->GetComponentTransform();
  auto Local=[&](const FVector& W){ return Eye.InverseTransformPosition(W).ToString(); };
  const auto* View=Cast<UCrosshairViewmodelAnimInstance>(Arms->GetAnimInstance());
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_ARMS %s ik=%d shoulder=%s elbow=%s hand=%s goal=%s"),Map,View ? View->bLeftHandIK : -1,
   *Local(Arms->GetBoneLocation(TEXT("upperarm_l"))),*Local(Arms->GetBoneLocation(TEXT("lowerarm_l"))),*Local(Arms->GetBoneLocation(TEXT("hand_l"))),
   View ? *Local(Arms->GetComponentTransform().TransformPosition(View->LeftHandTarget)) : TEXT("-"));
  if (auto* Weapon=Hero->Inventory->GetPresentationWeapon())
  {
   const FBox Box=Weapon->PresentationMesh->Bounds.GetBox();
   const FTransform M=Weapon->PresentationMesh->GetComponentTransform();
   const FBox LocalBox=Weapon->PresentationMesh->GetStaticMesh() ? Weapon->PresentationMesh->GetStaticMesh()->GetBoundingBox() : FBox(ForceInit);
   UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_ARMS weapon local=%s origin=%s +X=%s +Z=%s worldbox=%s..%s"),*LocalBox.ToString(),*Local(M.GetLocation()),
    *Eye.InverseTransformVector(M.TransformVector(FVector::XAxisVector)).ToString(),*Eye.InverseTransformVector(M.TransformVector(FVector::ZAxisVector)).ToString(),*Local(Box.Min),*Local(Box.Max));
  }
 };
 auto Look=[&](float Pitch){ if (PC) { FRotator View=PC->GetControlRotation(); View.Pitch=Pitch; PC->SetControlRotation(View); } };
 const bool bReady=PC && PC->GetPawn();
 switch (Step)
 {
 case 0:
  if (!bReady || !MapName.Contains(TEXT("L_Practice")) || Elapsed<6) return;
  if (!CheckRig(TEXT("Testing Map"))) return;
  Look(-4); Capture(TEXT("GfxPractice.png")); Library->PlayMap(1); Next(1); return;
 case 1:
  if (!bReady || !MapName.Contains(TEXT("L_Nuketown")) || Elapsed<10) return;
  if (!CheckRig(TEXT("Nuketown"))) return;
  Look(-2); Capture(TEXT("GfxNuketown.png")); Next(2); return;
 case 2:
 {
  if (Elapsed<2) return;
  // Laptop presets drive engine scalability and resolution; the menu row cycles them.
  auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
  FCrosshairSettings& S=Replay->GetSettings();
  for (int32 Preset=0;Preset<CrosshairGraphics::PresetCount;++Preset)
  {
   CrosshairGraphics::SelectPreset(S,Preset); CrosshairGraphics::Apply(S);
   const Scalability::FQualityLevels Levels=Scalability::GetQualityLevels();
   UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_GRAPHICS_INFO preset=%s shadows=%d gi=%d resolution=%.0f rt=%d"),CrosshairGraphics::PresetName(Preset),Levels.ShadowQuality,Levels.GlobalIlluminationQuality,Levels.ResolutionQuality,int32(CrosshairGraphics::IsRayTracingActive()));
   if (!Check(Levels.ShadowQuality==Preset && Levels.GlobalIlluminationQuality==Preset && FMath::IsNearlyEqual(Levels.ResolutionQuality,float(CrosshairGraphics::PresetResolution(Preset))),
    *FString::Printf(TEXT("%s preset applies engine quality %d"),CrosshairGraphics::PresetName(Preset),Preset))) return;
  }
  if (auto* Menu=Cast<ACrosshairPlayerController>(PC))
  {
   CrosshairGraphics::SelectPreset(S,1);
   if (!Menu->bMenuOpen) Menu->ToggleMenu();
   Menu->SetMenuTab(2);
   Menu->AdjustMenuRow(Menu->GetGraphicsRow(),1);
   const bool bListed=Menu->GetVisibleMenuRows().Contains(Menu->GetGraphicsRow()+1);
   Menu->ToggleMenu();
   if (!Check(bListed,TEXT("Graphics tab lists the ray tracing toggle"))) return;
   if (!Check(S.GraphicsPreset==2 && Scalability::GetQualityLevels().ShadowQuality==2,TEXT("Graphics menu row changes the preset"))) return;
  }
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_GRAPHICS_INFO ray tracing supported=%d"),int32(CrosshairGraphics::IsRayTracingSupported()));
  CrosshairGraphics::SelectPreset(S,3); S.bRayTracing=false; CrosshairGraphics::Apply(S);
  Next(10); return;
 }
 case 10:
 {
  if (Elapsed<4) return;
  if (!Check(!CrosshairGraphics::IsRayTracingActive(),TEXT("Ray tracing turns off"))) return;
  Look(-2); Capture(TEXT("GfxNuketownRayTracingOff.png"));
  FCrosshairSettings& S=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
  S.bRayTracing=true; CrosshairGraphics::Apply(S);
  Next(11); return;
 }
 case 11:
 {
  if (Elapsed<4) return;
  if (!Check(CrosshairGraphics::IsRayTracingActive()==CrosshairGraphics::IsRayTracingSupported(),TEXT("Ray tracing turns back on where the GPU supports it"))) return;
  Capture(TEXT("GfxNuketownRayTracingOn.png"));
  Next(12); return;
 }
 case 12:
  if (Elapsed<2) return;
  Library->PlayMap(2); Next(3); return;
 case 3:
  if (!bReady || !MapName.Contains(TEXT("L_Grid")) || Elapsed<10) return;
  if (!CheckRig(TEXT("Grid"))) return;
  Look(-2); ProbeArms(TEXT("Grid")); Capture(TEXT("GfxGrid.png")); Next(20); return;
 case 20:
  if (Elapsed<2) return;
  if (!Check(Library->MapName(3)==TEXT("Space Station") && Library->PlayMap(3),TEXT("Space Station loads from the map list"))) return;
  Next(21); return;
 case 21:
 {
  if (!bReady || !MapName.Contains(TEXT("L_SpaceStation")) || Elapsed<10) return;
  // The station brings its own vacuum lighting: the shared Earth-sky rig must stay out.
  const auto* Environment=World->GetSubsystem<UCrosshairEnvironmentSubsystem>();
  int32 Suns=0,Spawns=0,Skies=0;
  for (TActorIterator<ADirectionalLight> It(World);It;++It) ++Suns;
  for (TActorIterator<ACrosshairSpawnPoint> It(World);It;++It) ++Spawns;
  for (TActorIterator<ASkyAtmosphere> It(World);It;++It) ++Skies;
  if (const APawn* Body=PC->GetPawn()) { FHitResult Floor; FCollisionQueryParams Ignore; Ignore.AddIgnoredActor(Body); const bool bFloor=World->LineTraceSingleByChannel(Floor,Body->GetActorLocation(),Body->GetActorLocation()-FVector(0,0,500),ECC_Visibility,Ignore); UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_GRAPHICS_INFO Space Station pawn=%s velocity=%s floor=%d %s"),*Body->GetActorLocation().ToString(),*Body->GetVelocity().ToString(),int32(bFloor),bFloor && Floor.GetActor() ? *Floor.GetActor()->GetName() : TEXT("-")); }
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_GRAPHICS_INFO Space Station suns=%d atmospheres=%d spawns=%d rig=%d"),Suns,Skies,Spawns,int32(Environment && Environment->IsApplied()));
  if (!Check(Environment && !Environment->IsApplied() && Suns==1 && Skies==0,TEXT("Space Station keeps its own vacuum lighting"))) return;
  if (!Check(PC->GetPawn() && PC->GetPawn()->GetActorLocation().Z<5000.f,TEXT("Space Station spawns the player on the station deck, not on the sky"))) return;
  if (!Check(Spawns==21 && ACrosshairMatchGameMode::IsMatchMap(MapName),TEXT("Space Station keeps Nuketown's 21 match spawns"))) return;
  Look(-2); Capture(TEXT("GfxSpaceStation.png")); Next(22); return;
 }
 case 22:
  if (Elapsed<2) return;
  // Rise above the rooftops for a clear view of the sky.
  if (auto* Body=PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr) { Body->GetCharacterMovement()->SetMovementMode(MOVE_Flying); Body->SetActorLocation(Body->GetActorLocation()+FVector(0,0,1300)); }
  if (PC) PC->SetControlRotation(FRotator(8,-20,0));   // toward the planet drawn in the sky
  Next(23); return;
 case 23:
  if (Elapsed<2.5) return;
  Capture(TEXT("GfxSpaceStationPlanet.png")); Next(24); return;
 case 24:
  if (Elapsed<2) return;
  Next(4); return;
 case 4:
 {
  if (Elapsed<2) return;
  FString Error;
  if (!Check(Library->Import(FPaths::ProjectDir()/TEXT("ContentSource/UserMapsExample/map.json"),Error),*(TEXT("Example user map imports: ")+Error))) return;
  if (!Check(Library->PlayMap(Library->MapCount()-1),TEXT("Imported map loads"))) return;
  Next(5); return;
 }
 case 5:
  if (!bReady || !MapName.Contains(TEXT("L_UserMap")) || Elapsed<8) return;
  if (!CheckRig(TEXT("Imported user map"))) return;
  Look(-6); Capture(TEXT("GfxImported.png")); Next(6); return;
 case 6:
  if (Elapsed<3) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_GRAPHICS_OK")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
 default: return;
 }
}
void UCrosshairMatchTest::TickLan()
{
 UWorld* World=GetWorld();
 auto* Session=GetGameInstance()->GetSubsystem<UCrosshairMatchSession>();
 const double Elapsed=FPlatformTime::Seconds()-StepStarted;
 const APlayerController* PC=World->GetFirstPlayerController();
 const ACrosshairMatchState* Match=ACrosshairMatchState::Get(World);
 if (FPlatformTime::Seconds()-Started>300) { Check(false,TEXT("LAN test timeout")); return; }
 // Both sides run forward sprinting so each can check how the other player moves.
 auto RunForward=[&]()
 {
  auto* Mine=PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
  if (Mine && !Mine->Health->bDead) { Mine->SetBotInput(false,true); Mine->AddMovementInput(Mine->GetActorForwardVector()); }
  return Mine;
 };
 if (Flag(TEXT("CrosshairLanHost")))
 {
  if (Step==0) { if (PC && PC->GetPawn() && Elapsed>1) { Check(Session->HostLanMatch(),TEXT("Host starts a LAN session")); Next(1); } return; }
  if (!Match || World->GetNetMode()!=NM_ListenServer) return;
  int32 Humans=0; for (APlayerState* Player:Match->PlayerArray) Humans+=Player && !Player->IsABot() && !Player->IsOnlyASpectator() ? 1 : 0;
  if (Step==1) { UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_HOST_READY")); Next(2); }
  if (Step==2)
  {
   if (Humans<2) return;
   Check(Session->HasLanSession(),TEXT("Host sees a second human player join over LAN")); Next(3); return;
  }
  // The joined player's pawn as simulated on the host: sprint intent must arrive, or the host corrects them to walk speed.
  const ACrosshairCharacter* Mine=RunForward();
  for (TActorIterator<ACrosshairCharacter> It(World);It;++It)
   if (*It!=Mine && !It->IsBot()) RemoteSpeedMax=FMath::Max(RemoteSpeedMax,float(It->GetVelocity().Size2D()));
  if (Elapsed<20) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_INFO host saw the joined player's top speed %.0f cm/s"),RemoteSpeedMax);
  if (!Check(RemoteSpeedMax>=560.f,TEXT("Joined player's sprint is simulated at sprint speed on the host (no walk-speed correction)"))) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_HOST_OK")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
 }
 switch (Step)
 {
 case 0: if (PC && PC->GetPawn() && Elapsed>3) { Session->FindLanMatches(); Next(1); } return;
 case 1:
  if (Session->bSearching) return;
  if (Session->LanResults.IsEmpty()) { UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_CLIENT_SEARCH %s"),*Session->LanStatus); Next(0); return; }
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_CLIENT_FOUND %s"),*Session->LanResults[0]);
  if (!Check(Session->JoinLanMatch(0),TEXT("Client joins the first LAN match found"))) return;
  Next(2); return;
 case 2:
  if (World->GetNetMode()!=NM_Client || !Match || !PC || !PC->GetPawn() || Elapsed<3) return;
  if (!Check(Match->PlayerArray.Num()>=2,TEXT("Client receives the host's replicated match and spawns"))) return;
  Next(3); return;
 case 3:
 {
  // The host's soldier as seen on this client: it should move and step through walk/jog cycles, not slide.
  const ACrosshairCharacter* Mine=RunForward();
  for (TActorIterator<ACrosshairCharacter> It(World);It;++It)
  {
   if (*It==Mine || It->IsBot()) continue;
   const float Speed=It->GetVelocity().Size2D();
   HostSpeedMax=FMath::Max(HostSpeedMax,Speed);
   if (const auto* Anim=Cast<UCrosshairLocomotionAnimInstance>(It->GetMesh()->GetAnimInstance()); Anim && Speed>200.f)
    HostSteppingMax=FMath::Max(HostSteppingMax,Anim->GetMovingWeight());
  }
  if (Elapsed<10) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_INFO client saw the host's top speed %.0f cm/s, stepping weight %.2f"),HostSpeedMax,HostSteppingMax);
  if (!Check(HostSpeedMax>300.f,TEXT("Client sees the host player moving"))) return;
  if (!Check(HostSteppingMax>.8f,TEXT("The host's soldier plays walk/jog locomotion on the client instead of sliding"))) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_LAN_CLIENT_OK mode=%s map=%s players=%d"),Match->IsTeamMode() ? TEXT("TDM") : TEXT("FFA"),*Match->MapName,Match->PlayerArray.Num());
  bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
 }
 default: return;
 }
}
void UCrosshairMatchTest::Tick(float Delta)
{
 UWorld* World=GetWorld(); if (!World || !World->IsGameWorld() || bDone) return;
 if (GetGameInstance()->GetSubsystem<UCrosshairStartup>()->IsPreparing()) { Started=StepStarted=FPlatformTime::Seconds(); return; }
 if (FPlatformTime::Seconds()-Started>480) { Check(false,TEXT("Timeout")); return; }
 if (Flag(TEXT("CrosshairSoldierProbe"))) { ProbeSoldier(); return; }
 if (Flag(TEXT("CrosshairMatchLayoutProbe"))) { TickProbe(); return; }
 if (Flag(TEXT("CrosshairLanHost")) || Flag(TEXT("CrosshairLanClient"))) { TickLan(); return; }
 // The rejected Testing-map match lasts a single frame; observe it before any visual-harness wait.
 if (Step==1)
  if (const auto* Rejecting=World->GetAuthGameMode<ACrosshairMatchGameMode>(); Rejecting && World->GetMapName().Contains(TEXT("L_Practice")))
  {
   const ACrosshairMatchState* State=ACrosshairMatchState::Get(World);
   int32 Count=0; for (TActorIterator<ACrosshairCharacter> It(World);It;++It) ++Count;
   bSawInvalid=bSawInvalid || (Rejecting->bInvalidMap && State && State->bMatchOver && Count<=1);
   return;
  }
#if WITH_EDITOR
 if ((Flag(TEXT("CrosshairMatchVisual")) || Flag(TEXT("CrosshairGraphicsCheck"))) && GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
 if (Flag(TEXT("CrosshairGraphicsCheck"))) { TickGraphics(); return; }
 const double Elapsed=FPlatformTime::Seconds()-StepStarted;
 auto Capture=[&](const TCHAR* Name){ if (Flag(TEXT("CrosshairMatchVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,false,false); };
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 auto* Session=GetGameInstance()->GetSubsystem<UCrosshairMatchSession>();
 FCrosshairBotMatchOptions& Options=Replay->GetSettings().BotMatch;
 auto* PC=Cast<ACrosshairPlayerController>(World->GetFirstPlayerController());
 auto* Pawn=PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
 ACrosshairMatchState* Match=ACrosshairMatchState::Get(World);
 auto* Mode=World->GetAuthGameMode<ACrosshairMatchGameMode>();
 const FString MapName=World->GetMapName();
 auto* Me=PC ? PC->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
 auto Characters=[&]{ TArray<ACrosshairCharacter*> Result; for (TActorIterator<ACrosshairCharacter> It(World);It;++It) Result.Add(*It); return Result; };
 auto NearestSpawn=[&](const FVector& At,float& Distance)->ACrosshairSpawnPoint*
 {
  ACrosshairSpawnPoint* Best=nullptr; Distance=FLT_MAX;
  if (Mode) for (ACrosshairSpawnPoint* Point:Mode->GetSpawnPoints()) if (Point && FVector::Dist(Point->GetActorLocation(),At)<Distance) { Distance=FVector::Dist(Point->GetActorLocation(),At); Best=Point; }
  return Best;
 };
 auto LiveBot=[&](int32 Team)->ACrosshairCharacter*
 {
  for (ACrosshairCharacter* Character:Characters())
  {
   const auto* State=Character->GetPlayerState<ACrosshairMatchPlayerState>();
   if (Character->IsBot() && !Character->Health->bDead && State && (Team<0 || State->Team==Team)) return Character;
  }
  return nullptr;
 };
 auto Key=[&](FKey K,EInputEvent Event){ PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released ? 0.f : 1.f)); };
 // Every spawn must stand on ground with a clear capsule, lie on the navmesh and reach the
 // other spawns by a complete path (catches points outside the playable area).
 auto NavReady=[&]
 {
  // Runtime generation may not have queued tiles yet, so require real coverage at the map hub.
  UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
  if (!Navigation || !Navigation->GetDefaultNavDataInstance() || Navigation->IsNavigationBuildInProgress()) return false;
  FNavLocation Hub;
  for (TActorIterator<APlayerStart> It(World);It;++It) return Navigation->ProjectPointToNavigation(It->GetActorLocation(),Hub,FVector(300,300,400));
  return false;
 };
 auto ValidateSpawns=[&](const TCHAR* Map)->bool
 {
  UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
  if (!Mode || !NavReady()) return Check(false,TEXT("Match navmesh is built"));
  FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairSpawnCheck),false);
  for (ACrosshairCharacter* Character:Characters()) Params.AddIgnoredActor(Character);
  const TArray<TObjectPtr<ACrosshairSpawnPoint>>& Points=Mode->GetSpawnPoints();
  // Reachability target: the map's central practice start, which is in the main play space.
  FVector Hub=Points[0]->GetActorLocation();
  for (TActorIterator<APlayerStart> It(World);It;++It) Hub=It->GetActorLocation();
  FNavLocation HubNav; Navigation->ProjectPointToNavigation(Hub,HubNav,FVector(300,300,400));
  auto Valid=[&](const FVector& At,bool& bGround,bool& bClear,bool& bNav,bool& bReach)
  {
   FHitResult Ground; FNavLocation OnNav;
   bGround=World->LineTraceSingleByChannel(Ground,At,At-FVector(0,0,250),ECC_Visibility,Params) && Ground.ImpactNormal.Z>.7f;
   bClear=!World->OverlapBlockingTestByChannel(At+FVector(0,0,8),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(30.f,80.f),Params);
   bNav=Navigation->ProjectPointToNavigation(At,OnNav,FVector(120,120,250));
   const UNavigationPath* Path=bNav ? UNavigationSystemV1::FindPathToLocationSynchronously(World,OnNav.Location,HubNav.Location) : nullptr;
   bReach=Path && Path->IsValid() && !Path->IsPartial();
   return bGround && bClear && bNav && bReach;
  };
  int32 Bad=0;
  for (const ACrosshairSpawnPoint* Point:Points)
  {
   const FVector At=Point->GetActorLocation();
   bool bGround,bClear,bNav,bReach;
   if (Valid(At,bGround,bClear,bNav,bReach)) continue;
   ++Bad;
   UE_LOG(LogTemp,Warning,TEXT("CROSSHAIR_MATCH_SPAWN_BAD %s '%s' at %s ground=%d clear=%d nav=%d reach=%d"),Map,*Point->Area,*At.ToCompactString(),bGround,bClear,bNav,bReach);
   // Suggest the nearest valid standing spot (used to correct Scripts/setup_match_spawns.py).
   for (float Radius=100.f;Radius<=900.f;Radius+=100.f)
   {
    bool bFound=false;
    for (int32 Angle=0;Angle<360 && !bFound;Angle+=30)
    {
     const FVector Probe=At+FRotator(0,Angle,0).Vector()*Radius;
     FHitResult Floor;
     if (!World->LineTraceSingleByChannel(Floor,Probe+FVector(0,0,150),Probe-FVector(0,0,300),ECC_Visibility,Params)) continue;
     const FVector Stand=Floor.ImpactPoint+FVector(0,0,97);
     bool G,C,N,R;
     if (Valid(Stand,G,C,N,R)) { UE_LOG(LogTemp,Warning,TEXT("CROSSHAIR_MATCH_SPAWN_SUGGEST %s '%s' (%.0f, %.0f) z=%.0f"),Map,*Point->Area,Stand.X,Stand.Y,Floor.ImpactPoint.Z); bFound=true; }
    }
    if (bFound) break;
   }
  }
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO %s spawn audit: %d of %d valid"),Map,Points.Num()-Bad,Points.Num());
  if (Flag(TEXT("CrosshairMatchSpawnAudit")))
  {
   // Report-only placement tuning: '#' cells can stand and path to the hub, '.' are walkable ground outside the play space.
   FBox Area(ForceInit); for (const ACrosshairSpawnPoint* Point:Points) Area+=Point->GetActorLocation();
   Area=Area.ExpandBy(FVector(1500,1500,0));
   UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_REACH %s x_from=%.0f y_from=%.0f cell=250 (rows descend X, columns ascend Y)"),Map,Area.Max.X,Area.Min.Y);
   for (float X=Area.Max.X;X>=Area.Min.X;X-=250.f)
   {
    FString Line;
    for (float Y=Area.Min.Y;Y<=Area.Max.Y;Y+=250.f)
    {
     FHitResult Floor; bool G,C,N,R;
     if (!World->LineTraceSingleByChannel(Floor,FVector(X,Y,1500),FVector(X,Y,-3500),ECC_Visibility,Params)) { Line+=TEXT(" "); continue; }
     Line+=Valid(Floor.ImpactPoint+FVector(0,0,97),G,C,N,R) ? TEXT("#") : TEXT(".");
    }
    UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_REACH %s %7.0f %s"),Map,X,*Line);
   }
   return true;
  }
  return Check(Bad==0,TEXT("All spawn points stand on clear ground on the navmesh and connect to the map"));
 };
 // Combat evidence gathered continuously: bots killing bots and bots hurting the player.
 if (Match && Mode)
 {
  for (const FCrosshairKillEntry& Entry:Match->KillFeed) bSawBotKillsBot|=Entry.Killer.Contains(TEXT("(bot)")) && Entry.Victim.Contains(TEXT("(bot)"));
  if (Pawn && Pawn->Health->LastAttacker.IsValid() && Pawn->Health->LastAttacker->IsA<ACrosshairBotController>()) bSawBotHitsPlayer=true;
  for (TActorIterator<ACrosshairCharacter> It(World);It;++It)
   if (const auto* Anim=Cast<UCrosshairLocomotionAnimInstance>(It->GetMesh()->GetAnimInstance()); Anim && It->IsBot() && It->GetVelocity().Size2D()>250.f && Anim->GetMovingWeight()>.8f) bSawBotStepping=true;
 }
 switch (Step)
 {
 case 0:
  if (!Pawn || Match || Elapsed<1) return;
  Options=FCrosshairBotMatchOptions(); Replay->SaveSettings();
  Check(Options.BotCount==7 && Options.ScoreLimit==30 && Options.TeamSize==6 && Options.TeamScoreLimit==75 && Options.TimeLimitMinutes==10,TEXT("COD default match options: 8-player FFA to 30, 6v6 TDM to 75, 10 minutes"));
  if (!Check(Replay->ChangePracticeMap(TEXT("/Game/Crosshair/Maps/L_Practice"),UCrosshairMatchSession::TravelOptions(Options,false)),TEXT("Forced match travel to the Testing map is requested"))) return;
  Next(1); return;
 case 1:
  if (!bSawInvalid) { if (Elapsed>90) Check(false,TEXT("Testing map match rejection observed")); return; }
  if (!Pawn || Match || Elapsed<1) return;
  if (!Check(MapName.Contains(TEXT("L_Practice")) && Pawn->CanAct(),TEXT("Testing map rejects matches (no bots) and returns to practice"))) return;
  // Start the match through the menus, as a player would.
  PC->ShowHome();
  if (!Check(PC->bHomeScreen && PC->GetVisibleMenuRows()[1]==4,TEXT("Home lists Offline match vs bots right after Trickshot practice"))) return;
  Next(50); return;
 case 50:
  if (Elapsed<.6) return;
  if (!bCapturedHome) { Capture(TEXT("MenuHome.png")); bCapturedHome=true; }
  if (Elapsed<3) return; // rendered screenshots complete a few frames later
  PC->ActivateMenuRow(4);
  if (!Check(PC->MenuTab==8 && !PC->bHomeScreen && PC->GetVisibleMenuRows().Last()==PC->GetBotMatchRow()+6,TEXT("Offline match page shows settings and a Start button"))) return;
  Next(51); return;
 case 51:
  if (Elapsed<.6) return;
  Capture(TEXT("MenuMatchSetup.png"));
  if (Elapsed<1.2) return;
  PC->ActivateMenuRow(PC->GetBotMatchRow()+6);
  Next(2); return;
 case 2:
 {
  if (!Match || !Mode || !MapName.Contains(TEXT("L_Nuketown")) || !Pawn) { if (Elapsed>120) Check(false,TEXT("Nuketown FFA loads")); return; }
  const TArray<ACrosshairCharacter*> All=Characters();
  if (All.Num()<8) { if (Elapsed>60) Check(false,TEXT("FFA spawns player plus seven bots")); return; }
  int32 Bots=0,AtSpawn=0; bool NoTeams=true;
  for (ACrosshairCharacter* Character:All)
  {
   Bots+=Character->IsBot() ? 1 : 0;
   AtSpawn+=Mode->GetLastSpawn(Character->GetController())!=nullptr ? 1 : 0;
   if (const auto* State=Character->GetPlayerState<ACrosshairMatchPlayerState>()) NoTeams&=State->Team==CrosshairMatchRules::NoTeam;
   SpawnedAt.Add(Character->GetController(),Character->GetActorLocation());
  }
  const UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO Nuketown spawn points=%d at-spawn=%d navmesh=%d"),Mode->GetSpawnPoints().Num(),AtSpawn,
   Navigation && Navigation->GetDefaultNavDataInstance() ? 1 : 0);
  if (!Check(All.Num()==8 && Bots==7 && NoTeams,TEXT("FFA lobby is the player plus seven bots, no teams"))) return;
  if (!Check(Mode->GetSpawnPoints().Num()>=12 && AtSpawn==8,TEXT("Everyone starts on Nuketown match spawn points"))) return;
  int32 Dummies=0; for (TActorIterator<ACrosshairDummy> It(World);It;++It) ++Dummies;
  if (!Check(Dummies==0 && Replay->IsMatchRecording(),TEXT("Matches remove practice targets and use the match kill recorder, not practice attempts"))) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO rules team=%d limit=%d remaining=%.1f canact=%d menu=%d dead=%d"),Match->IsTeamMode(),Match->ScoreLimit,Match->GetRemainingSeconds(),Pawn->CanAct(),PC->bMenuOpen,Pawn->Health->bDead);
  if (!Check(!Match->IsTeamMode() && Match->ScoreLimit==30 && Match->GetRemainingSeconds()>0 && Match->GetRemainingSeconds()<=600 && (Pawn->CanAct() || Pawn->Health->bDead),TEXT("FFA rules: 30 kills, 10 minute clock, player can act"))) return;
  Capture(TEXT("MatchFFA.png")); Next(3); return;
 }
 case 3:
 {
  if (Elapsed<1) return;
  ACrosshairCharacter* Bot=nullptr;
  for (ACrosshairCharacter* Candidate:Characters()) if (Candidate->IsBot() && !Candidate->Health->bDead && Candidate->Health->Health>=Candidate->Health->MaxHealth) { Bot=Candidate; break; }
  if (!Bot) return;
  Victim=Bot->GetController();
  const float Applied=Bot->Health->ApplyDamage(60,PC,nullptr,false,TEXT("Test"));
  if (!Check(Applied==60 && Bot->Health->Health<=40.f && !Bot->Health->bDead,TEXT("Damage lowers match health without killing"))) return;
  Next(4); return;
 }
 case 4:
 {
  auto* Bot=Victim.IsValid() ? Cast<ACrosshairCharacter>(Victim->GetPawn()) : nullptr;
  if (!Bot || Bot->Health->bDead) { UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO regen bot was killed by another bot; regen covered by Crosshair.Match.Rules")); Next(3); return; }
  if (Elapsed<5.5) return;
  if (World->GetTimeSeconds()-Bot->Health->LastDamageAt>5.f && !Check(Bot->Health->Health>40.f,TEXT("Health regenerates after the delay"))) return;
  const int32 KillsBefore=Me ? Me->Kills : 0;
  Bot->Health->ApplyDamage(500,PC,nullptr,true,TEXT("Test"));
  const auto* VictimState=Victim->GetPlayerState<ACrosshairMatchPlayerState>();
  if (!Check(Bot->Health->bDead && Me && Me->Kills==KillsBefore+1 && VictimState && VictimState->Deaths>=1,TEXT("Kill credits the killer and the victim's deaths"))) return;
  if (!Check(Match->KillFeed.Num()>0 && Match->KillFeed.Last().Killer==Me->GetPlayerName() && Match->KillFeed.Last().bHeadshot,TEXT("Kill feed records killer, victim and headshot"))) return;
  Next(5); return;
 }
 case 5:
 {
  if (Elapsed<Mode->RespawnDelay+.3) return;
  auto* Bot=Victim.IsValid() ? Cast<ACrosshairCharacter>(Victim->GetPawn()) : nullptr;
  if (!Bot || Bot->Health->bDead) { if (Elapsed>12) Check(false,TEXT("Victim respawns")); return; }
  float Distance; NearestSpawn(Bot->GetActorLocation(),Distance);
  if (!Check(Distance<400 && Bot->Health->Health==Bot->Health->MaxHealth,TEXT("Victim respawns at full health on a spawn point"))) return;
  Next(6); return;
 }
 case 6:
 {
  int32 Shots=0,Moved=0,BotKills=0;
  for (TActorIterator<ACrosshairBotController> It(World);It;++It)
  {
   Shots+=It->ShotsFired;
   if (const auto* State=It->GetPlayerState<ACrosshairMatchPlayerState>()) BotKills+=State->Kills;
   if (const FVector* Start=SpawnedAt.Find(*It); Start && It->GetPawn() && FVector::Dist2D(*Start,It->GetPawn()->GetActorLocation())>400) ++Moved;
  }
  if ((Shots==0 || Moved<3 || !NavReady() || !bSawBotKillsBot || !bSawBotHitsPlayer || !bSawBotStepping) && Elapsed<120) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO bots shots=%d moved=%d bot-kills=%d after %.0fs"),Shots,Moved,BotKills,Elapsed);
  if (!Check(Shots>0 && Moved>=3,TEXT("Bots roam the map and engage visible enemies"))) return;
  if (!Check(bSawBotKillsBot,TEXT("Bots shoot and kill each other"))) return;
  if (!Check(bSawBotHitsPlayer,TEXT("Bots shoot the player"))) return;
  if (!Check(bSawBotStepping,TEXT("Moving bots play walk/jog locomotion instead of sliding"))) return;
  if (!ValidateSpawns(TEXT("Nuketown"))) return;
  Capture(TEXT("MatchCombat.png")); Next(7); return;
 }
 case 7:
 {
  // Real weapon path: stand a bot in clear view and fire the player's equipped weapon.
  ACrosshairCharacter* Bot=LiveBot(-1);
  if (!Pawn || !Bot || !Pawn->CanAct()) return;
  const FVector Eye=Pawn->GetFirstPersonCameraComponent()->GetComponentLocation();
  FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairMatchTestView),false,Pawn);
  for (int32 Yaw=0;Yaw<360;Yaw+=30)
  {
   const FVector Direction=FRotator(0,Yaw,0).Vector();
   FHitResult Wall,Ground;
   if (World->LineTraceSingleByChannel(Wall,Eye,Eye+Direction*700,ECC_Visibility,Params)) continue;
   const FVector Stand=Pawn->GetActorLocation()+Direction*500;
   if (!World->LineTraceSingleByChannel(Ground,Stand,Stand-FVector(0,0,300),ECC_Visibility,Params)) continue;
   Bot->SetActorLocation(Ground.ImpactPoint+FVector(0,0,96),false,nullptr,ETeleportType::TeleportPhysics);
   PC->SetControlRotation((Bot->GetMesh()->GetBoneLocation(TEXT("spine_03"))-Eye).Rotation());
   Pawn->Inventory->ApplyLoadout(0,1,ECrosshairLethalType::Frag); // sniper: one body shot kills
   Pawn->SetBotInput(true,false);
   if (ACrosshairWeapon* Weapon=Pawn->Inventory->GetCurrent()) Weapon->ResetWeapon();
   ReplaysBefore=Replay->GetReplays().Num();
   // Hold the target still for a deterministic shot.
   if (auto* Brain=Cast<ACrosshairBotController>(Bot->GetController())) { Brain->SetActorTickEnabled(false); Brain->StopMovement(); }
   WorldMark=World->GetTimeSeconds();
   Victim=Bot->GetController(); Next(8); return;
  }
  if (Elapsed>20) Check(false,TEXT("Clear firing lane found"));
  return;
 }
 case 8:
 {
  auto* Bot=Victim.IsValid() ? Cast<ACrosshairCharacter>(Victim->GetPawn()) : nullptr;
  if (!Pawn || !Bot || Pawn->Health->bDead || Bot->Health->bDead)
  {
   // Bots really fight back: if either side died before the test shot, set it up again.
   if (auto* Brain=Cast<ACrosshairBotController>(Victim.Get())) Brain->SetActorTickEnabled(true);
   Next(7); return;
  }
  ACrosshairWeapon* Weapon=Pawn->Inventory->GetCurrent();
  // Wait in game time (rendered frames can be slow) for the sniper draw and scope to settle.
  if (World->GetTimeSeconds()-WorldMark<1.2 || !Weapon || Weapon->IsSwitching() || Pawn->GetAimAlpha()<.95f)
  {
   PC->SetControlRotation((Bot->GetMesh()->GetBoneLocation(TEXT("spine_03"))-Pawn->GetFirstPersonCameraComponent()->GetComponentLocation()).Rotation());
   if (Elapsed>20)
   {
    UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO sniper wait: world=%.2f weapon=%s switching=%d aim=%.2f canact=%d menu=%d paused=%d lethalbusy=%d reloading=%d aimheld=%d placing=%d"),
     World->GetTimeSeconds()-WorldMark,Weapon && Weapon->Definition ? *Weapon->Definition->DisplayName.ToString() : TEXT("-"),Weapon ? int32(Weapon->IsSwitching()) : -1,Pawn->GetAimAlpha(),
     Pawn->CanAct(),PC->bMenuOpen,World->IsPaused(),Pawn->Lethals->IsBusy(),Weapon ? int32(Weapon->bReloading) : -1,Pawn->IsAimHeld(),Pawn->Placement->bPlacing);
    Check(false,TEXT("Sniper ready to fire"));
   }
   return;
  }
  // Any sniper hit (even a 75% wallbang) is then lethal, so the kill-to-replay path always runs.
  Bot->Health->Health=FMath::Min(Bot->Health->Health,50.f);
  const float Before=Bot->Health->Health;
  Weapon->StartFire(); Weapon->StopFire(); Pawn->SetBotInput(false,false);
  if (auto* Brain=Cast<ACrosshairBotController>(Bot->GetController())) Brain->SetActorTickEnabled(true);
  if (!Check(Bot->Health->Health<Before || Bot->Health->bDead,TEXT("Player weapon fire damages a bot through the posed body hitboxes"))) return;
  if (!Check(Bot->Health->bDead,TEXT("Sniper body shot kills the bot"))) return;
  Next(19); return;
 }
 case 19:
 {
  // The kill is saved to Replays without interrupting play; recording resumes.
  if (Replay->GetReplays().Num()<=ReplaysBefore) { if (Elapsed>25) Check(false,TEXT("Match kill saved to Replays")); return; }
  const FCrosshairReplayEntry& Saved=Replay->GetReplays()[0];
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO saved match kill '%s' map=%s hit=%.1fs"),*Saved.Label,*Saved.Map,Saved.HitSeconds);
  if (!Check(Saved.Map.Contains(TEXT("L_Nuketown")) && Saved.Label.Contains(TEXT(" m")) && !Saved.Label.Contains(TEXT(": ")),TEXT("Match kill is saved to Replays with a kill label"))) return;
  if (!Check(!Replay->IsPlayback() && Replay->IsMatchRecording() && Match && !Match->bMatchOver,TEXT("Saving a kill does not interrupt the match; recording continues"))) return;
  if (!Check(Match->GetStandings().Num()==8,TEXT("Scoreboard lists only the eight players while the replay recorder runs"))) return;
  Next(9); return;
 }
 case 9:
 {
  if (Elapsed<1) return;
  ACrosshairCharacter* Bot=LiveBot(-1); if (!Bot || !Me) return;
  Me->Kills=Match->ScoreLimit-1;
  Bot->Health->ApplyDamage(500,PC,nullptr,false,TEXT("Test"));
  if (!Check(Match->bMatchOver && Match->Result.Contains(Me->GetPlayerName()) && (!Pawn || !Pawn->CanAct()),TEXT("Reaching the FFA kill limit ends the match with the winner"))) return;
  Next(10); return;
 }
 case 10:
  if (Elapsed<1.5) return;
  Capture(TEXT("MatchResults.png"));
  if (Elapsed<2.5) return;
  Key(EKeys::Enter,IE_Pressed); Key(EKeys::Enter,IE_Released);
  Next(11); return;
 case 11:
  if (Match || !Pawn || !MapName.Contains(TEXT("L_Practice"))) { if (Elapsed>90) Check(false,TEXT("Leaving the match returns to practice")); return; }
  if (!Check(Pawn->CanAct() || PC->bHomeScreen,TEXT("Results screen leaves to the practice map"))) return;
  {
   // Watch the saved match kill from the practice side.
   const int32 Clip=Replay->GetReplays().IndexOfByPredicate([](const FCrosshairReplayEntry& Entry){ return !Entry.Label.IsEmpty(); });
   if (!Check(Clip!=INDEX_NONE,TEXT("Match kill is listed in Replays after the match"))) return;
   if (!Check(PC->GetMenuRows()[PC->GetFirstReplayRow()+Clip].Contains(Replay->GetReplays()[Clip].Label),TEXT("Replays tab shows the kill label"))) return;
   Replay->PlaySaved(Clip);
  }
  Next(40); return;
 case 40:
 {
  const auto* Viewed=Cast<ACrosshairCharacter>(PC ? PC->GetViewTarget() : nullptr);
  if (!Replay->IsPlayback() || !Viewed || !Viewed->bReplayFocus) { if (Elapsed>60) Check(false,TEXT("Match replay plays")); return; }
  if (Elapsed<1.5) return;
  if (!Check(!Viewed->UsesWorldPresentation() && Characters().Num()>1,TEXT("Match replay follows the player in first person with the bots present"))) return;
  Capture(TEXT("MatchReplay.png"));
  Next(41); return;
 }
 case 41:
  if (Replay->IsPlayback() || Match || !Pawn || !MapName.Contains(TEXT("L_Practice"))) { if (Elapsed>60) Check(false,TEXT("Match replay returns to practice")); return; }
  Options.Mode=ECrosshairMatchMode::TeamDeathmatch; Options.Map=ECrosshairMatchMap::Grid; Replay->SaveSettings();
  if (!Check(Session->StartOfflineMatch(),TEXT("Offline TDM travel to Grid starts"))) return;
  SpawnedAt.Reset(); Next(12); return;
 case 12:
 {
  if (!Match || !Mode || !MapName.Contains(TEXT("L_Grid")) || !Pawn) { if (Elapsed>180) Check(false,TEXT("Grid TDM loads")); return; }
  const TArray<ACrosshairCharacter*> All=Characters();
  if (All.Num()<12) { if (Elapsed>60) Check(false,TEXT("TDM spawns twelve players")); return; }
  int32 Alpha=0,Bravo=0,OnTeamStart=0;
  for (ACrosshairCharacter* Character:All)
  {
   const auto* State=Character->GetPlayerState<ACrosshairMatchPlayerState>(); if (!State) continue;
   Alpha+=State->Team==CrosshairMatchRules::Alpha ? 1 : 0; Bravo+=State->Team==CrosshairMatchRules::Bravo ? 1 : 0;
   const ACrosshairSpawnPoint* Point=Mode->GetLastSpawn(Character->GetController());
   OnTeamStart+=Point && uint8(Point->InitialTeam)==State->Team ? 1 : 0;
  }
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO Grid spawn points=%d team-start=%d"),Mode->GetSpawnPoints().Num(),OnTeamStart);
  if (Flag(TEXT("CrosshairMatchSpawnAudit")))
  {
   if (!NavReady() && Elapsed<120) return;
   ValidateSpawns(TEXT("Grid"));
   UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_AUDIT_DONE")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
  }
  if (!Check(Alpha==6 && Bravo==6 && Me && Me->Team==CrosshairMatchRules::Alpha,TEXT("TDM is 6v6 with the player on Alpha"))) return;
  if (!Check(OnTeamStart==12,TEXT("Each team starts in its own TDM start area"))) return;
  if (!Check(Match->IsTeamMode() && Match->ScoreLimit==75,TEXT("TDM score limit 75"))) return;
  Capture(TEXT("MatchTDM.png"));
  ACrosshairCharacter* Friend=LiveBot(CrosshairMatchRules::Alpha); ACrosshairCharacter* Foe=LiveBot(CrosshairMatchRules::Bravo);
  if (!Check(Friend && Friend->Health->ApplyDamage(50,PC,nullptr,false,TEXT("Test"))==0,TEXT("TDM friendly fire is off"))) return;
  if (!Check(Foe && Foe->Health->ApplyDamage(500,PC,nullptr,false,TEXT("Test"))>0 && Match->AlphaScore==1 && Match->BravoScore==0,TEXT("Enemy kill scores for the player's team"))) return;
  Next(20); return;
 }
 case 20:
 {
  // Difficulty duel: one Bravo bot with an AR against a passive, durable target 12 m away on open snow.
  if (Elapsed<1) return;
  ACrosshairCharacter* ShooterPawn=LiveBot(CrosshairMatchRules::Bravo); if (!ShooterPawn) return;
  auto* Shooter=Cast<ACrosshairBotController>(ShooterPawn->GetController());
  for (TActorIterator<ACrosshairBotController> It(World);It;++It)
   if (*It!=Shooter)
   {
    It->SetActorTickEnabled(false); It->StopMovement();
    if (auto* Frozen=Cast<ACrosshairCharacter>(It->GetPawn())) if (Frozen->Inventory->GetCurrent()) Frozen->Inventory->GetCurrent()->StopFire();
   }
  Pawn->SetActorLocation(FVector(-10000,9000,120),false,nullptr,ETeleportType::TeleportPhysics);
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Target=World->SpawnActor<ACrosshairCharacter>(Mode->DefaultPawnClass,FVector(14000,-3000,120),FRotator::ZeroRotator,Params);
  auto* TargetBrain=World->SpawnActor<AAIController>(AAIController::StaticClass(),Params);
  if (!Check(Target && TargetBrain,TEXT("Duel target spawns"))) return;
  TargetBrain->Possess(Target);
  Target->Health->MaxHealth=Target->Health->Health=100000.f;
  ShooterPawn->Inventory->ApplyLoadout(1,0,ECrosshairLethalType::Frag); // assault rifle for both runs
  DuelShooter=Shooter; DuelTarget=Target; DuelTargetController=TargetBrain;
  EasyFirst=99; EasyDamage=0; EasyShots=0;
  Next(21); return;
 }
 case 21: case 22: case 23: case 24:
 {
  auto* Shooter=DuelShooter.Get(); auto* Target=DuelTarget.Get();
  auto* ShooterPawn=Shooter ? Cast<ACrosshairCharacter>(Shooter->GetPawn()) : nullptr;
  if (!Shooter || !Target || !ShooterPawn || ShooterPawn->Health->bDead) { Check(false,TEXT("Duel participants stay alive")); return; }
  auto* Difficulty=GetGameInstance()->GetSubsystem<UCrosshairBotMatch>();
  if (Step==21 || Step==23)
  {
   // Start a 6 s run: shooter 12 m from the target, facing 60 degrees away so it must notice, turn and aim.
   Difficulty->SetDifficulty(Step==21 ? ECrosshairBotDifficulty::Easy : ECrosshairBotDifficulty::Veteran);
   ShooterPawn->SetActorLocation(FVector(14000,-500,120),false,nullptr,ETeleportType::TeleportPhysics);
   Shooter->SetControlRotation(FRotator(0,-30,0)); Shooter->StopMovement(); Shooter->Target=nullptr;
   if (ShooterPawn->Inventory->GetCurrent()) ShooterPawn->Inventory->GetCurrent()->ResetWeapon();
   // 12 m: inside the bots' all-round awareness, so roaming/facing cannot hide the target.
   Target->SetActorLocation(FVector(14000,-1700,120),false,nullptr,ETeleportType::TeleportPhysics);
   Target->Health->TotalDamageTaken=0; Target->Health->Health=Target->Health->MaxHealth;
   RunStart=World->GetTimeSeconds(); FirstHit=-1; RunShotsBefore=Shooter->ShotsFired;
   Next(Step+1); return;
  }
  if (FirstHit<0 && Target->Health->TotalDamageTaken>0) FirstHit=World->GetTimeSeconds()-RunStart;
  if (World->GetTimeSeconds()-RunStart<6.0) return;
  const ECrosshairBotDifficulty Expected=Step==22 ? ECrosshairBotDifficulty::Easy : ECrosshairBotDifficulty::Veteran;
  if (!Check(FMath::IsNearlyEqual(Shooter->Tuning.ReactionSeconds,UCrosshairBotMatch::TuningFor(Expected).ReactionSeconds) && FMath::IsNearlyEqual(Shooter->Tuning.AimErrorDegrees,UCrosshairBotMatch::TuningFor(Expected).AimErrorDegrees),
   TEXT("Changing difficulty updates an active bot's live tuning"))) return;
  const double First=FirstHit<0 ? 99.0 : FirstHit; const float Damage=Target->Health->TotalDamageTaken; const int32 Shots=Shooter->ShotsFired-RunShotsBefore;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO duel %s: first hit %.2fs, damage %.0f, shots %d"),*UCrosshairBotMatch::DifficultyName(Expected),First,Damage,Shots);
  {
   // Diagnostics: what the shooter is engaging and what its line of fire meets.
   const FVector Eye=ShooterPawn->GetPawnViewLocation();
   const FVector Chest=Target->GetMesh()->GetBoneLocation(TEXT("spine_03"));
   FHitResult Probe; FCollisionQueryParams ProbeParams(SCENE_QUERY_STAT(CrosshairDuelProbe),true,ShooterPawn);
   World->LineTraceSingleByChannel(Probe,Eye,Eye+Shooter->GetControlRotation().Vector()*5000,ECC_Visibility,ProbeParams);
   UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO duel aim: target=%s aimErrDeg=%.1f fireTraceHit=%s comp=%s bone=%s targetBody=%s collision=%d"),
    *GetNameSafe(Shooter->Target),FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Shooter->GetControlRotation().Vector(),(Chest-Eye).GetSafeNormal()),-1.f,1.f))),
    *GetNameSafe(Probe.GetActor()),*GetNameSafe(Probe.GetComponent()),*Probe.BoneName.ToString(),*GetNameSafe(Target->GetMesh()->GetSkeletalMeshAsset()),int32(Target->GetMesh()->GetCollisionEnabled()));
   const ACrosshairCharacter* Engaged=Shooter->Target;
   UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO duel actors: duelTarget=%s at %s; engaged at %s controller=%s dead=%d; shooter at %s; player=%s at %s"),
    *Target->GetName(),*Target->GetActorLocation().ToCompactString(),Engaged ? *Engaged->GetActorLocation().ToCompactString() : TEXT("-"),Engaged ? *GetNameSafe(Engaged->GetController()) : TEXT("-"),
    Engaged ? int32(Engaged->Health->bDead) : -1,*ShooterPawn->GetActorLocation().ToCompactString(),*GetNameSafe(Pawn),Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("-"));
  }
  if (Step==22) { EasyFirst=First; EasyDamage=Damage; EasyShots=Shots; Next(23); return; }
  if (!Check(Shots>0 && Damage>0,TEXT("Veteran bot shoots and hits the target"))) return;
  if (!Check(First<EasyFirst && Damage>EasyDamage,TEXT("Veteran bots react faster and land more damage than Easy bots"))) return;
  // Restore the match.
  for (TActorIterator<ACrosshairBotController> It(World);It;++It) It->SetActorTickEnabled(true);
  if (DuelTargetController.IsValid()) DuelTargetController->UnPossess();
  Target->Destroy(); if (DuelTargetController.IsValid()) DuelTargetController->Destroy();
  Difficulty->SetDifficulty(ECrosshairBotDifficulty::Regular);
  Next(13); return;
 }
 case 13:
  if (Elapsed<2 || (!NavReady() && Elapsed<90)) return;
  if (!ValidateSpawns(TEXT("Grid"))) return;
  Key(EKeys::Tab,IE_Pressed);
  if (!Check(PC->bScoreboardHeld,TEXT("Holding Tab shows the scoreboard"))) return;
  Capture(TEXT("MatchScoreboard.png"));
  Next(14); return;
 case 14:
  if (Elapsed<1) return;
  Key(EKeys::Tab,IE_Released);
  PC->ToggleMenu();
  if (!Check(!PC->bScoreboardHeld && PC->bMenuOpen && World->IsPaused(),TEXT("Offline match pauses behind the menu"))) return;
  PC->SetMenuTab(0);
  if (!Check(PC->GetVisibleMenuRows().Contains(PC->GetHomeRow()) && !PC->GetVisibleMenuRows().Contains(9) && !PC->GetVisibleMenuRows().Contains(PC->MapRow),TEXT("Match menu offers Leave match and hides practice tools"))) return;
  PC->ActivateMenuRow(PC->GetHomeRow());
  Next(15); return;
 case 15:
  if (Match || !Pawn || !MapName.Contains(TEXT("L_Practice"))) { if (Elapsed>90) Check(false,TEXT("Leave match returns to practice")); return; }
  if (!Check(!World->IsPaused(),TEXT("Leaving a paused match unpauses"))) return;
  Options.Mode=ECrosshairMatchMode::FreeForAll; Options.Map=ECrosshairMatchMap::Nuketown; Replay->SaveSettings();
  if (!Check(Session->HostLanMatch(),TEXT("LAN host session creation starts"))) return;
  Next(16); return;
 case 16:
  if (!Match || !Pawn || World->GetNetMode()!=NM_ListenServer) { if (Elapsed>120) Check(false,Session->LanStatus.IsEmpty() ? TEXT("LAN host opens a listen server") : *Session->LanStatus); return; }
  if (!Check(Session->HasLanSession(),TEXT("LAN host advertises a same-network session as a listen server"))) return;
  Session->FindLanMatches();
  Next(17); return;
 case 17:
  if (Session->bSearching && Elapsed<15) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_INFO LAN search: %s"),*Session->LanStatus);
  if (!Check(!Session->bSearching,TEXT("LAN search completes"))) return;
  Session->LeaveMatch(); Next(18); return;
 case 18:
  if (Match || !Pawn || !MapName.Contains(TEXT("L_Practice"))) { if (Elapsed>90) Check(false,TEXT("Leaving the LAN match returns to practice")); return; }
  if (!Check(!Session->HasLanSession(),TEXT("Leaving ends the LAN session"))) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_MATCH_OK")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
 default: return;
 }
}
