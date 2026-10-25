#include "CrosshairMatch.h"
#include "CrosshairBot.h"
#include "CrosshairCharacter.h"
#include "CrosshairData.h"
#include "CrosshairDummy.h"
#include "CrosshairMatchSession.h"
#include "CrosshairReplaySubsystem.h"
#include "Algo/StableSort.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACrosshairSpawnPoint::ACrosshairSpawnPoint()
{
 Capsule=CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn capsule"));
 Capsule->InitCapsuleSize(34.f,96.f);
 Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Capsule->SetHiddenInGame(true);
 Capsule->ShapeColor=FColor(80,220,255);
 SetRootComponent(Capsule);
#if WITH_EDITORONLY_DATA
 Arrow=CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Facing"));
 if (Arrow) { Arrow->SetupAttachment(Capsule); Arrow->ArrowColor=FColor(80,220,255); }
#endif
 SetCanBeDamaged(false);
}

UCrosshairHealthComponent::UCrosshairHealthComponent()
{
 SetIsReplicatedByDefault(true);
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickInterval=.1f;
}
void UCrosshairHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(UCrosshairHealthComponent,Health); DOREPLIFETIME(UCrosshairHealthComponent,bDead); DOREPLIFETIME(UCrosshairHealthComponent,KilledBy);
}
void UCrosshairHealthComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta,Type,Function);
 if (!GetOwner()->HasAuthority() || bDead || Health>=MaxHealth) return;
 Health=CrosshairMatchRules::RegenerateHealth(Health,MaxHealth,GetWorld()->GetTimeSeconds()-LastDamageAt,Delta,RegenDelay,RegenRate);
}
float UCrosshairHealthComponent::ApplyDamage(float Amount,AController* Instigator,AActor* Causer,bool bHeadshot,const FString& Weapon)
{
 auto* Owner=Cast<ACrosshairCharacter>(GetOwner());
 auto* Mode=GetWorld()->GetAuthGameMode<ACrosshairMatchGameMode>();
 if (!Owner || !Mode || !Owner->HasAuthority() || bDead || Amount<=0) return 0;
 AController* Victim=Owner->GetController();
 if (!Mode->CanDamage(Instigator,Victim)) return 0;
 TotalDamageTaken+=FMath::Min(Amount,Health);
 Health=CrosshairRules::RemainingHealth(Health,Amount);
 LastDamageAt=GetWorld()->GetTimeSeconds();
 LastAttacker=Instigator;
 if (Health<=0)
 {
  bDead=true;
  KilledBy=Instigator && Instigator->PlayerState ? Instigator->PlayerState->GetPlayerName() : FString();
  OnRep_Dead();
  Mode->NotifyKilled(Instigator,Victim,bHeadshot,Weapon);
 }
 Owner->ForceNetUpdate();
 return Amount;
}
void UCrosshairHealthComponent::Kill(const FString& Reason)
{
 auto* Owner=Cast<ACrosshairCharacter>(GetOwner());
 auto* Mode=GetWorld()->GetAuthGameMode<ACrosshairMatchGameMode>();
 if (!Owner || !Mode || bDead) return;
 Health=0; bDead=true; KilledBy=Reason;
 OnRep_Dead();
 Mode->NotifyKilled(nullptr,Owner->GetController(),false,Reason);
 Owner->ForceNetUpdate();
}
void UCrosshairHealthComponent::OnRep_Dead()
{ if (bDead) if (auto* Owner=Cast<ACrosshairCharacter>(GetOwner())) Owner->HandleDeath(); }

void ACrosshairMatchPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(ACrosshairMatchPlayerState,Kills); DOREPLIFETIME(ACrosshairMatchPlayerState,Deaths); DOREPLIFETIME(ACrosshairMatchPlayerState,Team);
}

void ACrosshairMatchState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(ACrosshairMatchState,Mode); DOREPLIFETIME(ACrosshairMatchState,ScoreLimit); DOREPLIFETIME(ACrosshairMatchState,MatchEndsAt);
 DOREPLIFETIME(ACrosshairMatchState,AlphaScore); DOREPLIFETIME(ACrosshairMatchState,BravoScore); DOREPLIFETIME(ACrosshairMatchState,bMatchOver);
 DOREPLIFETIME(ACrosshairMatchState,Result); DOREPLIFETIME(ACrosshairMatchState,MapName); DOREPLIFETIME(ACrosshairMatchState,KillFeed);
}
ACrosshairMatchState* ACrosshairMatchState::Get(const UObject* WorldContext)
{
 const UWorld* World=GEngine ? GEngine->GetWorldFromContextObject(WorldContext,EGetWorldErrorMode::ReturnNull) : nullptr;
 return World ? Cast<ACrosshairMatchState>(World->GetGameState()) : nullptr;
}
float ACrosshairMatchState::GetRemainingSeconds() const
{ return MatchEndsAt<=0 ? 0.f : FMath::Max(0.f,float(MatchEndsAt-GetServerWorldTimeSeconds())); }
FString ACrosshairMatchState::TeamName(uint8 Team)
{ return Team==CrosshairMatchRules::Alpha ? TEXT("Alpha") : Team==CrosshairMatchRules::Bravo ? TEXT("Bravo") : TEXT(""); }
TArray<ACrosshairMatchPlayerState*> ACrosshairMatchState::GetStandings() const
{
 TArray<ACrosshairMatchPlayerState*> Standings;
 // Replay recording adds a spectator player state; it never appears in standings.
 for (APlayerState* Player:PlayerArray) if (auto* Match=Cast<ACrosshairMatchPlayerState>(Player); Match && !Match->IsOnlyASpectator()) Standings.Add(Match);
 Algo::StableSortBy(Standings,[](const ACrosshairMatchPlayerState* P){ return TPair<int32,int32>(-P->Kills,P->Deaths); });
 return Standings;
}

ACrosshairMatchGameMode::ACrosshairMatchGameMode()
{
 PrimaryActorTick.bCanEverTick=true;
 GameStateClass=ACrosshairMatchState::StaticClass();
 PlayerStateClass=ACrosshairMatchPlayerState::StaticClass();
 // Same configured pawn as practice: loadout, inputs, arms and body assets live on the Blueprint.
 static ConstructorHelpers::FClassFinder<ACrosshairCharacter> PracticePawn(TEXT("/Game/Crosshair/Player/BP_PracticeCharacter"));
 if (PracticePawn.Class) DefaultPawnClass=PracticePawn.Class;
}
bool ACrosshairMatchGameMode::IsMatchMap(const FString& MapName)
{ return MapName.Contains(TEXT("L_Nuketown")) || MapName.Contains(TEXT("L_Grid")) || MapName.Contains(TEXT("L_SpaceStation")); }
void ACrosshairMatchGameMode::InitGame(const FString& MapName,const FString& Options,FString& ErrorMessage)
{
 Super::InitGame(MapName,Options,ErrorMessage);
 bInvalidMap=!IsMatchMap(MapName);
 const FCrosshairSettings& Settings=GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings();
 bLanMatch=UGameplayStatics::HasOption(Options,TEXT("CrosshairLan"));
 MatchOptions=bLanMatch ? Settings.Lan.Match : Settings.BotMatch;
 MaxHumans=bLanMatch ? FMath::Clamp(Settings.Lan.MaxPlayers,2,12) : 1;
 if (!bLanMatch) MatchOptions.bBots=true;   // offline matches are always against bots
 const FString Mode=UGameplayStatics::ParseOption(Options,TEXT("CrosshairMode"));
 if (Mode==TEXT("TDM")) MatchOptions.Mode=ECrosshairMatchMode::TeamDeathmatch;
 else if (Mode==TEXT("FFA")) MatchOptions.Mode=ECrosshairMatchMode::FreeForAll;
 UCrosshairBotMatch::Validate(MatchOptions);
}
void ACrosshairMatchGameMode::InitGameState()
{
 Super::InitGameState();
 auto* State=GetGameState<ACrosshairMatchState>(); if (!State) return;
 State->Mode=MatchOptions.Mode;
 State->ScoreLimit=MatchOptions.Mode==ECrosshairMatchMode::TeamDeathmatch ? MatchOptions.TeamScoreLimit : MatchOptions.ScoreLimit;
 State->MapName=GetWorld()->GetMapName().Contains(TEXT("L_Grid")) ? TEXT("Grid") : GetWorld()->GetMapName().Contains(TEXT("L_SpaceStation")) ? TEXT("Space Station") : TEXT("Nuketown");
}
void ACrosshairMatchGameMode::CollectSpawnPoints()
{
 SpawnPoints.Reset();
 for (TActorIterator<ACrosshairSpawnPoint> It(GetWorld());It;++It) SpawnPoints.Add(*It);
}
void ACrosshairMatchGameMode::StartPlay()
{
 Super::StartPlay();
 auto* State=GetGameState<ACrosshairMatchState>();
 CollectSpawnPoints();
 if (bInvalidMap || SpawnPoints.IsEmpty())
 {
  UE_LOG(LogTemp,Error,TEXT("Crosshair match unavailable on %s: matches require Nuketown, Grid or Space Station with match spawn points."),*GetWorld()->GetMapName());
  if (State) { State->bMatchOver=true; State->Result=TEXT("Matches are only available on Nuketown, Grid and Space Station."); }
  GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this](){ GetGameInstance()->GetSubsystem<UCrosshairMatchSession>()->LeaveMatch(); }));
  return;
 }
 // Practice targets are not match combatants.
 for (TActorIterator<ACrosshairDummy> It(GetWorld());It;++It) It->Destroy();
 // Bot navigation: regenerate the map's navmesh asynchronously rather than trusting saved editor data.
 if (UNavigationSystemV1* Navigation=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
  for (TActorIterator<ANavMeshBoundsVolume> It(GetWorld());It;++It) Navigation->AddDirtyArea(It->GetComponentsBoundingBox(true),ENavigationDirtyFlag::All);
 if (State) State->MatchEndsAt=State->GetServerWorldTimeSeconds()+MatchOptions.TimeLimitMinutes*60.f;
 if (!MatchOptions.bBots) {}   // LAN host turned bots off: humans only
 else if (MatchOptions.Mode==ECrosshairMatchMode::TeamDeathmatch)
 {
  int32 Humans[3]={0,0,0};
  for (auto It=GetWorld()->GetPlayerControllerIterator();It;++It) if (It->IsValid()) ++Humans[FMath::Min<uint8>(TeamOf(It->Get()),2)];
  int32 Number=0;
  for (uint8 Team:{CrosshairMatchRules::Alpha,CrosshairMatchRules::Bravo})
   for (int32 i=0;i<CrosshairMatchRules::BotsForTeam(MatchOptions.TeamSize,Humans[Team]);++i) AddBot(Team,++Number);
 }
 else for (int32 i=0;i<MatchOptions.BotCount;++i) AddBot(CrosshairMatchRules::NoTeam,i+1);
}
void ACrosshairMatchGameMode::AddBot(uint8 Team,int32 Number)
{
 static const TCHAR* Callsigns[]={TEXT("Viper"),TEXT("Ghost"),TEXT("Hawk"),TEXT("Raven"),TEXT("Saber"),TEXT("Echo"),TEXT("Falcon"),TEXT("Wolf"),TEXT("Cobra"),TEXT("Ranger"),TEXT("Bishop"),TEXT("Nomad")};
 FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Bot=GetWorld()->SpawnActor<ACrosshairBotController>(ACrosshairBotController::StaticClass(),Params);
 if (!Bot) return;
 if (auto* State=Bot->GetPlayerState<ACrosshairMatchPlayerState>())
 {
  State->Team=Team;
  State->SetPlayerName(FString::Printf(TEXT("%s (bot)"),Callsigns[(Number-1)%UE_ARRAY_COUNT(Callsigns)]));
 }
 RestartPlayer(Bot);
}
uint8 ACrosshairMatchGameMode::TeamOf(const AController* Controller) const
{
 const auto* State=Controller ? Controller->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
 return State ? State->Team : CrosshairMatchRules::NoTeam;
}
void ACrosshairMatchGameMode::AssignTeam(AController* Controller)
{
 auto* Player=Controller ? Controller->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
 if (!Player) return;
 if (MatchOptions.Mode!=ECrosshairMatchMode::TeamDeathmatch) { Player->Team=CrosshairMatchRules::NoTeam; return; }
 int32 Alpha=0,Bravo=0;
 for (APlayerState* Other:GameState->PlayerArray)
  if (const auto* Match=Cast<ACrosshairMatchPlayerState>(Other); Match && Match!=Player && !Match->IsOnlyASpectator())
  {
   if (Match->Team==CrosshairMatchRules::Alpha) ++Alpha;
   else if (Match->Team==CrosshairMatchRules::Bravo) ++Bravo;
  }
 Player->Team=Alpha<=Bravo ? CrosshairMatchRules::Alpha : CrosshairMatchRules::Bravo;
}
FString ACrosshairMatchGameMode::InitNewPlayer(APlayerController* NewPlayerController,const FUniqueNetIdRepl& UniqueId,const FString& Options,const FString& Portal)
{
 AssignTeam(NewPlayerController);
 const FString Error=Super::InitNewPlayer(NewPlayerController,UniqueId,Options,Portal);
 // The Null subsystem name is a machine ID. Offline, use the saved username at once; LAN players send
 // theirs on arrival (ACrosshairPlayerController::ServerSetPlayerName), each machine its own.
 if (GetNetMode()==NM_Standalone && NewPlayerController->PlayerState)
  NewPlayerController->PlayerState->SetPlayerName(UCrosshairBotMatch::CleanPlayerName(GetGameInstance()->GetSubsystem<UCrosshairReplaySubsystem>()->GetSettings().PlayerName));
 return Error;
}
void ACrosshairMatchGameMode::RestartPlayerAtPlayerStart(AController* NewPlayer,AActor* StartSpot)
{
 Super::RestartPlayerAtPlayerStart(NewPlayer,StartSpot);
 // Spawn bookkeeping happens only when a pawn really spawns, not on every start-spot query.
 auto* Point=Cast<ACrosshairSpawnPoint>(StartSpot);
 if (!NewPlayer || !NewPlayer->GetPawn() || !Point) return;
 Point->LastUsedAt=GetWorld()->GetTimeSeconds(); HasSpawned.Add(NewPlayer); LastSpawn.Add(NewPlayer,Point);
 UE_LOG(LogTemp,Verbose,TEXT("Crosshair match spawn: %s team %d -> %s"),*GetNameSafe(NewPlayer),TeamOf(NewPlayer),*Point->Area);
}
AActor* ACrosshairMatchGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
 // Local players log in before StartPlay, so spawn points may not be gathered yet.
 if (SpawnPoints.IsEmpty()) CollectSpawnPoints();
 if (SpawnPoints.IsEmpty()) return Super::ChoosePlayerStart_Implementation(Player);
 const bool bTeamMode=MatchOptions.Mode==ECrosshairMatchMode::TeamDeathmatch;
 const uint8 Team=TeamOf(Player);
 const double Now=GetWorld()->GetTimeSeconds();
 TArray<const ACrosshairCharacter*> Enemies,Friends;
 for (TActorIterator<ACrosshairCharacter> It(GetWorld());It;++It)
 {
  if (!It->GetController() || It->GetController()==Player || It->Health->bDead) continue;
  (CrosshairMatchRules::CanDamage(bTeamMode,Team,TeamOf(It->GetController()),false) ? Enemies : Friends).Add(*It);
 }
 FCollisionQueryParams Params(SCENE_QUERY_STAT(CrosshairSpawnSight),false);
 auto Choose=[&](bool bInitial,bool bAllowOccupied)->ACrosshairSpawnPoint*
 {
  ACrosshairSpawnPoint* Best=nullptr; float BestScore=-FLT_MAX;
  for (ACrosshairSpawnPoint* Point:SpawnPoints)
  {
   if (!Point || !(bTeamMode ? Point->bTeamDeathmatch : Point->bFreeForAll)) continue;
   if (bInitial && bTeamMode && uint8(Point->InitialTeam)!=Team) continue;
   const FVector Location=Point->GetActorLocation();
   if (!bAllowOccupied && GetWorld()->OverlapAnyTestByObjectType(Location,FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeCapsule(40.f,90.f))) continue;
   CrosshairMatchRules::FSpawnContext Context;
   Context.bTeamMode=bTeamMode; Context.SecondsSinceUsed=Now-Point->LastUsedAt;
   const FVector Eye=Location+FVector(0,0,64);
   for (const ACrosshairCharacter* Enemy:Enemies)
   {
    const FVector Head=Enemy->GetPawnViewLocation();
    Context.NearestEnemy=FMath::Min(Context.NearestEnemy,float(FVector::Dist(Eye,Head)));
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Head,ECC_Visibility,Params) || Hit.GetActor()==Enemy) ++Context.VisibleEnemies;
   }
   for (const ACrosshairCharacter* Friend:Friends) Context.NearestFriend=FMath::Min(Context.NearestFriend,float(FVector::Dist(Eye,Friend->GetActorLocation())));
   const float Score=CrosshairMatchRules::ScoreSpawn(Context)+FMath::FRand()*15.f;
   if (Score>BestScore) { BestScore=Score; Best=Point; }
  }
  return Best;
 };
 const bool bInitial=!HasSpawned.Contains(Player);
 ACrosshairSpawnPoint* Point=Choose(bInitial,false);
 if (!Point && bInitial)
 {
  UE_LOG(LogTemp,Warning,TEXT("Crosshair match: no free start point for team %d; using a general spawn."),Team);
  Point=Choose(false,false);
 }
 if (!Point) Point=Choose(false,true);
 return Point ? Point : Super::ChoosePlayerStart_Implementation(Player);
}
bool ACrosshairMatchGameMode::CanDamage(AController* Attacker,AController* Victim) const
{
 const auto* State=GetGameState<ACrosshairMatchState>();
 if (!State || State->bMatchOver) return false;
 if (!Attacker || !Victim) return true;
 return CrosshairMatchRules::CanDamage(State->IsTeamMode(),TeamOf(Attacker),TeamOf(Victim),Attacker==Victim);
}
void ACrosshairMatchGameMode::NotifyKilled(AController* Killer,AController* Victim,bool bHeadshot,const FString& Weapon)
{
 auto* State=GetGameState<ACrosshairMatchState>(); if (!State) return;
 auto* VictimState=Victim ? Victim->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
 auto* KillerState=Killer && Killer!=Victim ? Killer->GetPlayerState<ACrosshairMatchPlayerState>() : nullptr;
 if (VictimState) ++VictimState->Deaths;
 if (KillerState)
 {
  ++KillerState->Kills;
  if (State->IsTeamMode()) (KillerState->Team==CrosshairMatchRules::Alpha ? State->AlphaScore : State->BravoScore)+=1;
 }
 FCrosshairKillEntry Entry;
 Entry.Killer=KillerState ? KillerState->GetPlayerName() : FString();
 Entry.Victim=VictimState ? VictimState->GetPlayerName() : TEXT("Player");
 Entry.Weapon=Weapon; Entry.bHeadshot=bHeadshot;
 Entry.KillerTeam=KillerState ? KillerState->Team : 0; Entry.VictimTeam=VictimState ? VictimState->Team : 0;
 Entry.ServerTime=State->GetServerWorldTimeSeconds();
 State->KillFeed.Add(Entry);
 if (State->KillFeed.Num()>6) State->KillFeed.RemoveAt(0);
 if (KillerState)
 {
  const int32 Score=State->IsTeamMode() ? State->GetTeamScore(KillerState->Team) : KillerState->Kills;
  if (CrosshairMatchRules::ReachedLimit(Score,State->ScoreLimit))
   EndMatch(State->IsTeamMode() ? ACrosshairMatchState::TeamName(KillerState->Team)+TEXT(" wins") : KillerState->GetPlayerName()+TEXT(" wins"));
 }
 if (Victim && !State->bMatchOver)
 {
  FTimerHandle Handle;
  GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[this,Weak=TWeakObjectPtr<AController>(Victim)]()
  { if (Weak.IsValid()) Respawn(Weak.Get()); }),RespawnDelay,false);
 }
}
void ACrosshairMatchGameMode::Respawn(AController* Controller)
{
 const auto* State=GetGameState<ACrosshairMatchState>();
 if (!Controller || !State || State->bMatchOver) return;
 if (APawn* Old=Controller->GetPawn()) { Controller->UnPossess(); Old->Destroy(); }
 RestartPlayer(Controller);
 if (!Controller->GetPawn())
 {
  FTimerHandle Handle;
  GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[this,Weak=TWeakObjectPtr<AController>(Controller)]()
  { if (Weak.IsValid()) Respawn(Weak.Get()); }),1.f,false);
 }
}
void ACrosshairMatchGameMode::Tick(float DeltaSeconds)
{
 Super::Tick(DeltaSeconds);
 const auto* State=GetGameState<ACrosshairMatchState>();
 if (!State || State->bMatchOver || bInvalidMap || State->MatchEndsAt<=0 || State->GetRemainingSeconds()>0) return;
 if (State->IsTeamMode())
  EndMatch(State->AlphaScore==State->BravoScore ? FString(TEXT("Draw")) : ACrosshairMatchState::TeamName(State->AlphaScore>State->BravoScore ? CrosshairMatchRules::Alpha : CrosshairMatchRules::Bravo)+TEXT(" wins"));
 else
 {
  const auto Standings=State->GetStandings();
  const bool Tie=Standings.Num()>1 && Standings[0]->Kills==Standings[1]->Kills;
  EndMatch(Standings.IsEmpty() || Tie ? FString(TEXT("Draw")) : Standings[0]->GetPlayerName()+TEXT(" wins"));
 }
}
void ACrosshairMatchGameMode::EndMatch(const FString& Result)
{
 auto* State=GetGameState<ACrosshairMatchState>();
 if (!State || State->bMatchOver) return;
 State->bMatchOver=true; State->Result=Result;
 for (TActorIterator<ACrosshairCharacter> It(GetWorld());It;++It) It->StopActions();
 UE_LOG(LogTemp,Display,TEXT("Crosshair match over: %s"),*Result);
 // Players can leave immediately from the results screen; otherwise return to Home.
 FTimerHandle Handle;
 GetWorldTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[this]()
 { GetGameInstance()->GetSubsystem<UCrosshairMatchSession>()->LeaveMatch(); }),ResultsSeconds,false);
}
void ACrosshairMatchGameMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage)
{
 Super::PreLogin(Options,Address,UniqueId,ErrorMessage);
 if (ErrorMessage.IsEmpty() && bLanMatch && GetNumPlayers()>=MaxHumans)
  ErrorMessage=FString::Printf(TEXT("This LAN match is full (%d players)."),MaxHumans);
}
