#include "CrosshairStartup.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CrosshairData.h"
#include "CrosshairSoldierAppearance.h"
#include "ShaderPipelineCache.h"
#include "UObject/UObjectGlobals.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif
void UCrosshairStartup::BeginPreparation()
{
 if (bStarted || IsRunningDedicatedServer() || IsRunningCommandlet()) return;
 bStarted=true; ++PreparationCount; Started=FPlatformTime::Seconds();
 UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_STARTUP_BEGIN"));
}
void UCrosshairStartup::Fail(const FString& Message)
{
 Error=Message;
 UE_LOG(LogTemp,Error,TEXT("CROSSHAIR_STARTUP_FAIL %s"),*Error);
}
FString UCrosshairStartup::GetStatus() const
{
 if (!Error.IsEmpty()) return TEXT("Preparation failed: ")+Error+TEXT(". Please restart the game.");
 if (!Handle && !bCollected) return TEXT("Discovering game content...");
 if (!bCollected) return FString::Printf(TEXT("Loading maps and equipment... %d%%"),FMath::RoundToInt(Handle->GetProgress()*100));
 if (!bRequestedPSOs) return FString::Printf(TEXT("Preparing shaders and assets... %d remaining"),PendingWork);
 return FString::Printf(TEXT("Preparing graphics pipelines... %d remaining"),PendingWork);
}
bool UCrosshairStartup::HasCachedAsset(const FString& Path) const
{
 return CachedAssets.ContainsByPredicate([&](const UObject* Asset){ return Asset && Asset->GetPathName()==Path; });
}
void UCrosshairStartup::CollectLoadedContent()
{
 TArray<UObject*> Loaded; Handle->GetLoadedAssets(Loaded);
 for (const auto& Path:Paths) if (!Path.ResolveObject()) { Fail(TEXT("Unable to load ")+Path.ToString()); return; }
 for (UObject* Asset:Loaded)
 {
  if (auto* World=Cast<UWorld>(Asset)) PreviewWorlds.Add(World);
  else if (Asset) CachedAssets.Add(Asset);
 }
 bCollected=true;
 UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_STARTUP_LOADED assets=%d maps=%d"),CachedAssets.Num(),PreviewWorlds.Num());
}
void UCrosshairStartup::Tick(float DeltaTime)
{
 auto* World=GetWorld(); if (!World || !World->IsGameWorld()) return;
 if (FPlatformTime::Seconds()-Started>900) { Fail(TEXT("Content preparation timed out")); return; }
 if (!Handle)
 {
  auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
  if (Registry.IsLoadingAssets()) return;
  TArray<FAssetData> Assets; Registry.GetAssetsByPath(TEXT("/Game/Crosshair"),Assets,true);
  if (Assets.IsEmpty()) { Fail(TEXT("No Crosshair content found")); return; }
  TSet<FName> Packages; TArray<FName> Queue;
  for (const auto& Asset:Assets) if (!Packages.Contains(Asset.PackageName)) { Packages.Add(Asset.PackageName); Queue.Add(Asset.PackageName); }
  // Map dependencies include content outside /Crosshair (Nuketown, mannequin, engine materials).
  for (int32 i=0;i<Queue.Num();++i)
  {
   TArray<FName> Dependencies; Registry.GetDependencies(Queue[i],Dependencies,UE::AssetRegistry::EDependencyCategory::Package,UE::AssetRegistry::EDependencyQuery::Game);
   for (FName Package:Dependencies)
    if (!Package.ToString().StartsWith(TEXT("/Script/")) && !Packages.Contains(Package)) { Packages.Add(Package); Queue.Add(Package); }
  }
  for (FName Package:Queue)
  {
   TArray<FAssetData> PackageAssets; Registry.GetAssetsByPackageName(Package,PackageAssets);
   for (const auto& Asset:PackageAssets) if (!Asset.IsRedirector()) Paths.AddUnique(Asset.GetSoftObjectPath());
  }
  Handle=UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,FStreamableDelegate(),FStreamableManager::AsyncLoadHighPriority);
  if (!Handle) Fail(TEXT("Unable to queue game content"));
  return;
 }
 if (!Handle->HasLoadCompleted()) return;
 if (!bCollected) { CollectLoadedContent(); return; }
#if WITH_EDITOR
 FAssetCompilingManager::Get().ProcessAsyncTasks(true);
 if (GShaderCompilingManager) GShaderCompilingManager->ProcessAsyncResults(true,false);
 PendingWork=FAssetCompilingManager::Get().GetNumRemainingAssets();
 if (GShaderCompilingManager) PendingWork+=GShaderCompilingManager->GetNumRemainingJobs();
 if (PendingWork>0) { QuietSince=0; return; }
#endif
 if (!bRequestedPSOs)
 {
  // Request actual map component/material combinations without initializing worlds or BeginPlay.
  for (UObject* Object:PreviewWorlds)
  {
   auto* Map=CastChecked<UWorld>(Object);
   if (!Map->PersistentLevel) continue;
   for (AActor* Actor:Map->PersistentLevel->Actors) if (Actor)
   {
    TInlineComponentArray<UPrimitiveComponent*> Components; Actor->GetComponents(Components);
    for (auto* Component:Components) { PreviewComponents.Add(Component); Component->PrecachePSOs(); }
   }
  }
  // Runtime-selected skins need their own mesh/material combinations.
  auto StaticPreview=[&](UStaticMesh* Mesh,const TArray<TObjectPtr<UMaterialInterface>>& Materials)
  {
   if (!Mesh) return;
   auto* Component=NewObject<UStaticMeshComponent>(this); Component->SetStaticMesh(Mesh);
   for (int32 i=0;i<Materials.Num();++i) Component->SetMaterial(i,Materials[i]);
   PreviewComponents.Add(Component); Component->PrecachePSOs();
  };
  for (UObject* Asset:CachedAssets)
  {
   if (auto* Weapon=Cast<UCrosshairWeaponDefinition>(Asset))
    for (const auto& Skin:Weapon->Skins) StaticPreview(Weapon->PresentationMesh,Skin.Materials);
   if (auto* Catalog=Cast<UCrosshairSoldierCatalog>(Asset))
    for (const auto& Design:Catalog->Designs)
    {
     auto* Body=NewObject<USkeletalMeshComponent>(this); Body->SetSkeletalMesh(Catalog->BodyMesh);
     Body->SetMaterial(0,Design.Uniform); PreviewComponents.Add(Body); Body->PrecachePSOs();
     for (const auto& Gear:Design.Equipment) if (Gear.Mesh)
     {
      TArray<TObjectPtr<UMaterialInterface>> Materials;
      for (const auto& Slot:Gear.Mesh->GetStaticMaterials())
       Materials.Add(Slot.MaterialSlotName==TEXT("Paint") ? Design.Fabric : Slot.MaterialSlotName==TEXT("Metal") ? Design.Armor : Slot.MaterialInterface);
      StaticPreview(Gear.Mesh,Materials);
     }
    }
  }
  bRequestedPSOs=true; QuietSince=0; return;
 }
 PendingWork=FShaderPipelineCache::NumPrecompilesRemaining();
 for (const UPrimitiveComponent* Component:PreviewComponents) if (Component->IsPSOPrecaching()) ++PendingWork;
 if (PendingWork>0 || IsAsyncLoading()) { QuietSince=0; return; }
 // Allow render-thread submissions and PSO collection tasks to reach the counters.
 if (!QuietSince) QuietSince=FPlatformTime::Seconds();
 if (FPlatformTime::Seconds()-QuietSince<1) return;
 bReady=true;
 // Keep assets, never worlds/actors: OpenLevel must be free to destroy the previous world.
 PreviewComponents.Empty(); PreviewWorlds.Empty(); Handle->ReleaseHandle(); Handle.Reset();
 UE_LOG(LogTemp,Display,TEXT("CROSSHAIR_STARTUP_READY assets=%d seconds=%.2f"),CachedAssets.Num(),FPlatformTime::Seconds()-Started);
}
void UCrosshairStartup::Deinitialize()
{
 if (Handle) { Handle->CancelHandle(); Handle.Reset(); }
 PreviewComponents.Empty(); PreviewWorlds.Empty(); CachedAssets.Empty();
 Super::Deinitialize();
}

