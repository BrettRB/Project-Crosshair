#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrosshairBotMatch.generated.h"

UENUM(BlueprintType)
enum class ECrosshairBotDifficulty : uint8 { Easy, Regular, Hardened, Veteran };
UENUM(BlueprintType)
enum class ECrosshairMatchMode : uint8 { FreeForAll, TeamDeathmatch };
/** Matches run only on the COD maps; the Testing map and local imports are practice-only. */
UENUM(BlueprintType)
enum class ECrosshairMatchMap : uint8 { Nuketown, Grid, SpaceStation };

/** Shared tuning for future AI controllers. No difficulty grants wall vision. */
USTRUCT(BlueprintType)
struct FCrosshairBotTuning
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float ReactionSeconds=.55f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float AimErrorDegrees=4.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float TrackingDegreesPerSecond=100.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float DecisionSeconds=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float MovementSpeed=450.f;
};

USTRUCT(BlueprintType)
struct FCrosshairBotMatchOptions
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairBotDifficulty Difficulty=ECrosshairBotDifficulty::Regular;
 /** Free-for-all opponents; the COD default lobby is eight players. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 BotCount=7;
 /** Free-for-all kill limit. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 ScoreLimit=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TimeLimitMinutes=10;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairMatchMode Mode=ECrosshairMatchMode::FreeForAll;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) ECrosshairMatchMap Map=ECrosshairMatchMap::Nuketown;
 /** Players per team, including the local player; COD default 6v6. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TeamSize=6;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TeamScoreLimit=75;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCrosshairBotDifficultyChanged,ECrosshairBotDifficulty,Difficulty);

/** Persistent match configuration and difficulty presets shared by match bots. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairBotMatch : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static FCrosshairBotTuning TuningFor(ECrosshairBotDifficulty Difficulty);
 static FString DifficultyName(ECrosshairBotDifficulty Difficulty);
 static FString ModeName(ECrosshairMatchMode Mode);
 static FString MapName(ECrosshairMatchMap Map);
 static FName MapPackage(ECrosshairMatchMap Map);
 static void Validate(FCrosshairBotMatchOptions& Options);
 UFUNCTION(BlueprintCallable) void SetDifficulty(ECrosshairBotDifficulty Difficulty);
 UFUNCTION(BlueprintPure) FCrosshairBotTuning GetCurrentTuning() const;
 UPROPERTY(BlueprintAssignable) FCrosshairBotDifficultyChanged OnDifficultyChanged;
};
