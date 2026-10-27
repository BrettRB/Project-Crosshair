#pragma once
#include "CoreMinimal.h"
#include "CrosshairGame.h"
#include "CrosshairBotMatch.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CrosshairMatch.generated.h"

class UCapsuleComponent;
class UArrowComponent;
class ACrosshairCharacter;

/** Pure match rules; kept free of world state so automation tests can cover them. */
namespace CrosshairMatchRules
{
 constexpr uint8 NoTeam=0, Alpha=1, Bravo=2;
 struct FSpawnContext
 {
  float NearestEnemy=1e6f;
  int32 VisibleEnemies=0;
  float NearestFriend=1e6f;
  float SecondsSinceUsed=1e6f;
  bool bTeamMode=false;
 };
 /** COD-style spawn safety: away from and out of sight of enemies, near teammates in TDM. */
 inline float ScoreSpawn(const FSpawnContext& C)
 {
  float Score=FMath::Min(C.NearestEnemy,4000.f)/40.f;
  if (C.NearestEnemy<1000.f) Score-=200.f;
  Score-=250.f*C.VisibleEnemies;
  if (C.bTeamMode && C.NearestFriend<2500.f) Score+=40.f;
  if (C.SecondsSinceUsed<5.f) Score-=120.f;
  return Score;
 }
 /** Friendly fire is off in TDM; self-damage is always allowed. */
 inline bool CanDamage(bool bTeamMode,uint8 InstigatorTeam,uint8 VictimTeam,bool bSelf)
 { return bSelf || !bTeamMode || InstigatorTeam==NoTeam || InstigatorTeam!=VictimTeam; }
 inline bool ReachedLimit(int32 Score,int32 Limit) { return Limit>0 && Score>=Limit; }
 /** COD health regeneration: nothing until the delay passes, then a steady refill. */
 inline float RegenerateHealth(float Health,float MaxHealth,float SecondsSinceDamage,float Delta,float Delay=4.f,float Rate=40.f)
 { return SecondsSinceDamage<Delay ? Health : FMath::Min(MaxHealth,Health+Rate*Delta); }
 /** Bots needed so each team has TeamSize players; humans are already assigned. */
 inline int32 BotsForTeam(int32 TeamSize,int32 HumansOnTeam) { return FMath::Max(0,TeamSize-HumansOnTeam); }
 /** Kill Confirmed: a kill by an enemy drops the victim's tag (suicides and falls drop none). */
 inline bool DropsTag(bool bKillConfirmed,uint8 KillerTeam,uint8 VictimTeam)
 { return bKillConfirmed && KillerTeam!=NoTeam && VictimTeam!=NoTeam && KillerTeam!=VictimTeam; }
 /** Collecting an enemy's tag confirms the kill (team +1); collecting a teammate's tag denies it. */
 inline bool ConfirmsTag(uint8 CollectorTeam,uint8 VictimTeam) { return CollectorTeam!=VictimTeam; }
 /** LAN lobby: each team holds at most half the human slots (rounded up). */
 inline int32 TeamCapacity(int32 MaxHumans) { return FMath::Max(1,(MaxHumans+1)/2); }
 inline bool CanJoinTeam(int32 HumansAlreadyOnTeam,int32 MaxHumans) { return HumansAlreadyOnTeam<TeamCapacity(MaxHumans); }
}

UENUM(BlueprintType)
enum class ECrosshairSpawnTeam : uint8 { None, Alpha, Bravo };

/**
 * Designer-placed match spawn. Not an APlayerStart, so practice spawning is unaffected.
 * InitialTeam marks the TDM start area; FFA/TDM flags control which modes may respawn here.
 */
UCLASS(Blueprintable)
class PROJECT_CROSSHAIR_API ACrosshairSpawnPoint : public AActor
{
 GENERATED_BODY()
public:
 ACrosshairSpawnPoint();
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCapsuleComponent> Capsule;
#if WITH_EDITORONLY_DATA
 UPROPERTY(VisibleAnywhere) TObjectPtr<UArrowComponent> Arrow;
#endif
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") bool bFreeForAll=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") bool bTeamDeathmatch=true;
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") ECrosshairSpawnTeam InitialTeam=ECrosshairSpawnTeam::None;
 /** Short area name shown in spawn diagnostics, e.g. "Yellow backyard". */
 UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Spawn") FString Area;
 double LastUsedAt=-1000;
};

/** Match health with COD-style delayed regeneration. Only matches apply damage to players. */
UCLASS(ClassGroup=(Crosshair),meta=(BlueprintSpawnableComponent))
class PROJECT_CROSSHAIR_API UCrosshairHealthComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 UCrosshairHealthComponent();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function) override;
 /** Returns damage applied; zero when blocked by team rules, death or match end. */
 float ApplyDamage(float Amount,AController* Instigator,AActor* Causer,bool bHeadshot,const FString& Weapon);
 /** Environmental death such as falling out of the map. */
 void Kill(const FString& Reason);
 UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(ClampMin="1")) float MaxHealth=100.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RegenDelay=4.f;
 UPROPERTY(EditAnywhere,BlueprintReadOnly) float RegenRate=40.f;
 UPROPERTY(Replicated,BlueprintReadOnly) float Health=100.f;
 UPROPERTY(ReplicatedUsing=OnRep_Dead,BlueprintReadOnly) bool bDead=false;
 UPROPERTY(Replicated,BlueprintReadOnly) FString KilledBy;
 double LastDamageAt=-1000;
 /** Server-side record of who last hurt this player and total damage taken (diagnostics/tests). */
 TWeakObjectPtr<AController> LastAttacker;
 float TotalDamageTaken=0;
private:
 UFUNCTION() void OnRep_Dead();
};

USTRUCT(BlueprintType)
struct FCrosshairKillEntry
{
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) FString Killer;
 UPROPERTY(BlueprintReadOnly) FString Victim;
 UPROPERTY(BlueprintReadOnly) FString Weapon;
 UPROPERTY(BlueprintReadOnly) uint8 KillerTeam=0;
 UPROPERTY(BlueprintReadOnly) uint8 VictimTeam=0;
 UPROPERTY(BlueprintReadOnly) bool bHeadshot=false;
 UPROPERTY(BlueprintReadOnly) float ServerTime=0;
};

UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairMatchPlayerState : public APlayerState
{
 GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 Kills=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 Deaths=0;
 UPROPERTY(Replicated,BlueprintReadOnly) uint8 Team=CrosshairMatchRules::NoTeam;
 /** Kill Confirmed: enemy tags collected and teammate tags denied. */
 UPROPERTY(Replicated,BlueprintReadOnly) int32 Confirms=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 Denies=0;
 /** The LAN host, who starts the match from the lobby. */
 UPROPERTY(Replicated,BlueprintReadOnly) bool bLobbyHost=false;
};

/**
 * Kill Confirmed dog tag dropped where a player was killed. The killer's team collects it to score;
 * the victim's team collects it to deny the point. The host decides pickups; clients only show it.
 */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairDogTag : public AActor
{
 GENERATED_BODY()
public:
 ACrosshairDogTag();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> Tag;
 UPROPERTY(Replicated,BlueprintReadOnly) uint8 VictimTeam=CrosshairMatchRules::NoTeam;
 UPROPERTY(Replicated,BlueprintReadOnly) FString VictimName;
 /** Horizontal pickup reach from a living soldier's capsule centre. */
 UPROPERTY(EditDefaultsOnly) float PickupRadius=100.f;
 /** The team the local viewer sees this tag for (255 before it is known); tests read it. */
 uint8 GetShownForTeam() const { return ShownForTeam; }
private:
 UPROPERTY(Transient) TObjectPtr<class UMaterialInstanceDynamic> Material;
 uint8 ShownForTeam=255;
};

/** Replicated scoreboard, clock and kill feed. Its presence marks a match world. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairMatchState : public AGameStateBase
{
 GENERATED_BODY()
public:
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 static ACrosshairMatchState* Get(const UObject* WorldContext);
 bool IsTeamMode() const { return UCrosshairBotMatch::IsTeamMode(Mode); }
 bool IsKillConfirmed() const { return Mode==ECrosshairMatchMode::KillConfirmed; }
 float GetRemainingSeconds() const;
 int32 GetTeamScore(uint8 Team) const { return Team==CrosshairMatchRules::Alpha ? AlphaScore : Team==CrosshairMatchRules::Bravo ? BravoScore : 0; }
 static FString TeamName(uint8 Team);
 /** Highest kills first; ties keep join order. */
 TArray<ACrosshairMatchPlayerState*> GetStandings() const;
 UPROPERTY(Replicated,BlueprintReadOnly) ECrosshairMatchMode Mode=ECrosshairMatchMode::FreeForAll;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 ScoreLimit=30;
 UPROPERTY(Replicated,BlueprintReadOnly) float MatchEndsAt=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 AlphaScore=0;
 UPROPERTY(Replicated,BlueprintReadOnly) int32 BravoScore=0;
 UPROPERTY(Replicated,BlueprintReadOnly) bool bMatchOver=false;
 UPROPERTY(Replicated,BlueprintReadOnly) FString Result;
 UPROPERTY(Replicated,BlueprintReadOnly) FString MapName;
 UPROPERTY(Replicated,BlueprintReadOnly) TArray<FCrosshairKillEntry> KillFeed;
 /** LAN matches wait here until the host starts: no pawns, bots or clock yet. */
 UPROPERTY(Replicated,BlueprintReadOnly) bool bInLobby=false;
 /** Human player limit, shown in the lobby. */
 UPROPERTY(Replicated,BlueprintReadOnly) int32 MaxHumans=1;
 /** Lobby summary of the host's setup, e.g. "Bots fill open slots  /  10 minutes". */
 UPROPERTY(Replicated,BlueprintReadOnly) FString LobbyInfo;
 /** Human players in the match (bots and replay spectators excluded). */
 int32 CountHumans(uint8 Team=255) const;
};

/** Offline/LAN FFA and TDM rules for the COD maps. The host is authoritative. */
UCLASS()
class PROJECT_CROSSHAIR_API ACrosshairMatchGameMode : public ACrosshairGameMode
{
 GENERATED_BODY()
public:
 ACrosshairMatchGameMode();
 virtual void InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage) override;
 virtual void InitGameState() override;
 virtual void StartPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 /** Teams are assigned here because login asks for a start spot before HandleStartingNewPlayer. */
 virtual FString InitNewPlayer(APlayerController* NewPlayerController,const FUniqueNetIdRepl& UniqueId,const FString& Options,const FString& Portal) override;
 virtual void RestartPlayerAtPlayerStart(AController* NewPlayer,AActor* StartSpot) override;
 virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
 virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }
 /** LAN host: refuses joins once the setup's human player limit is reached. */
 virtual void PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage) override;
 /** Nobody spawns while a LAN match waits in its lobby. */
 virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
 /** Marks the LAN host's player state (local players are only known after login). */
 virtual void PostLogin(APlayerController* NewPlayer) override;
 bool IsInLobby() const;
 /** Lobby team choice (TDM / Kill Confirmed). False when the team is full or teams are locked. */
 bool RequestTeam(APlayerController* Player,uint8 Team);
 /** The host leaves the lobby: bots join, everyone spawns and the clock starts. */
 bool StartFromLobby(APlayerController* Requester);
 /** Kill Confirmed pickup, decided on the host. */
 void CollectTag(ACrosshairDogTag* Tag,AController* Collector);
 /** Hosted from the LAN setup (travel option CrosshairLan) rather than the offline match setup. */
 bool IsLanMatch() const { return bLanMatch; }
 int32 GetMaxHumans() const { return MaxHumans; }
 static bool IsMatchMap(const FString& MapName);
 bool CanDamage(AController* Attacker,AController* Victim) const;
 void NotifyKilled(AController* Killer,AController* Victim,bool bHeadshot,const FString& Weapon);
 void EndMatch(const FString& Result);
 const FCrosshairBotMatchOptions& GetOptions() const { return MatchOptions; }
 uint8 TeamOf(const AController* Controller) const;
 const TArray<TObjectPtr<ACrosshairSpawnPoint>>& GetSpawnPoints() const { return SpawnPoints; }
 /** The spawn point most recently chosen for a controller (diagnostics and tests). */
 ACrosshairSpawnPoint* GetLastSpawn(const AController* Controller) const { return LastSpawn.FindRef(Controller).Get(); }
 /** Seconds between death and respawn. */
 UPROPERTY(EditDefaultsOnly) float RespawnDelay=3.f;
 /** Results screen time before returning to Home. */
 UPROPERTY(EditDefaultsOnly) float ResultsSeconds=20.f;
 bool bInvalidMap=false;
private:
 void AssignTeam(AController* Controller);
 void AddBot(uint8 Team,int32 Number);
 void Respawn(AController* Controller);
 void CollectSpawnPoints();
 /** Bots, clock and first spawns: at StartPlay offline, or when the host leaves the LAN lobby. */
 void BeginMatch();
 void DropTag(AController* Victim,const FVector& Where);
 FCrosshairBotMatchOptions MatchOptions;
 bool bLanMatch=false,bLobby=false;
 int32 MaxHumans=1;
 UPROPERTY() TArray<TObjectPtr<ACrosshairSpawnPoint>> SpawnPoints;
 TSet<TWeakObjectPtr<AController>> HasSpawned;
 TMap<TWeakObjectPtr<const AController>,TWeakObjectPtr<ACrosshairSpawnPoint>> LastSpawn;
};
