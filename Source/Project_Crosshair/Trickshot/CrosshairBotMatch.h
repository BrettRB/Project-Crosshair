#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CrosshairBotMatch.generated.h"

UENUM(BlueprintType)
enum class ECrosshairBotDifficulty : uint8 { Easy, Regular, Hardened, Veteran };

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
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 BotCount=5;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 ScoreLimit=30;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) int32 TimeLimitMinutes=10;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCrosshairBotDifficultyChanged,ECrosshairBotDifficulty,Difficulty);

/** Persistent FFA configuration foundation. Combat/spawns/navigation are the next stage. */
UCLASS()
class PROJECT_CROSSHAIR_API UCrosshairBotMatch : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure) static FCrosshairBotTuning TuningFor(ECrosshairBotDifficulty Difficulty);
 static FString DifficultyName(ECrosshairBotDifficulty Difficulty);
 static void Validate(FCrosshairBotMatchOptions& Options);
 UFUNCTION(BlueprintCallable) void SetDifficulty(ECrosshairBotDifficulty Difficulty);
 UFUNCTION(BlueprintPure) FCrosshairBotTuning GetCurrentTuning() const;
 UPROPERTY(BlueprintAssignable) FCrosshairBotDifficultyChanged OnDifficultyChanged;
};
