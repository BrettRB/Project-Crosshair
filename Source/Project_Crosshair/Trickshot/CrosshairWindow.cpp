#include "CrosshairWindow.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/DemoNetDriver.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACrosshairWindow::ACrosshairWindow()
{
    bReplicates = true; bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    Glass = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glass")); SetRootComponent(Glass);
    Glass->SetCollisionProfileName(TEXT("BlockAll"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Piece(TEXT("/Engine/BasicShapes/Cube"));
    for (int32 i=0;i<12;++i)
    {
        auto* Shard = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("GlassShard%d"),i));
        Shard->SetupAttachment(Glass); Shard->SetMobility(EComponentMobility::Movable);
        Shard->SetStaticMesh(Piece.Object); Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Shard->SetCastShadow(false); Shard->SetVisibility(false); Shards.Add(Shard);
    }
    ShardVelocity.SetNum(12);
}
void ACrosshairWindow::BeginPlay() { Super::BeginPlay(); OnRep_Broken(); }
void ACrosshairWindow::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ACrosshairWindow,bBroken);
}
float ACrosshairWindow::TakeDamage(float Amount,const FDamageEvent& Event,AController* EventInstigator,AActor* Causer)
{
    if (Amount<=0 || bBroken || !HasAuthority() || (GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying())) return 0;
    bBroken=true; OnRep_Broken(); ForceNetUpdate(); return Amount;
}
void ACrosshairWindow::ResetGlass() { RestoreBroken(false); }
void ACrosshairWindow::RestoreBroken(bool Broken)
{
    if (!HasAuthority() || (GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying())) return;
    bBroken=Broken; OnRep_Broken(); ForceNetUpdate();
}
bool ACrosshairWindow::GetOpening(FVector& Center,FVector& Normal,float& HalfWidth,float& Bottom,float& Top) const
{
    if (!Glass->GetStaticMesh()) return false;
    const FBoxSphereBounds Bounds=Glass->GetStaticMesh()->GetBounds();
    const FTransform Transform=Glass->GetComponentTransform();
    if (bConfiguredOpening)
    {
        Center=Transform.TransformPosition(OpeningCenter);
        Normal=Transform.TransformVectorNoScale(OpeningNormal).GetSafeNormal2D();
        const FVector Side=FVector::CrossProduct(FVector::UpVector,OpeningNormal);
        HalfWidth=Transform.TransformVector(Side*OpeningHalfWidth).Size();
        const float Height=FMath::Abs(Transform.GetScale3D().Z)*OpeningHalfHeight;
        Bottom=Center.Z-Height; Top=Center.Z+Height;
        return HalfWidth>0 && Height>0 && !Normal.IsNearlyZero();
    }
    const bool ThinX=Bounds.BoxExtent.X<Bounds.BoxExtent.Y;
    Normal=Transform.TransformVectorNoScale(ThinX ? FVector::ForwardVector : FVector::RightVector).GetSafeNormal2D();
    Center=Transform.TransformPosition(Bounds.Origin);
    const FVector Scale=Transform.GetScale3D().GetAbs();
    HalfWidth=(ThinX ? Bounds.BoxExtent.Y*Scale.Y : Bounds.BoxExtent.X*Scale.X);
    Bottom=Center.Z-Bounds.BoxExtent.Z*Scale.Z; Top=Center.Z+Bounds.BoxExtent.Z*Scale.Z;
    return HalfWidth>0 && !Normal.IsNearlyZero();
}
void ACrosshairWindow::OnRep_Broken()
{
    Glass->SetVisibility(!bBroken);
    Glass->SetCollisionEnabled(bBroken ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    ShardAge=bBroken ? 0 : 1;
    FVector Center,Normal; float Width,Bottom,Top;
    if (!GetOpening(Center,Normal,Width,Bottom,Top)) return;
    UMaterialInterface* Material=nullptr;
    for (int32 i=0;i<Glass->GetNumMaterials();++i)
        if (auto* M=Glass->GetMaterial(i); M && M->GetPathName().Contains(TEXT("glass"),ESearchCase::IgnoreCase)) { Material=M; break; }
    const FVector Right=FVector::CrossProduct(FVector::UpVector,Normal);
    for (int32 i=0;i<Shards.Num();++i)
    {
        auto* Shard=Shards[i].Get(); Shard->SetVisibility(bBroken); Shard->SetMaterial(0,Material);
        const float U=(i%4+.5f)/4.f*2-1, V=(i/4+.5f)/3.f;
        Shard->SetWorldLocation(Center+Right*(U*Width)+FVector(0,0,FMath::Lerp(Bottom,Top,V)-Center.Z));
        Shard->SetWorldRotation(Normal.Rotation());
        Shard->SetWorldScale3D(FVector(.004f,FMath::Min(Width*.003f,.12f),FMath::Min((Top-Bottom)*.0015f,.13f)));
        ShardVelocity[i]=Normal*(i%2 ? 90.f : -90.f)+Right*(U*80)+FVector(0,0,80+V*60);
    }
}
void ACrosshairWindow::Tick(float Delta)
{
    Super::Tick(Delta);
    if (ShardAge>=.65f) return;
    ShardAge+=Delta;
    for (int32 i=0;i<Shards.Num();++i)
    {
        ShardVelocity[i].Z-=980*Delta;
        Shards[i]->AddWorldOffset(ShardVelocity[i]*Delta);
        Shards[i]->AddLocalRotation(FRotator(120*Delta,70*Delta,90*Delta));
        if (ShardAge>=.65f) Shards[i]->SetVisibility(false);
    }
}
