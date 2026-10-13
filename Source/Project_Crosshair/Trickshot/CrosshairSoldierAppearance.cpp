#include "CrosshairSoldierAppearance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/DemoNetDriver.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
int32 UCrosshairSoldierSelection::Draw(int32 Count)
{
 if (Count<=0) return INDEX_NONE;
 if (Deck.IsEmpty() || DeckSize!=Count)
 {
  Deck.Reset(); DeckSize=Count;
  for(int32 i=0;i<Count;++i) Deck.Add(i);
  for(int32 i=Count-1;i>0;--i) Deck.Swap(i,FMath::RandRange(0,i));
  if(Count>1 && Deck.Last()==Last) Deck.Swap(Count-1,0);
 }
 Last=Deck.Pop(EAllowShrinking::No);return Last;
}
UCrosshairSoldierAppearance::UCrosshairSoldierAppearance()
{
 SetIsReplicatedByDefault(true);
 static ConstructorHelpers::FObjectFinder<UCrosshairSoldierCatalog> Asset(TEXT("/Game/Crosshair/Soldiers/DA_SoldierAppearances"));
 Catalog=Asset.Object;
}
void UCrosshairSoldierAppearance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UCrosshairSoldierAppearance,DesignIndex); }
FTransform UCrosshairSoldierAppearance::GetReferenceBoneTransform(USkeletalMeshComponent* Body,FName Bone)
{
 if(!Body || !Body->GetSkeletalMeshAsset()) return FTransform::Identity;
 const FReferenceSkeleton& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();int32 Index=Ref.FindBoneIndex(Bone);
 FTransform Result=FTransform::Identity;
 while(Index!=INDEX_NONE){Result=Result*Ref.GetRefBonePose()[Index];Index=Ref.GetParentIndex(Index);}
 return Result;
}
void UCrosshairSoldierAppearance::InitializeAppearance(USkeletalMeshComponent* Body,int32 RequestedDesign)
{
 BodyMesh=Body;if(!BodyMesh || !Catalog) return;
 if(Catalog->BodyMesh) BodyMesh->SetSkeletalMesh(Catalog->BodyMesh);
 const bool Playback=GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying();
 if(GetOwner()->HasAuthority() && !Playback)
 {
  if(Catalog->Designs.IsValidIndex(RequestedDesign)) DesignIndex=RequestedDesign;
  else if(!Catalog->Designs.IsValidIndex(DesignIndex)) DesignIndex=GetWorld()->GetSubsystem<UCrosshairSoldierSelection>()->Draw(Catalog->Designs.Num());
 }
 OnRep_Design();
}
void UCrosshairSoldierAppearance::SetDesign(int32 Index)
{
 if(!GetOwner()->HasAuthority() || !Catalog || !Catalog->Designs.IsValidIndex(Index)) return;
 DesignIndex=Index;OnRep_Design();GetOwner()->ForceNetUpdate();
}
FText UCrosshairSoldierAppearance::GetDesignName() const
{ return Catalog && Catalog->Designs.IsValidIndex(DesignIndex) ? Catalog->Designs[DesignIndex].Name : FText::GetEmpty(); }
void UCrosshairSoldierAppearance::OnRep_Design()
{
 if(!BodyMesh || !Catalog || Catalog->Designs.IsEmpty()) return;
 for(UStaticMeshComponent* Part:Gear) if(Part) Part->DestroyComponent();Gear.Reset();
 // Old replay files have no design field. Use a fixed fallback until replication arrives.
 const auto& Design=Catalog->Designs[Catalog->Designs.IsValidIndex(DesignIndex) ? DesignIndex : 0];
 for(int32 i=0;i<BodyMesh->GetNumMaterials();++i) BodyMesh->SetMaterial(i,Design.Uniform);
 for(const auto& Piece:Design.Equipment)
 {
  if(!Piece.Mesh || BodyMesh->GetBoneIndex(Piece.Bone)==INDEX_NONE) continue;
  auto* Part=NewObject<UStaticMeshComponent>(GetOwner());
  Part->SetupAttachment(BodyMesh,Piece.Bone);Part->SetStaticMesh(Piece.Mesh);
  Part->SetRelativeTransform(Piece.ReferenceTransform.GetRelativeTransform(GetReferenceBoneTransform(BodyMesh,Piece.Bone)));
  // Keep the mannequin's existing posed physics bodies as the only bullet hit geometry.
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);Part->SetGenerateOverlapEvents(false);
  Part->SetCanEverAffectNavigation(false);
  for(int32 i=0;i<Piece.Mesh->GetStaticMaterials().Num();++i)
  {
   const FName Slot=Piece.Mesh->GetStaticMaterials()[i].MaterialSlotName;
   if(Slot==TEXT("Paint")) Part->SetMaterial(i,Design.Fabric);
   else if(Slot==TEXT("Metal")) Part->SetMaterial(i,Design.Armor);
  }
  Part->RegisterComponent();Gear.Add(Part);
 }
}
