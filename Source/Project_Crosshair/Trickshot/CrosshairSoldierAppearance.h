#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataAsset.h"
#include "Subsystems/WorldSubsystem.h"
#include "CrosshairSoldierAppearance.generated.h"
class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FCrosshairSoldierGear
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UStaticMesh> Mesh;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Bone;
 /** Placement in the body mesh reference-pose coordinates (centimetres). */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FTransform ReferenceTransform;
};
USTRUCT(BlueprintType)
struct FCrosshairSoldierDesign
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FName Id;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FText Name;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UMaterialInterface> Uniform;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UMaterialInterface> Fabric;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TObjectPtr<UMaterialInterface> Armor;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FCrosshairSoldierGear> Equipment;
};
/** Shared by practice targets and future AI soldier pawns; no combat tuning. */
UCLASS(BlueprintType)
class PROJECT_CROSSHAIR_API UCrosshairSoldierCatalog : public UDataAsset
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TObjectPtr<USkeletalMesh> BodyMesh;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) TArray<FCrosshairSoldierDesign> Designs;
};
/** Per-world shuffled deck: every design once per cycle, no repeat at cycle boundaries. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairSoldierSelection : public UWorldSubsystem
{
 GENERATED_BODY()
public:
 int32 Draw(int32 Count);
private:
 TArray<int32> Deck;
 int32 Last=INDEX_NONE,DeckSize=0;
};
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairSoldierAppearance : public UActorComponent
{
 GENERATED_BODY()
public:
 UCrosshairSoldierAppearance();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 /** Call after assigning the pawn's mesh. Authority chooses a design if none was specified. */
 UFUNCTION(BlueprintCallable) void InitializeAppearance(USkeletalMeshComponent* Body,int32 RequestedDesign=-1);
 UFUNCTION(BlueprintCallable) void SetDesign(int32 Index);
 UFUNCTION(BlueprintPure) FText GetDesignName() const;
 UFUNCTION(BlueprintPure) static FTransform GetReferenceBoneTransform(USkeletalMeshComponent* Body,FName Bone);
 UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Soldier") TObjectPtr<UCrosshairSoldierCatalog> Catalog;
 UPROPERTY(ReplicatedUsing=OnRep_Design,BlueprintReadOnly,Category="Soldier") int32 DesignIndex=INDEX_NONE;
 UPROPERTY(Transient,BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> Gear;
private:
 UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> BodyMesh;
 UFUNCTION() void OnRep_Design();
};
