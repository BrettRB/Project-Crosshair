#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/Actor.h"
#include "CrosshairMapLibrary.generated.h"
class UProceduralMeshComponent;
class ACrosshairPlayerController;
struct FCrosshairMapMesh
{
 TArray<FVector> Vertices,Normals;
 TArray<FVector2D> UV;
 TArray<int32> Triangles;
};
struct FCrosshairLocalMap
{
 FString Id,Name,Folder,Mesh;
 float Scale=1;
 FVector Spawn=FVector(0,0,200);
 float Yaw=0;
 FString Texture;
};
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairMapLibrary : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 void Refresh();
 static constexpr int32 BuiltinMapCount=2;
 int32 MapCount() const { return Maps.Num()+BuiltinMapCount; }
 FString MapName(int32 Index) const;
 bool Import(const FString& File,FString& Error);
 bool PlayMap(int32 Index);
 void ShowImportDialog(ACrosshairPlayerController* Controller);
 static bool ParseObj(const FString& Text,float Scale,FCrosshairMapMesh& Out,FString& Error);
 FString RootDirectory() const;
 const FCrosshairLocalMap* Find(const FString& Id) const;
 FString ActiveMapId;
 bool bHomeVisited=false;
 TArray<FCrosshairLocalMap> Maps;
};
/** Rebuilds imported geometry from the immutable local copy during demo playback. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairImportedMap : public AActor
{
 GENERATED_BODY()
public:
 ACrosshairImportedMap();
 virtual void BeginPlay() override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UFUNCTION() void OnRep_MapId();
 UPROPERTY(ReplicatedUsing=OnRep_MapId) FString MapId;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Mesh;
};
