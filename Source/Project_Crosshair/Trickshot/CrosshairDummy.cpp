#include "CrosshairDummy.h"
#include "CrosshairData.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DemoNetDriver.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACrosshairDummy::ACrosshairDummy()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
	Collision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Target collision"));
	SetRootComponent(Collision);
	Collision->InitCapsuleSize(28.f, 90.f);
	Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.45f, 0.45f, 1.25f));
	Body->SetRelativeLocation(FVector(0, 0, -15));
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Collision);
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head->SetRelativeScale3D(FVector(0.42f));
	Head->SetRelativeLocation(FVector(0, 0, 66));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere"));
	Body->SetStaticMesh(Cylinder.Object);
	Head->SetStaticMesh(Sphere.Object);
	Body->SetVisibility(false); Head->SetVisibility(false);
	Model = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Humanoid target"));
	Model->SetupAttachment(Collision);
	Model->SetRelativeLocationAndRotation(FVector(0,0,-90), FRotator(0,-90,0));
	// Keep shot geometry out of Unreal's pre-BeginPlay spawn encroachment
    // test. The physical capsule owns placement clearance; enable posed hit
    // bodies once the idle animation has initialized.
    Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Model->SetCollisionObjectType(ECC_WorldDynamic);
	Model->SetCollisionResponseToAllChannels(ECR_Ignore);
	Model->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	Model->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Humanoid(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"));
	Model->SetSkeletalMesh(Humanoid.Object); TargetIdle = Idle.Object;
}

void ACrosshairDummy::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority()) Health = MaxHealth;
	Body->SetVisibility(false); Head->SetVisibility(false);
	Collision->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
	if (TargetIdle) Model->PlayAnimation(TargetIdle,true);
	Model->TickAnimation(0,false); Model->RefreshBoneTransforms();
    Model->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (TargetMaterial) { Body->SetMaterial(0, TargetMaterial); Head->SetMaterial(0, TargetMaterial); }
	OnRep_Hit();
}
void ACrosshairDummy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACrosshairDummy, bHit);
	DOREPLIFETIME(ACrosshairDummy, Health);
}
float ACrosshairDummy::TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
	if (!HasAuthority() || bHit || Amount <= 0 || (GetWorld()->GetDemoNetDriver() && GetWorld()->GetDemoNetDriver()->IsPlaying())) return 0;
	Health = CrosshairRules::RemainingHealth(Health, Amount);
	bHit = Health <= 0.f;
	OnRep_Hit();
	ForceNetUpdate();
	return Amount;
}
void ACrosshairDummy::ResetTarget() { Health = MaxHealth; bHit = false; OnRep_Hit(); ForceNetUpdate(); }
bool ACrosshairDummy::IsHeadHit(const FHitResult& Hit) const
{
	return Hit.GetComponent() == Model && Hit.BoneName == TEXT("head");
}
bool ACrosshairDummy::IsHeadImpact(const FVector& Impact) const
{
	// Legacy location-only callers use the actual posed head; weapon shots use the physics bone.
	const FVector LocalImpact = Collision->GetComponentTransform().InverseTransformPosition(Impact);
	const FVector HeadCenter = Collision->GetComponentTransform().InverseTransformPosition(Model->GetBoneLocation(TEXT("head")));
	const float HeadRadius = 13.f;
	return FVector::DistSquared(LocalImpact,HeadCenter) <= FMath::Square(HeadRadius);
}
void ACrosshairDummy::OnRep_Hit()
{
	if (TargetMaterial)
	{
		auto* Overlay = UMaterialInstanceDynamic::Create(TargetMaterial,this);
		Overlay->SetVectorParameterValue(TEXT("Color"),FLinearColor(1.f,.06f,.025f));
		Model->SetOverlayMaterial(bHit ? Overlay : nullptr);
	}
	const FLinearColor Color = bHit ? FLinearColor(1.f, 0.15f, 0.04f) : FLinearColor(0.05f, 0.7f, 0.85f);
	for (UStaticMeshComponent* Part : {Body.Get(), Head.Get()})
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(0)) Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
}
