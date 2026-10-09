#include "CrosshairCombatAppearance.h"
#include "CrosshairCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
UCrosshairCombatAppearance::UCrosshairCombatAppearance()
{
 LeftPadTransform=FTransform(FRotator(0,0,0),FVector(0,0,-4));
 RightPadTransform=LeftPadTransform;
}
void UCrosshairCombatAppearance::BeginPlay()
{
 Super::BeginPlay(); auto* Character=Cast<ACrosshairCharacter>(GetOwner()); if (!Character || !ElbowPad) return;
 auto* Arms=Character->GetFirstPersonMesh();
 for (int32 i=0;i<2;++i)
 {
  auto* Pad=NewObject<UStaticMeshComponent>(Character);
  Pad->SetupAttachment(Arms,i==0 ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
  Pad->SetStaticMesh(ElbowPad); Pad->SetRelativeTransform(i==0 ? LeftPadTransform : RightPadTransform);
  Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision); Pad->SetCastShadow(false);
  Pad->RegisterComponent(); Gear.Add(Pad);
 }
}
