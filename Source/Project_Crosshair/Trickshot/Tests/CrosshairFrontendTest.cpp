#include "CrosshairFrontendTest.h"
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
 if (FPlatformTime::Seconds()-Started>180) { Check(false,TEXT("Timeout")); return; }
 const double Elapsed=FPlatformTime::Seconds()-StepStarted;
 auto* EarlyReplay=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>();
 if (Step==20 && EarlyReplay->IsPlayback())
 { for(TActorIterator<ACrosshairImportedMap> It(World);It;++It) if (It->MapId==ImportedId && It->Mesh->GetNumSections()>0) bSawImportedReplay=true; return; }
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
  S.bContinuousPractice=true; Replay->BeginAttempt();
  if (!Check(S.Classes.Num()==5 && S.StickYawSpeed>=720 && !S.bSandboxTargets,TEXT("Five saved classes, faster default controller and sandbox Off"))) return;
  P->Placement->Toggle(); if (!Check(!P->Placement->bPlacing,TEXT("Target editing requires sandbox setting"))) return;
  PC->ShowHome(); if (!Check(PC->bHomeScreen && !P->CanAct(),TEXT("Home screen blocks gameplay input"))) return;
  Capture(TEXT("FrontendHome.png")); Next(1); return;
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
   PC->StartPlaying(); Library->PlayMap(Index+2); Next(5); return;
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
  World->SpawnActor<ACrosshairDummy>(P->TargetClass,FVector(500,0,96),FRotator(0,180,0));
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
  Library->PlayMap(0); Next(12); return;
 case 12:
  if (!World->GetOutermost()->GetName().Contains(TEXT("L_Practice")) || Elapsed<.5) return;
  if (!Check(Library->ActiveMapId.IsEmpty() && !PC->bHomeScreen && P->CanAct(),TEXT("Return to built-in practice clears imported selection without reopening home"))) return;
  UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_FRONTEND_OK")); bDone=true; FPlatformMisc::RequestExitWithStatus(false,0); return;
 }
}
