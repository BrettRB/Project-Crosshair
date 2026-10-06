#include "CrosshairThrowable.h"
#include "CrosshairCharacter.h"
#include "CrosshairDummy.h"
#include "CrosshairPractice.h"
#include "CrosshairTraversal.h"
#include "CrosshairWeapon.h"
#include "CrosshairWindow.h"
#include "CrosshairReplaySubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DemoNetDriver.h"
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"

bool UCrosshairLethalMovement::HandleDeflection(FHitResult& Hit,const FVector& OldVelocity,const uint32 NumBounces,float& SubTickTimeRemaining)
{
    if (const auto* Window=Cast<ACrosshairWindow>(Hit.GetActor()); Window && Window->bBroken)
    { bIsSliding=false; return true; }
    return Super::HandleDeflection(Hit,OldVelocity,NumBounces,SubTickTimeRemaining);
}

ACrosshairThrowable::ACrosshairThrowable()
{
    PrimaryActorTick.bCanEverTick=true; bReplicates=true; bAlwaysRelevant=true;
    SetReplicateMovement(true); SetNetUpdateFrequency(60);
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("Throwable collision")); SetRootComponent(Collision);
    Collision->InitSphereRadius(4); Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic,ECR_Block);
    Movement=CreateDefaultSubobject<UCrosshairLethalMovement>(TEXT("Flight"));
    Movement->SetUpdatedComponent(Collision); Movement->bAutoActivate=false;
    Movement->bShouldBounce=true; Movement->BounceVelocityStopSimulatingThreshold=1;
    Movement->bForceSubStepping=true; Movement->MaxSimulationTimeStep=1.f/120;
    Movement->bRotationFollowsVelocity=true;
    Movement->OnProjectileBounce.AddDynamic(this,&ACrosshairThrowable::OnBounce);
    AddTickPrerequisiteComponent(Movement);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Metal(TEXT("/Game/Crosshair/Weapons/Finishes/M_WeaponMetal"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Paint(TEXT("/Game/Crosshair/Weapons/Finishes/M_Woodland"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rubber(TEXT("/Game/Crosshair/Weapons/Finishes/M_WeaponRubber"));
    for (int32 i=0;i<8;++i)
    {
        auto* Part=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("LethalPart%d"),i));
        Part->SetupAttachment(Collision); Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetStaticMesh(i==0 || i==7 ? Sphere.Object : (i==1 || i==3 ? Cylinder.Object : Cube.Object));
        Part->SetMaterial(0,i==0 ? Paint.Object : (i==3 ? Rubber.Object : Metal.Object)); Parts.Add(Part);
    }
    Parts[0]->SetRelativeScale3D(FVector(.085f,.085f,.11f));
    Parts[1]->SetRelativeLocation(FVector(0,0,6)); Parts[1]->SetRelativeScale3D(FVector(.035f,.035f,.03f));
    Parts[2]->SetRelativeLocationAndRotation(FVector(2,0,4),FRotator(0,0,15)); Parts[2]->SetRelativeScale3D(FVector(.018f,.012f,.11f));
    Parts[3]->SetRelativeScale3D(FVector(.022f,.022f,.28f));
    Parts[4]->SetRelativeLocation(FVector(4,0,10)); Parts[4]->SetRelativeScale3D(FVector(.11f,.014f,.06f));
    Parts[5]->SetRelativeLocationAndRotation(FVector(8,0,7),FRotator(30,0,0)); Parts[5]->SetRelativeScale3D(FVector(.055f,.008f,.045f));
    Parts[6]->SetRelativeLocation(FVector(-2,0,10)); Parts[6]->SetRelativeScale3D(FVector(.035f,.035f,.04f));
}
bool ACrosshairThrowable::IsPlayback() const
{
    return GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying();
}
void ACrosshairThrowable::BeginPlay()
{
    Super::BeginPlay(); Collision->IgnoreActorWhenMoving(GetInstigator(),true);
    OnRep_State(); PreviousPosition=GetActorLocation();
}
void ACrosshairThrowable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACrosshairThrowable,Spec); DOREPLIFETIME(ACrosshairThrowable,bHeld);
    DOREPLIFETIME(ACrosshairThrowable,bResting); DOREPLIFETIME(ACrosshairThrowable,bDetonated); DOREPLIFETIME(ACrosshairThrowable,FlightAge);
}
void ACrosshairThrowable::OnRep_State()
{
    const bool Frag=Spec.Type==ECrosshairLethalType::Frag;
    for (int32 i=0;i<7;++i) Parts[i]->SetVisibility(!bDetonated && (Frag ? i<3 : i>=3));
    if (Spec.VisualMesh)
    {
        for (int32 i=0;i<7;++i) Parts[i]->SetVisibility(false);
        Parts[0]->SetStaticMesh(Spec.VisualMesh); Parts[0]->EmptyOverrideMaterials();
        Parts[0]->SetRelativeScale3D(FVector(1)); Parts[0]->SetVisibility(!bDetonated);
    }
    Parts[7]->SetVisibility(bDetonated);
    if (bDetonated)
    {
        auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Crosshair/Targets/M_Target"));
        auto* Tint=UMaterialInstanceDynamic::Create(Material,this);
        if (Tint) { Tint->SetVectorParameterValue(TEXT("Color"),FLinearColor(1,.24f,.015f)); Parts[7]->SetMaterial(0,Tint); }
        EffectAge=0;
    }
    if (IsPlayback() || !HasAuthority() || bHeld || bResting || bDetonated)
    {
        Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision); Movement->Deactivate();
    }
}
void ACrosshairThrowable::Release(FVector Velocity,float CookedSeconds)
{
    if (!HasAuthority() || IsPlayback()) return;
    bHeld=false; FuseLeft=FMath::Max(.01f,Spec.Fuse-CookedSeconds); FlightAge=0;
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Movement->SetUpdatedComponent(Collision); Movement->ProjectileGravityScale=Spec.GravityScale;
    Movement->Bounciness=Spec.Type==ECrosshairLethalType::Frag ? Spec.Bounciness : 0;
    Movement->Velocity=Velocity; Movement->Activate(true);
    PreviousPosition=GetActorLocation(); OnRep_State(); ForceNetUpdate();
}
void ACrosshairThrowable::OnBounce(const FHitResult& Hit,const FVector& Velocity)
{
    if (IsPlayback() || !HasAuthority()) return;
    if (auto* Window=Cast<ACrosshairWindow>(Hit.GetActor()))
    {
        Window->TakeDamage(1,FDamageEvent(),GetInstigatorController(),this);
        Movement->Velocity=Velocity; return;
    }
    if (Spec.Type==ECrosshairLethalType::Tomahawk) Impact(Hit);
}
void ACrosshairThrowable::Impact(const FHitResult& Hit)
{
    if (bHeld || bResting || bDetonated || IsPlayback() || !HasAuthority()) return;
    if (auto* Window=Cast<ACrosshairWindow>(Hit.GetActor()))
    { Window->TakeDamage(1,FDamageEvent(),GetInstigatorController(),this); return; }
    if (Spec.Type==ECrosshairLethalType::Frag)
    {
        if (Cast<ACrosshairDummy>(Hit.GetActor()))
        {
            SetActorLocation(Hit.ImpactPoint+Hit.ImpactNormal*5);
            Movement->Velocity=FMath::GetReflectionVector(Movement->Velocity,Hit.ImpactNormal)*Spec.Bounciness;
        }
        return;
    }
    if (auto* Target=Cast<ACrosshairDummy>(Hit.GetActor()))
    {
        if (Target->TakeDamage(Spec.Damage,FDamageEvent(),GetInstigatorController(),this)>0)
            if (auto* Player=Cast<ACrosshairCharacter>(GetInstigator())) Player->TargetHit(Target,Target->IsHeadHit(Hit));
    }
    bResting=true; Movement->StopMovementImmediately(); Movement->Deactivate();
    SetActorLocation(Hit.ImpactPoint); OnRep_State(); SetLifeSpan(8); ForceNetUpdate();
}
void ACrosshairThrowable::Detonate()
{
    if (bDetonated || IsPlayback() || !HasAuthority() || Spec.Type!=ECrosshairLethalType::Frag) return;
    bHeld=false; bDetonated=true; Movement->StopMovementImmediately(); Movement->Deactivate();
    const FVector Origin=GetActorLocation()+FVector(0,0,5);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(FragBlast),true,this); Params.AddIgnoredActor(GetInstigator());
    for (TActorIterator<ACrosshairWindow> It(GetWorld());It;++It)
    {
        const FVector Point=It->Glass->Bounds.GetBox().GetClosestPointTo(Origin);
        if (It->bBroken || FVector::Dist(Origin,Point)>Spec.BlastRadius) continue;
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit,Origin,Point,ECC_Visibility,Params) || Hit.GetActor()==*It)
            It->TakeDamage(1,FDamageEvent(),GetInstigatorController(),this);
    }
    for (TActorIterator<ACrosshairDummy> It(GetWorld());It;++It)
    {
        const FVector Point=It->Model->GetBoneLocation(TEXT("spine_03"));
        const float Distance=FVector::Dist(Origin,Point);
        if (Distance>Spec.BlastRadius) continue;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit,Origin,Point,ECC_Visibility,Params) && Hit.GetActor()!=*It) continue;
        const float Alpha=FMath::Clamp((Distance-Spec.InnerRadius)/FMath::Max(1.f,Spec.BlastRadius-Spec.InnerRadius),0.f,1.f);
        const float Damage=Spec.Damage*FMath::Lerp(1.f,.2f,Alpha);
        if (It->TakeDamage(Damage,FDamageEvent(),GetInstigatorController(),this)>0)
            if (auto* Player=Cast<ACrosshairCharacter>(GetInstigator())) Player->TargetHit(*It);
    }
    OnRep_State(); SetLifeSpan(.55f); ForceNetUpdate();
}
void ACrosshairThrowable::Tick(float Delta)
{
    Super::Tick(Delta);
    if (bDetonated)
    {
        EffectAge+=Delta; Parts[7]->SetRelativeScale3D(FVector(FMath::Lerp(.12f,1.8f,FMath::Clamp(EffectAge/.4f,0.f,1.f))));
        Parts[7]->SetVisibility(EffectAge<.4f); return;
    }
    if (!bHeld && !bResting && HasAuthority() && !IsPlayback())
    {
        FlightAge+=Delta;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LethalFlight),true,this); Params.AddIgnoredActor(GetInstigator());
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit,PreviousPosition,GetActorLocation(),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(3),Params)) Impact(Hit);
        PreviousPosition=GetActorLocation();
        if (FlightAge>12) { Destroy(); return; }
    }
    if (!bHeld && Spec.Type==ECrosshairLethalType::Frag && HasAuthority() && !IsPlayback())
    { FuseLeft-=Delta; if (FuseLeft<=0) Detonate(); }
    if (!bHeld && !bResting && Spec.Type==ECrosshairLethalType::Tomahawk)
        for (int32 i=3;i<7;++i)
        {
            // Spin around the handle's center without affecting collision or trajectory.
            const float Angle=FlightAge*900;
            if (Spec.VisualMesh) Parts[0]->SetRelativeRotation(FRotator(Angle,0,0));
            const FVector Bases[]={FVector(0,0,0),FVector(4,0,10),FVector(8,0,7),FVector(-2,0,10)};
            Parts[i]->SetRelativeLocation(FRotator(Angle,0,0).RotateVector(Bases[i-3]));
            Parts[i]->SetRelativeRotation(FRotator(Angle+(i==5 ? 30 : 0),0,0));
        }
}

UCrosshairLethalComponent::UCrosshairLethalComponent()
{
    PrimaryComponentTick.bCanEverTick=true; SetIsReplicatedByDefault(true);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrenadeMesh(TEXT("/Game/Crosshair/Weapons/Throwables/SM_Frag"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> AxeMesh(TEXT("/Game/Crosshair/Weapons/Throwables/SM_Tomahawk"));
    Frag.VisualMesh=GrenadeMesh.Object; Tomahawk.VisualMesh=AxeMesh.Object;
    Tomahawk.Type=ECrosshairLethalType::Tomahawk; Tomahawk.Speed=2200; Tomahawk.GravityScale=.5f; Tomahawk.Damage=250;
}
void UCrosshairLethalComponent::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Player=Cast<ACrosshairCharacter>(GetOwner()); Player && !Player->IsReplayPlayback())
    { Selected=Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().LethalType; Remaining=Capacity; }
}
void UCrosshairLethalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCrosshairLethalComponent,Remaining); DOREPLIFETIME(UCrosshairLethalComponent,Selected); DOREPLIFETIME(UCrosshairLethalComponent,HeldActor);
}
bool UCrosshairLethalComponent::IsBusy() const { return IsHolding() || (GetWorld() && GetWorld()->GetTimeSeconds()<ReadyAt); }
float UCrosshairLethalComponent::GetCookedSeconds() const { return IsHolding() ? GetWorld()->GetTimeSeconds()-PreparedAt : 0; }
FString UCrosshairLethalComponent::GetName() const { return Selected==ECrosshairLethalType::Frag ? TEXT("Frag grenade") : TEXT("Tomahawk / battle axe"); }
void UCrosshairLethalComponent::Prepare()
{
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (!Player || !Player->HasAuthority() || !Player->CanAct() || IsBusy() || Player->Placement->bPlacing || Player->Traversal->bMantling) return;
    const auto* Weapon=Player->Inventory->GetCurrent(); if (Weapon && Weapon->bReloading) return;
    if (Remaining<=0) { Player->Notify(TEXT("No lethals left. Reset to restock.")); return; }
    Player->StopActions();
    const FTransform Transform(Player->GetControlRotation(),Player->GetFirstPersonCameraComponent()->GetComponentLocation());
    HeldActor=GetWorld()->SpawnActorDeferred<ACrosshairThrowable>(ACrosshairThrowable::StaticClass(),Transform,Player,Player,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!HeldActor) return;
    HeldActor->Spec=Selected==ECrosshairLethalType::Frag ? Frag : Tomahawk; HeldActor->Spec.Type=Selected;
    HeldActor->FinishSpawning(Transform); PreparedAt=GetWorld()->GetTimeSeconds();
}
void UCrosshairLethalComponent::Throw()
{
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (!IsHolding() || !Player || !Player->CanAct()) { Cancel(); return; }
    const FVector Start=Player->GetFirstPersonCameraComponent()->GetComponentLocation();
    const FVector Direction=Player->GetControlRotation().Vector();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LethalRelease),true,Player); Params.AddIgnoredActor(HeldActor);
    FHitResult Hit; FVector Launch=Start+Direction*30;
    if (GetWorld()->SweepSingleByChannel(Hit,Start,Launch,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(4),Params))
        Launch=Hit.Location-Direction;
    HeldActor->SetActorLocationAndRotation(Launch,Direction.Rotation());
    HeldActor->Release(Direction*HeldActor->Spec.Speed+Player->GetVelocity()*.25f,GetCookedSeconds());
    HeldActor=nullptr; --Remaining; ReadyAt=GetWorld()->GetTimeSeconds()+.4;
}
void UCrosshairLethalComponent::Cycle()
{
    auto* Player=Cast<ACrosshairCharacter>(GetOwner());
    if (!Player || Player->IsReplayPlayback() || Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->IsFinishing() || IsBusy()) return;
    Selected=Selected==ECrosshairLethalType::Frag ? ECrosshairLethalType::Tomahawk : ECrosshairLethalType::Frag;
    auto* Replay=Player->GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>(); auto& S=Replay->GetSettings(); S.LethalType=Selected;
    if (S.Classes.IsValidIndex(S.ActiveClass)) S.Classes[S.ActiveClass].Lethal=Selected;
    Replay->SaveSettings();
}
void UCrosshairLethalComponent::Cancel() { if (IsValid(HeldActor)) HeldActor->Destroy(); HeldActor=nullptr; }
void UCrosshairLethalComponent::Reset()
{
    Cancel(); Remaining=Capacity; ReadyAt=0;
    for (TActorIterator<ACrosshairThrowable> It(GetWorld());It;++It) if (It->GetInstigator()==GetOwner()) It->Destroy();
}
void UCrosshairLethalComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,Type,Function);
    auto* Player=Cast<ACrosshairCharacter>(GetOwner()); if (!Player || Player->IsReplayPlayback() || !IsHolding()) return;
    if (!Player->CanAct()) { Cancel(); return; }
    auto* Camera=Player->GetFirstPersonCameraComponent();
    FVector Position=Camera->GetComponentTransform().TransformPosition(FVector(45,16,-12));
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LethalHeld),true,Player); Params.AddIgnoredActor(HeldActor);
    FHitResult Hit;
    if (GetWorld()->SweepSingleByChannel(Hit,Camera->GetComponentLocation(),Position,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(4),Params)) Position=Hit.Location;
    HeldActor->SetActorLocationAndRotation(Position,Camera->GetComponentRotation());
    if (Selected==ECrosshairLethalType::Frag && GetCookedSeconds()>=HeldActor->Spec.Fuse)
    {
        auto* Grenade=HeldActor.Get(); HeldActor=nullptr; --Remaining; ReadyAt=GetWorld()->GetTimeSeconds()+.4; Grenade->Detonate();
    }
}
