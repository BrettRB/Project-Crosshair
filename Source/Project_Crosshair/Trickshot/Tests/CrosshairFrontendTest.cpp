#include "CrosshairFrontendTest.h"
#include "../CrosshairStartup.h"
#include "UObject/GarbageCollection.h"
#include "../CrosshairCombatAppearance.h"
#include "../CrosshairSoldierAppearance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "../CrosshairGame.h"
#include "../CrosshairCharacter.h"
#include "../CrosshairWeapon.h"
#include "../CrosshairThrowable.h"
#include "../CrosshairPractice.h"
#include "../CrosshairData.h"
#include "../CrosshairDummy.h"
#include "ProceduralMeshComponent.h"
#include "../CrosshairMapLibrary.h"
#include "../CrosshairReplaySubsystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/PointLight.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
bool UCrosshairFrontendTest::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
 return false;
#else
 return FParse::Param(FCommandLine::Get(),TEXT("CrosshairSmoke")) && FParse::Param(FCommandLine::Get(),TEXT("CrosshairFrontendSmoke"));
#endif
}
void UCrosshairFrontendTest::Initialize(FSubsystemCollectionBase& Collection)
{ Super::Initialize(Collection); Collection.InitializeDependency<UCrosshairReplaySubsystem>(); Collection.InitializeDependency<UCrosshairMapLibrary>(); Started=StepStarted=FPlatformTime::Seconds(); }
bool UCrosshairFrontendTest::Check(bool Value,const TCHAR* Message)
{ UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_FRONTEND_%s step=%d %s"),Value ? TEXT("PASS") : TEXT("FAIL"),Step,Message); if (!Value) { bDone=true; FPlatformMisc::RequestExitWithStatus(false,1); } return Value; }
void UCrosshairFrontendTest::Next(int32 NewStep) { Step=NewStep; StepStarted=FPlatformTime::Seconds(); }
void UCrosshairFrontendTest::Tick(float Delta)
{
 auto* World=GetWorld(); if (!World || !World->IsGameWorld() || bDone) return;
 auto* Startup=GetGameInstance()->GetSubsystem<UCrosshairStartup>();
 if (!Startup->IsReady())
 {
  if (FPlatformTime::Seconds()-Started>900) { Check(false,TEXT("Startup preparation timeout")); return; }
  if (auto* LoadingPC=Cast<ACrosshairPlayerController>(World->GetFirstPlayerController()); LoadingPC && Startup->IsPreparing() && !bCheckedStartupGate)
  {
   auto* LoadingPawn=Cast<ACrosshairCharacter>(LoadingPC->GetPawn()); if (!LoadingPawn) return;
   LoadingPC->ShowHome(); LoadingPC->StartPlaying();
   if (!Check(LoadingPC->bHomeScreen && !LoadingPawn->CanAct(),TEXT("Startup blocks play and gameplay input"))) return;
   if (!Check(!GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>()->PlayMap(1),TEXT("Startup blocks map travel while preparing content"))) return;
   bCheckedStartupGate=true;
   if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairFrontendVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/StartupPreparation.png"),false,false);
  }
  return;
 }
 if (!bCheckedStartupReady)
 {
  if (!Check(bCheckedStartupGate && Startup->GetPreparationCount()==1 && Startup->GetCachedAssetCount()>0,TEXT("Startup finishes once with retained content"))) return;
  if (!Check(Startup->HasCachedAsset(TEXT("/Game/Crosshair/Maps/M_ImportedMap.M_ImportedMap")),TEXT("Imported-map base material is preloaded"))) return;
  bCheckedStartupReady=true; Started=StepStarted=FPlatformTime::Seconds();
 }
 if (FPlatformTime::Seconds()-Started>180) { Check(false,TEXT("Timeout")); return; }
 const double Elapsed=FPlatformTime::Seconds()-StepStarted;
 auto* EarlyReplay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 if (Step==20 && EarlyReplay->IsPlayback())
 { for(TActorIterator<ACrosshairDummy> It(World);It;++It) if(It->GetActorLocation().Equals(FVector(500,0,96),3) && It->SoldierAppearance->DesignIndex==4) bSawSoldierReplay=true;
   for(TActorIterator<ACrosshairImportedMap> It(World);It;++It) if (It->MapId==ImportedId && It->Mesh->GetNumSections()>0) bSawImportedReplay=true; return; }
 auto* PC=Cast<ACrosshairPlayerController>(World->GetFirstPlayerController()); auto* P=PC ? Cast<ACrosshairCharacter>(PC->GetPawn()) : nullptr;
 if (!P || !P->Inventory->GetCurrent()) return;
 auto* Replay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>(); auto& S=Replay->GetSettings(); auto* Library=GetGameInstance()->GetSubsystem<UCrosshairMapLibrary>();
 auto Key=[&](FKey K,EInputEvent Event){PC->InputKey(FInputKeyEventArgs::CreateSimulated(K,Event,Event==IE_Released ? 0.f : 1.f));};
 auto Capture=[&](const TCHAR* Name){ if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairFrontendVisual"))) FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots")/Name,false,false); };
#if WITH_EDITOR
 if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairFrontendVisual")) && GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) return;
#endif
 switch(Step)
 {
 case 0:
  if (Elapsed<2) return;
  PC->bFromHome=false; if (PC->bMenuOpen) PC->ToggleMenu();
  S.bContinuousPractice=true; Replay->BeginAttempt();
  if (!Check(S.Classes.Num()==5 && S.StickYawSpeed>=720 && !S.bSandboxTargets,TEXT("Five saved classes, faster default controller and sandbox Off"))) return;
  P->Placement->Toggle(); if (!Check(!P->Placement->bPlacing,TEXT("Target editing requires sandbox setting"))) return;
  PC->ShowHome(); if (!Check(PC->bHomeScreen && !P->CanAct(),TEXT("Home screen blocks gameplay input"))) return;
  Capture(TEXT("FrontendHome.png")); Next(16); return;
 case 16:
  if (Elapsed<.3) return;
  PC->ActivateMenuRow(4);
  if (!Check(PC->MenuTab==8 && !PC->bHomeScreen,TEXT("Home opens match setup before gameplay"))) return;
  S.BotMatch=FCrosshairBotMatchOptions(); // COD defaults: FFA, seven bots
  PC->AdjustMenuRow(PC->GetBotMatchRow(),1); PC->AdjustMenuRow(PC->GetBotMatchRow()+1,1);
  if (!Check(S.BotMatch.Difficulty==ECrosshairBotDifficulty::Hardened && S.BotMatch.BotCount==8 && FMath::IsNearlyEqual(GetGameInstance()->GetSubsystem<UCrosshairBotMatch>()->GetCurrentTuning().ReactionSeconds,.3f),TEXT("Bot difficulty updates saved options and live tuning"))) return;
  if (auto* Saved=Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairSmoke_v1"),0))) { if (!Check(Saved->Settings.BotMatch.Difficulty==S.BotMatch.Difficulty && Saved->Settings.BotMatch.BotCount==8,TEXT("Bot setup persists on disk"))) return; } else { Check(false,TEXT("Bot setup save missing"));return; }
  if (!Check(P->CombatAppearance->Gear.Num()==2 && P->GetFirstPersonMesh()->GetMaterial(0)->GetPathName().Contains(TEXT("M_CombatArms")),TEXT("Tactical arm material and animated elbow gear are configured"))) return;
  for (UStaticMeshComponent* Gear:P->CombatAppearance->Gear) if (!Check(Gear->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Gear->GetAttachParent()==P->GetFirstPersonMesh(),TEXT("Cosmetic gear follows arm bones without collision"))) return;
  Capture(TEXT("FrontendBotSetup.png")); Next(17);return;
 case 17:
  if (Elapsed<.3) return;
  PC->ShowHome(); Next(1);return;
 case 1:
 {
  if (Elapsed<.3) return;
  PC->ActivateMenuRow(1); if (!Check(PC->MenuTab==6 && !PC->bHomeScreen,TEXT("Home opens class creator"))) return;
  PC->EditingClass=2; auto& C=S.Classes[2]; C.Primary=2; C.Secondary=0; C.Lethal=ECrosshairLethalType::Tomahawk;
  PC->ActivateMenuRow(PC->GetClassRow()+4);
  if (!Check(S.ActiveClass==2 && P->Inventory->PrimaryIndex==2 && P->Inventory->SecondaryIndex==0 && P->Lethals->Selected==ECrosshairLethalType::Tomahawk,TEXT("Class equips primary, secondary and lethal"))) return;
  if (auto* Saved=Cast<UCrosshairSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CrosshairSmoke_v1"),0))) { if (!Check(Saved->Settings.ActiveClass==2 && Saved->Settings.Classes[2].Primary==2,TEXT("Classes persist on disk"))) return; } else { Check(false,TEXT("Class save missing")); return; }
  Capture(TEXT("FrontendClasses.png")); Next(2); return;
 }
 case 2:
  if (Elapsed<.3) return;
  PC->StartPlaying(); Key(EKeys::Two,IE_Pressed);
  if (!Check(P->Inventory->ActiveIndex==0,TEXT("Number 2 selects secondary"))) return;
  Key(EKeys::Gamepad_FaceButton_Top,IE_Pressed); Key(EKeys::Gamepad_FaceButton_Top,IE_Released); Next(3); return;
 case 3:
  if (Elapsed<.2) return;
  if (!Check(P->Inventory->ActiveIndex==2,TEXT("Y switches from secondary to primary"))) return;
  Key(EKeys::MouseScrollDown,IE_Pressed); if (!Check(P->Inventory->ActiveIndex==0,TEXT("Wheel switches only between class slots"))) return;
  Key(EKeys::One,IE_Pressed); if (!Check(P->Inventory->ActiveIndex==2,TEXT("Number 1 selects primary"))) return;
  PC->ToggleMenu(); PC->SetMenuTab(8); PC->AdjustMenuRow(PC->GetBotMatchRow(),1);
  if (!Check(S.BotMatch.Difficulty==ECrosshairBotDifficulty::Veteran,TEXT("Pause menu changes bot difficulty during gameplay"))) return;
  PC->ToggleMenu(); Capture(TEXT("CombatSMG.png")); Next(30);return;
 case 30:
  if (Elapsed<.3) return;
  P->Inventory->Equip(1); Next(31);return;
 case 31:
  if (Elapsed<.4) return;
  Capture(TEXT("CombatAR.png")); Next(32);return;
 case 32:
  if (Elapsed<.3) return;
  P->Inventory->Equip(0);Next(33);return;
 case 33:
  if (Elapsed<.4) return;
  Capture(TEXT("CombatSniper.png"));Next(34);return;
 case 34:
  if (Elapsed<.3) return;
  P->Inventory->Equip(2);
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY,IE_Axis,1.f)); Next(13); return;
 case 13:
  if (Elapsed<.2) return;
  Key(EKeys::Gamepad_LeftThumbstick,IE_Pressed); Key(EKeys::Gamepad_LeftThumbstick,IE_Released); Next(14); return;
 case 14:
  if (Elapsed<.2) return;
  if (!Check(P->GetCharacterMovement()->MaxWalkSpeed==P->SprintSpeed,TEXT("Controller click-and-release latches sprint while moving"))) return;
  PC->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Gamepad_LeftY,IE_Axis,0.f)); Next(15); return;
 case 15:
  if (Elapsed<.2) return;
  if (!Check(P->GetCharacterMovement()->MaxWalkSpeed==P->WalkSpeed,TEXT("Stopping stick movement ends latched sprint"))) return;
  PC->ToggleMenu(); PC->SetMenuTab(1); PC->ActivateMenuRow(PC->GetSandboxRow()); PC->ToggleMenu(); P->Placement->Toggle();
  if (!Check(S.bSandboxTargets && P->Placement->bPlacing,TEXT("Settings enables controller-compatible sandbox placement"))) return;
  P->Placement->Toggle();
  if (!Check(FMath::IsNearlyEqual(CrosshairRules::StickDelta(FVector2D(1,0),S,0,1).X,720.,.01),TEXT("Full stick supports two turns per second"))) return;
  PC->ToggleMenu(); PC->SetMenuTab(7); Capture(TEXT("FrontendMaps.png")); Next(4); return;
 case 4:
  if (Elapsed<.3) return;
  { FString Error; FCrosshairMapMesh Geometry;
   if (!Check(!UCrosshairMapLibrary::ParseObj(TEXT("v 0 0 0\nf 1 2 99\n"),1,Geometry,Error),TEXT("Malformed face rejected without partial geometry"))) return;
   TSet<FString> BeforeIds; for (const auto& Map:Library->Maps) BeforeIds.Add(Map.Id);
   const int32 Before=Library->MapCount();
   if (!Check(!Library->Import(FPaths::ProjectDir()/TEXT("ContentSource/UserMapsExample/invalid.json"),Error) && Library->MapCount()==Before,TEXT("Escaping package path rejected"))) return;
   if (!Check(Library->Import(FPaths::ProjectDir()/TEXT("ContentSource/UserMapsExample/map.json"),Error),TEXT("Local geometry package copied and registered"))) return;
   int32 Index=INDEX_NONE; for (int32 i=0;i<Library->Maps.Num();++i) if (!BeforeIds.Contains(Library->Maps[i].Id)) { Index=i; ImportedId=Library->Maps[i].Id; }
   if (!Check(Index>=0 && Library->Find(ImportedId),TEXT("Installed map survives library refresh"))) return;
   PC->StartPlaying(); Library->PlayMap(Index+UCrosshairMapLibrary::BuiltinMapCount); Next(5); return;
  }
 case 5:
  if (!World->GetOutermost()->GetName().Contains(TEXT("L_UserMap")) || Elapsed<1) return;
  { int32 Count=0; for(TActorIterator<ACrosshairImportedMap> It(World);It;++It) { ++Count; if (!Check(It->MapId==ImportedId,TEXT("Playable map rebuilds correct installed geometry"))) return;
     if (!Check(Cast<UMaterialInstanceDynamic>(It->Mesh->GetMaterial(0))!=nullptr,TEXT("Local texture atlas creates imported material instance"))) return; }
    if (!Check(Count==1 && P->GetCharacterMovement()->IsMovingOnGround() && P->GetActorLocation().Z>90 && P->GetActorLocation().Z<120,TEXT("Imported collision supports player at configured spawn"))) return;
    if (!Check(S.ActiveClass==2 && P->Inventory->PrimaryIndex==2,TEXT("Map travel preserves equipped class"))) return;
    Capture(TEXT("FrontendImportedMap.png")); Next(6); return;
  }
 case 6:
  if (Elapsed<.3) return;
  if (FParse::Param(FCommandLine::Get(),TEXT("CrosshairIndoorBefore"))) { Library->PlayMap(1); Next(7); return; }
  S.bContinuousPractice=false;
  { auto* ImportedTarget=World->SpawnActor<ACrosshairDummy>(P->TargetClass,FVector(500,0,96),FRotator(0,180,0));
  if (!Check(ImportedTarget!=nullptr,TEXT("Imported soldier target spawned"))) return;
  ImportedTarget->SoldierAppearance->SetDesign(4); }
  P->Attempt->SaveStart(); Next(18); return;
 case 18:
  if (Elapsed<.6 || !Replay->IsRecording()) return;
  P->Inventory->GetCurrent()->Definition->HipSpreadDegrees=0;
  P->Inventory->GetCurrent()->StartFire(); P->Inventory->GetCurrent()->StopFire();
  if (!Check(P->Attempt->bSucceeded,TEXT("Imported map target hit records successful attempt"))) return;
  Next(20); return;
 case 20:
  if (!bSawImportedReplay || !P->CanAct()) return;
  if (!Check(Library->ActiveMapId==ImportedId && P->Inventory->PrimaryIndex==2,TEXT("Replay reconstructs imported geometry and restores class/map"))) return;
  if (!Check(bSawSoldierReplay,TEXT("Replay keeps recorded soldier appearance"))) return;
  { bool Restored=false;for(TActorIterator<ACrosshairDummy> It(World);It;++It) if(It->GetActorLocation().Equals(FVector(500,0,96),3) && It->SoldierAppearance->DesignIndex==4) Restored=true;
    if(!Check(Restored,TEXT("Practice return preserves soldier design"))) return; }
  S.bContinuousPractice=true; Replay->BeginAttempt(); Library->PlayMap(1); Next(7); return;
 case 7:
  if (!World->GetOutermost()->GetName().Contains(TEXT("L_Nuketown")) || Elapsed<2) return;
  P->SetActorLocation(FVector(-2350,400,210),false,nullptr,ETeleportType::TeleportPhysics); P->GetCharacterMovement()->StopMovementImmediately(); P->GetCharacterMovement()->SetMovementMode(MOVE_Flying); PC->SetControlRotation(FRotator(0,0,0)); Next(8); return;
 case 8:
  if (Elapsed<2) return;
  Capture(FParse::Param(FCommandLine::Get(),TEXT("CrosshairIndoorBefore")) ? TEXT("IndoorBeforeYellow.png") : TEXT("IndoorYellow.png")); Next(9); return;
 case 9:
  if (Elapsed<.3) return;
  P->SetActorLocation(FVector(1900,900,200),false,nullptr,ETeleportType::TeleportPhysics); PC->SetControlRotation(FRotator(0,180,0)); Next(10); return;
 case 10:
  if (Elapsed<2) return;
  Capture(FParse::Param(FCommandLine::Get(),TEXT("CrosshairIndoorBefore")) ? TEXT("IndoorBeforeGreen.png") : TEXT("IndoorGreen.png")); Next(11); return;
 case 11:
  if (Elapsed<.3) return;
  if (!FParse::Param(FCommandLine::Get(),TEXT("CrosshairIndoorBefore"))) { int32 Lights=0; for(TActorIterator<APointLight> It(World);It;++It) if (It->GetLightComponent()->Intensity>100) ++Lights; if (!Check(Lights>=7,TEXT("Nuketown indoor lights are active at corrected intensities"))) return; }
  if (!Check(Library->PlayMap(2),TEXT("Grid built-in map can be selected"))) return;
  Next(70); return;
 case 70:
  if (!World->GetOutermost()->GetName().Contains(TEXT("L_Grid")) || Elapsed<2) return;
  if (!Check(P->GetCharacterMovement()->IsMovingOnGround() && P->GetActorLocation().Z>-1000,TEXT("Grid spawn has supported walkable collision"))) return;
  if (!Check(Library->ActiveMapId.IsEmpty() && S.ActiveClass==2 && Startup->IsReady(),TEXT("Grid travel preserves class and startup cache without imported-map identity"))) return;
  PC->ToggleMenu(); PC->SetMenuTab(7);
  if (!Check(PC->GetMenuRows()[PC->MapRow].Contains(TEXT("Grid")),TEXT("Maps menu identifies Grid after travel"))) return;
  Capture(TEXT("GridCredits.png")); Next(71); return;
 case 71:
  if (Elapsed<.3) return;
  PC->ToggleMenu(); PC->SetControlRotation(FRotator(0,0,0));
  P->Attempt->SaveStart(); GridStart=P->GetActorLocation(); Next(72); return;
 case 72:
  if (Elapsed<.35) { P->AddMovementInput(FVector::ForwardVector,1); return; }
  if (!Check(FVector::Dist2D(P->GetActorLocation(),GridStart)>20,TEXT("Player can walk on Grid"))) return;
  { auto* Weapon=P->Inventory->GetCurrent(); const int32 Ammo=Weapon->Ammo; Weapon->StartFire(); Weapon->StopFire();
    if (!Check(Weapon->Ammo==Ammo-1,TEXT("Weapons fire normally on Grid"))) return; }
  P->Attempt->ResetAttempt();
  if (!Check(FVector::Dist2D(P->GetActorLocation(),GridStart)<3,TEXT("Grid attempt reset restores saved position"))) return;
  PC->SetControlRotation(FRotator(-35,0,0)); Next(73); return;
 case 73:
  if (Elapsed<.3) return;
  P->Placement->Toggle(); P->Placement->UpdatePreview();
  if (!Check(P->Placement->bValidPlacement,TEXT("Grid ground supports sandbox target placement"))) return;
  { const int32 Before=P->Placement->GetLayout().Num(); P->Placement->Confirm();
    if (!Check(P->Placement->GetLayout().Num()==Before+1,TEXT("Grid accepts a soldier target without changing collision"))) return; }
  if (P->Placement->bPlacing) P->Placement->Toggle();
  PC->SetControlRotation(FRotator(0,0,0)); Next(74); return;
 case 74:
  if (Elapsed<1) return;
  Capture(TEXT("GridGameplay.png")); Next(75); return;
 case 75:
  if (Elapsed<.3) return;
  { const FVector Position(9000,-10000,9000);
    SoldierCamera=World->SpawnActor<ACameraActor>(Position,(FVector(0,-1000,200)-Position).Rotation());
    PC->SetViewTarget(SoldierCamera); }
  Next(76); return;
 case 76:
  if (Elapsed<2) return;
  Capture(TEXT("GridOverview.png")); Next(77); return;
 case 77:
  if (Elapsed<.3) return;
  PC->SetViewTarget(P); SoldierCamera->Destroy(); SoldierCamera=nullptr;
  if (!Check(Library->PlayMap(0),TEXT("Return from Grid to practice succeeds"))) return;
  Next(12); return;
 case 12:
  if (!World->GetOutermost()->GetName().Contains(TEXT("L_Practice")) || Elapsed<.5) return;
  if (!Check(Library->ActiveMapId.IsEmpty() && !PC->bHomeScreen && P->CanAct(),TEXT("Return to built-in practice clears imported selection without reopening home"))) return;
  CollectGarbage(RF_NoFlags);
  Startup->BeginPreparation();
  if (!Check(Startup->IsReady() && Startup->GetPreparationCount()==1 && Startup->HasCachedAsset(TEXT("/Game/Crosshair/Maps/M_ImportedMap.M_ImportedMap")),TEXT("Startup cache survives garbage collection and round-trip map travel without restarting"))) return;
  if (!Check(UCrosshairMapLibrary::BuiltinMapCount==3 && Library->MapName(2)==TEXT("Grid"),TEXT("Testing Map, Nuketown and Grid are built-in; imports begin at index three"))) return;
  {
   auto* Selector=NewObject<UCrosshairSoldierSelection>(World);int32 Last=INDEX_NONE;
   for(int32 Cycle=0;Cycle<4;++Cycle){TSet<int32> Seen;for(int32 i=0;i<6;++i){const int32 Design=Selector->Draw(6);if(!Check(Design!=Last,TEXT("Shuffled soldier deck avoids adjacent repeats"))) return;Seen.Add(Design);Last=Design;}if(!Check(Seen.Num()==6,TEXT("Every shuffled cycle includes six soldier designs"))) return;}
   for(int32 i=0;i<6;++i)
   {
    auto* Target=World->SpawnActor<ACrosshairDummy>(P->TargetClass,FVector(650,-375+i*150,96),FRotator(0,180,0));
    if(!Check(Target && Target->SoldierAppearance->Catalog && Target->SoldierAppearance->Catalog->Designs.Num()==6 && Target->SoldierAppearance->DesignIndex>=0 && Target->SoldierAppearance->DesignIndex<6,TEXT("New target automatically receives a valid soldier design"))) return;
    Target->SoldierAppearance->SetDesign(i);const auto* Appearance=Target->SoldierAppearance.Get();
    if(!Check(Target->Model->GetSkeletalMeshAsset()==Appearance->Catalog->BodyMesh && Target->Model->GetMaterial(0)==Appearance->Catalog->Designs[i].Uniform && Appearance->Gear.Num()==Appearance->Catalog->Designs[i].Equipment.Num(),TEXT("Soldier body, uniform and complete equipment are configured"))) return;
    for(UStaticMeshComponent* Gear:Appearance->Gear) if(!Check(Gear->GetCollisionEnabled()==ECollisionEnabled::NoCollision && Gear->GetAttachParent()==Target->Model,TEXT("Soldier equipment follows rig without altering shot collision"))) return;
    Target->ResetTarget();if(!Check(Appearance->DesignIndex==i,TEXT("Reset keeps assigned soldier design"))) return;
   }
   const auto Saved=P->Placement->CaptureTargets();P->Placement->RestoreTargets(Saved);
   for(const auto& At:Saved){bool Found=false;for(TActorIterator<ACrosshairDummy> It(World);It;++It) if(It->GetActorTransform().Equals(At.Transform,.01) && It->SoldierAppearance->DesignIndex==At.SoldierDesign) Found=true;if(!Check(Found,TEXT("Snapshot restore keeps each target transform and soldier design"))) return;}
  }
  P->Inventory->Equip(0);P->GetFirstPersonMesh()->SetHiddenInGame(true,true);P->Inventory->GetCurrent()->PresentationMesh->SetHiddenInGame(true);
  SoldierPhoto=0;SoldierCamera=World->SpawnActor<ACameraActor>(FVector(280,-395,145),FRotator(-7,3,0));PC->SetViewTarget(SoldierCamera);Next(55);return;
 case 55:
  if(Elapsed<1) return;
  {const TCHAR* Photos[]={TEXT("SoldierWoodland.png"),TEXT("SoldierDesert.png"),TEXT("SoldierUrban.png"),TEXT("SoldierArctic.png"),TEXT("SoldierNight.png"),TEXT("SoldierRecon.png")};Capture(Photos[SoldierPhoto]);}
  Next(56);return;
 case 56:
  if(Elapsed<.3) return;
  if(++SoldierPhoto<6){SoldierCamera->SetActorLocation(FVector(280,-395+SoldierPhoto*150,145));Next(55);return;}
  PC->SetViewTarget(P);SoldierCamera->Destroy();P->GetFirstPersonMesh()->SetHiddenInGame(false,true);P->Inventory->GetCurrent()->PresentationMesh->SetHiddenInGame(false);
  for(TActorIterator<ACrosshairDummy> It(World);It;++It) if(FMath::IsNearlyEqual(It->GetActorLocation().X,650.f,1.f)) It->Destroy();
  Next(41);return;
 case 41:
  if (Elapsed<.6) return;
  P->Inventory->Equip(2);
  if (!Check(P->Inventory->Weapons[0]->bStowing && P->Inventory->GetPresentationWeapon()==P->Inventory->Weapons[0],TEXT("Switch begins outgoing shoulder stow while hands follow old weapon"))) return;
  { auto* W=P->Inventory->GetCurrent();const int32 Ammo=W->Ammo;W->StartFire();W->StopFire();if(!Check(W->Ammo==Ammo,TEXT("Draw transition blocks firing"))) return; }
  Next(42);return;
 case 42:
  if (Elapsed<.09) return;
  if (!Check(P->Inventory->Weapons[0]->Mesh->GetRelativeLocation().Y>20,TEXT("Stowing gun and hands move toward the shoulder"))) return;
  Capture(TEXT("SwitchStow.png"));Next(43);return;
 case 43:
  if (Elapsed<.4) return;
  if (!Check(P->Inventory->Weapons[0]->bHolstered && P->Inventory->Weapons[0]->Mesh->GetAttachParent()==P->GetCapsuleComponent() && !P->Inventory->GetCurrent()->IsSwitching(),TEXT("Old gun is carried on back and new draw finishes"))) return;
  if (!Check(P->GetMesh()->GetUpVector().Z>.99f,TEXT("World character body is upright for back carry"))) return;
  { auto* Camera=World->SpawnActor<ACameraActor>(P->GetActorLocation()+FVector(-260,-260,100),(P->GetActorLocation()-(P->GetActorLocation()+FVector(-260,-260,100))).Rotation());PC->SetViewTarget(Camera);
    P->GetFirstPersonMesh()->SetHiddenInGame(true);P->Inventory->GetCurrent()->PresentationMesh->SetHiddenInGame(true); }
  Next(44);return;
 case 44:
  if (Elapsed<.3) return;
  Capture(TEXT("WeaponBackCarry.png"));Next(45);return;
 case 45:
  if (Elapsed<.3) return;
  PC->SetViewTarget(P);P->GetFirstPersonMesh()->SetHiddenInGame(false);P->Inventory->GetCurrent()->PresentationMesh->SetHiddenInGame(false);
  S.bContinuousPractice=true; Replay->BeginAttempt(); Next(50);return;
 case 50:
  if (Elapsed<1 || Replay->IsRecording()) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_FRONTEND_OK")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0);return;
 }
}
