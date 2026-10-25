#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../CrosshairData.h"
#include "../CrosshairMapLibrary.h"
#include "../CrosshairPractice.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairStickTest, "Crosshair.Input.DeadZoneAndFrameRate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairStickTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Center is stable"), CrosshairRules::FilterStick(FVector2D(.02,.02), .12f, 1.5f).IsNearlyZero());
	TestTrue(TEXT("Full stick reaches full output"), CrosshairRules::FilterStick(FVector2D(1,0), .12f, 1.5f).Equals(FVector2D(1,0)));
	TestTrue(TEXT("Diagonal stays within unit circle"), CrosshairRules::FilterStick(FVector2D(1,1), .12f, 1.5f).Size() <= 1.0001);
	TestTrue(TEXT("Negative direction preserved"), CrosshairRules::FilterStick(FVector2D(-1,0), .12f, 1.5f).X < 0);
	const FCrosshairSettings Settings;
	for (int32 FPS : {30, 60, 120})
	{
		FVector2D Turn = FVector2D::ZeroVector;
		for (int32 i=0; i<FPS; ++i) Turn += CrosshairRules::StickDelta(FVector2D(1,0), Settings, 0, 1.f/FPS);
		TestTrue(FString::Printf(TEXT("One second yaw at %d FPS"), FPS), FMath::IsNearlyEqual(Turn.X, double(Settings.StickYawSpeed), .001));
	}
	TestTrue(TEXT("ADS scales rotation"), FMath::IsNearlyEqual(CrosshairRules::StickDelta(FVector2D(1,0), Settings, 1, 1).X, double(Settings.StickYawSpeed * Settings.AimSensitivity), .001));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairCalibrationTest, "Crosshair.Input.HardwareDirectionCalibration", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairCalibrationTest::RunTest(const FString& Parameters)
{
	FCrosshairSettings Settings;
	const FVector2D Standard = CrosshairRules::StickDelta(FVector2D(.7,.8),Settings,0,.016f);
	for (int32 X : {-1,1}) for (int32 Y : {-1,1})
	{
		Settings.ControllerAxisDirection = FVector2D(X,Y);
		const FVector2D Input(.7*X,.8*Y);
		TestTrue(TEXT("Device directions normalize independently"), CrosshairRules::StickDelta(Input,Settings,0,.016f).Equals(Standard,.001));
		Settings.bInvertControllerVertical = true;
		const FVector2D Inverted = CrosshairRules::StickDelta(Input,Settings,0,.016f);
		TestTrue(TEXT("User vertical inversion still applies after device normalization"), FMath::IsNearlyEqual(Inverted.X,Standard.X,.001) && FMath::IsNearlyEqual(Inverted.Y,-Standard.Y,.001));
		TestTrue(TEXT("Mouse ignores hardware calibration"), CrosshairRules::MouseDelta(FVector2D(10,10),Settings,0).Equals(FVector2D(1.2,1.2),.001));
		Settings.bInvertControllerVertical = false;
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairFireTest, "Crosshair.Weapon.FireGates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairFireTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("Empty weapon cannot fire"), CrosshairRules::CanFire(0, false, 10, 0));
	TestFalse(TEXT("Reload blocks shots"), CrosshairRules::CanFire(5, true, 10, 0));
	TestFalse(TEXT("Equip/shot delay blocks early shots"), CrosshairRules::CanFire(5, false, 1, 2));
	TestTrue(TEXT("Exact deadline permits shot"), CrosshairRules::CanFire(5, false, 2, 2));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairSurfaceTest, "Crosshair.Target.SupportedSurface", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairSurfaceTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Floor accepted"), UCrosshairPlacementComponent::IsSupportedSurface(FVector::UpVector));
	TestFalse(TEXT("Wall rejected"), UCrosshairPlacementComponent::IsSupportedSurface(FVector::ForwardVector));
	TestFalse(TEXT("Ceiling rejected"), UCrosshairPlacementComponent::IsSupportedSurface(-FVector::UpVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairMouseTest, "Crosshair.Input.MouseDisplacement", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairMouseTest::RunTest(const FString& Parameters)
{
    FCrosshairSettings Settings;
    const FVector2D Raw(100, 50);
    const FVector2D Hip = CrosshairRules::MouseDelta(Raw, Settings, 0);
    TestTrue(TEXT("Raw displacement uses only game sensitivity"), Hip.Equals(FVector2D(12,6), .001));
    TestTrue(TEXT("Positive mouse Y looks up"), Hip.Y > 0);
    TestTrue(TEXT("ADS scales both axes"), CrosshairRules::MouseDelta(Raw, Settings, 1).Equals(Hip * Settings.AimSensitivity, .001));
    Settings.MouseSensitivity *= 2;
    TestTrue(TEXT("Sensitivity doubles displacement"), CrosshairRules::MouseDelta(Raw, Settings, 0).Equals(Hip * 2, .001));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairDamageTest, "Crosshair.Target.DamageThresholds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairDamageTest::RunTest(const FString& Parameters)
{
    float Health = 100;
    for (int32 i=0; i<4; ++i) Health = CrosshairRules::RemainingHealth(Health,20);
    TestTrue(TEXT("Four body hits survive"), Health > 0);
    TestEqual(TEXT("Five body hits defeat"), CrosshairRules::RemainingHealth(Health,20), 0.f);
    Health = CrosshairRules::RemainingHealth(CrosshairRules::RemainingHealth(100,100.f/3),100.f/3);
    TestTrue(TEXT("Two head hits survive"), Health > 0);
    TestEqual(TEXT("Three head hits defeat despite rounding"), CrosshairRules::RemainingHealth(Health,100.f/3), 0.f);
    Health = CrosshairRules::RemainingHealth(CrosshairRules::RemainingHealth(100,100.f/3),100.f/3);
    TestTrue(TEXT("Two heads plus one body survive"), CrosshairRules::RemainingHealth(Health,20) > 0);
    TestEqual(TEXT("Two heads plus two bodies defeat"), CrosshairRules::RemainingHealth(CrosshairRules::RemainingHealth(Health,20),20),0.f);
    TestEqual(TEXT("Sniper single hit defeats"),CrosshairRules::RemainingHealth(100,100),0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairInversionTest, "Crosshair.Input.IndependentControllerInversion", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairInversionTest::RunTest(const FString& Parameters)
{
	FCrosshairSettings Settings;
	TestFalse(TEXT("Horizontal inversion defaults off"), Settings.bInvertControllerHorizontal);
	TestFalse(TEXT("Vertical inversion defaults off"), Settings.bInvertControllerVertical);
	const FVector2D Raw(.6,.8);
	const FVector2D Normal = CrosshairRules::StickDelta(Raw, Settings, 0, 1);
	for (int32 Flags=0; Flags<4; ++Flags)
	{
		Settings.bInvertControllerHorizontal = (Flags & 1) != 0;
		Settings.bInvertControllerVertical = (Flags & 2) != 0;
		const FVector2D Turn = CrosshairRules::StickDelta(Raw, Settings, 0, 1);
		TestTrue(TEXT("Each controller axis inverts independently"), Turn.Equals(FVector2D((Flags & 1) ? -Normal.X : Normal.X, (Flags & 2) ? -Normal.Y : Normal.Y), .001));
		TestTrue(TEXT("ADS preserves inversion and scales rate"), CrosshairRules::StickDelta(Raw, Settings, 1, 1).Equals(Turn * Settings.AimSensitivity, .001));
		TestTrue(TEXT("Mouse does not inherit controller inversion"), CrosshairRules::MouseDelta(Raw, Settings, 0).Equals(Raw * Settings.MouseSensitivity, .001));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairObjTest, "Crosshair.Map.OBJValidation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairObjTest::RunTest(const FString& Parameters)
{
 FCrosshairMapMesh Mesh; FString Error;
 TestTrue(TEXT("Scaled negative-index quad imports"),UCrosshairMapLibrary::ParseObj(TEXT("v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\nf -4 -3 -2 -1\n"),2,Mesh,Error));
 TestEqual(TEXT("Quad becomes two triangles"),Mesh.Triangles.Num(),6);
 TestTrue(TEXT("Centimeter scale applied"),Mesh.Vertices.Contains(FVector(20,20,0)));
 TestTrue(TEXT("Upward shading normal retained"),Mesh.Normals[0].Z>.99);
 TestTrue(TEXT("Collision winding converted for upward floor"),FVector::CrossProduct(Mesh.Vertices[2]-Mesh.Vertices[0],Mesh.Vertices[1]-Mesh.Vertices[0]).Z>0);
 TestFalse(TEXT("Missing vertex rejected"),UCrosshairMapLibrary::ParseObj(TEXT("v 0 0 0\nf 1 2 3\n"),1,Mesh,Error));
 TestTrue(TEXT("Failed parsing leaves no partial geometry"),Mesh.Vertices.IsEmpty());
 TestFalse(TEXT("Nonfinite position rejected"),UCrosshairMapLibrary::ParseObj(TEXT("v nan 0 0\n"),1,Mesh,Error));
 TestFalse(TEXT("Zero scale rejected"),UCrosshairMapLibrary::ParseObj(TEXT("v 0 0 0\n"),0,Mesh,Error));
 TestFalse(TEXT("Empty geometry rejected"),UCrosshairMapLibrary::ParseObj(TEXT("# no faces\n"),1,Mesh,Error));
 return true;
}

#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairBotTuningTest, "Crosshair.Bots.DifficultyPresets", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairBotTuningTest::RunTest(const FString& Parameters)
{
 auto Previous=UCrosshairBotMatch::TuningFor(ECrosshairBotDifficulty::Easy);
 for (auto Difficulty:{ECrosshairBotDifficulty::Regular,ECrosshairBotDifficulty::Hardened,ECrosshairBotDifficulty::Veteran})
 {
  const auto Next=UCrosshairBotMatch::TuningFor(Difficulty);
  TestTrue(TEXT("Difficulty lowers reaction, aim error and decision latency"),Next.ReactionSeconds<Previous.ReactionSeconds && Next.AimErrorDegrees<Previous.AimErrorDegrees && Next.DecisionSeconds<Previous.DecisionSeconds);
  TestTrue(TEXT("Difficulty increases tracking and movement"),Next.TrackingDegreesPerSecond>Previous.TrackingDegreesPerSecond && Next.MovementSpeed>Previous.MovementSpeed);
  TestTrue(TEXT("Veteran still has reaction latency and aim error"),Next.ReactionSeconds>0 && Next.AimErrorDegrees>0); Previous=Next;
 }
 FCrosshairBotMatchOptions Invalid; Invalid.Difficulty=ECrosshairBotDifficulty(255);Invalid.BotCount=1000;Invalid.ScoreLimit=-5;Invalid.TimeLimitMinutes=0; UCrosshairBotMatch::Validate(Invalid);
 TestTrue(TEXT("Corrupt/unsupported saved options are bounded"),Invalid.Difficulty==ECrosshairBotDifficulty::Regular && Invalid.BotCount==11 && Invalid.ScoreLimit==5 && Invalid.TimeLimitMinutes==1);
 return true;
}

#if WITH_DEV_AUTOMATION_TESTS
#include "../CrosshairMatch.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairMatchRulesTest, "Crosshair.Match.Rules", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairMatchRulesTest::RunTest(const FString& Parameters)
{
 using namespace CrosshairMatchRules;
 // Spawn safety: distance from enemies, enemy sight lines, teammates and repeat use.
 FSpawnContext Safe; Safe.NearestEnemy=3500.f;
 FSpawnContext Near=Safe; Near.NearestEnemy=600.f;
 FSpawnContext Seen=Safe; Seen.VisibleEnemies=1;
 FSpawnContext Reused=Safe; Reused.SecondsSinceUsed=1.f;
 TestTrue(TEXT("Spawns far from enemies beat spawns beside them"),ScoreSpawn(Safe)>ScoreSpawn(Near)+150.f);
 TestTrue(TEXT("Spawns in enemy sight lines are heavily penalised"),ScoreSpawn(Safe)>ScoreSpawn(Seen)+200.f);
 TestTrue(TEXT("A just-used spawn is avoided"),ScoreSpawn(Safe)>ScoreSpawn(Reused));
 FSpawnContext Team=Safe; Team.bTeamMode=true; Team.NearestFriend=800.f;
 FSpawnContext Alone=Team; Alone.NearestFriend=6000.f;
 TestTrue(TEXT("TDM prefers spawning near teammates"),ScoreSpawn(Team)>ScoreSpawn(Alone));
 FSpawnContext Ffa=Team; Ffa.bTeamMode=false;
 TestEqual(TEXT("FFA ignores teammates"),ScoreSpawn(Ffa),ScoreSpawn(Alone));
 // Damage permissions.
 TestFalse(TEXT("TDM friendly fire is off"),CanDamage(true,Alpha,Alpha,false));
 TestTrue(TEXT("TDM enemies can be damaged"),CanDamage(true,Alpha,Bravo,false));
 TestTrue(TEXT("FFA allows damaging anyone"),CanDamage(false,NoTeam,NoTeam,false));
 TestTrue(TEXT("Self-damage is always allowed"),CanDamage(true,Alpha,Alpha,true));
 // Score limit and regeneration.
 TestTrue(TEXT("Limit reached at the limit"),ReachedLimit(30,30));
 TestFalse(TEXT("Below the limit continues"),ReachedLimit(29,30));
 TestFalse(TEXT("A zero limit never ends the match"),ReachedLimit(99,0));
 TestEqual(TEXT("No regeneration during the delay"),RegenerateHealth(40.f,100.f,3.9f,.5f),40.f);
 TestEqual(TEXT("Regeneration after the delay"),RegenerateHealth(40.f,100.f,4.1f,.5f),60.f);
 TestEqual(TEXT("Regeneration caps at max health"),RegenerateHealth(95.f,100.f,10.f,1.f),100.f);
 // Team fill: 6v6 with the player on Alpha.
 TestEqual(TEXT("Player's team gets five bots"),BotsForTeam(6,1),5);
 TestEqual(TEXT("Opposing team gets six bots"),BotsForTeam(6,0),6);
 TestEqual(TEXT("Never negative bots"),BotsForTeam(1,3),0);
 // Saved option bounds and COD defaults.
 FCrosshairBotMatchOptions Defaults;
 TestTrue(TEXT("COD defaults: 8-player FFA to 30, 6v6 TDM to 75, 10 minutes, Nuketown"),Defaults.BotCount==7 && Defaults.ScoreLimit==30 && Defaults.TeamSize==6 && Defaults.TeamScoreLimit==75 && Defaults.TimeLimitMinutes==10 && Defaults.Map==ECrosshairMatchMap::Nuketown);
 FCrosshairBotMatchOptions Invalid; Invalid.Mode=ECrosshairMatchMode(9); Invalid.Map=ECrosshairMatchMap(9); Invalid.TeamSize=40; Invalid.TeamScoreLimit=1;
 UCrosshairBotMatch::Validate(Invalid);
 TestTrue(TEXT("Corrupt mode/map/team options are bounded"),Invalid.Mode==ECrosshairMatchMode::FreeForAll && Invalid.Map==ECrosshairMatchMap::Nuketown && Invalid.TeamSize==6 && Invalid.TeamScoreLimit==10);
 TestTrue(TEXT("Matches run only on Nuketown, Grid and Space Station"),ACrosshairMatchGameMode::IsMatchMap(TEXT("L_Nuketown")) && ACrosshairMatchGameMode::IsMatchMap(TEXT("L_SpaceStation")) && ACrosshairMatchGameMode::IsMatchMap(TEXT("UEDPIE_0_L_Grid")) && !ACrosshairMatchGameMode::IsMatchMap(TEXT("L_Practice")) && !ACrosshairMatchGameMode::IsMatchMap(TEXT("L_UserMap")));
 TestEqual(TEXT("Match maps travel to the COD map packages"),UCrosshairBotMatch::MapPackage(ECrosshairMatchMap::Grid),FName(TEXT("/Game/Crosshair/Maps/L_Grid")));
 return true;
}
#include "../CrosshairTrickshot.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairTrickshotLabelTest, "Crosshair.Match.KillLabels", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairTrickshotLabelTest::RunTest(const FString& Parameters)
{
 using namespace CrosshairTrickshotRules;
 FKillContext Plain; Plain.Weapon=TEXT("Assault Rifle"); Plain.bAiming=true; Plain.DistanceMeters=12.f;
 TestEqual(TEXT("Aimed close kill is a plain kill"),Describe(Plain),FString(TEXT("Kill - Assault Rifle, 12 m")));
 TestFalse(TEXT("Plain kill gets no trickshot banner"),IsTrickshot(Plain));
 FKillContext Blind=Plain; Blind.bAiming=false;
 TestTrue(TEXT("Unaimed rifle kill is labelled hip-fire"),Describe(Blind).StartsWith(TEXT("Hip-fire")));
 TestFalse(TEXT("Hip-fire alone is not a banner trickshot"),IsTrickshot(Blind));
 FKillContext Classic; Classic.Weapon=TEXT("Sniper"); Classic.bScopedWeapon=true; Classic.bAirborne=true; Classic.SpinDegrees=400.f; Classic.bHeadshot=true; Classic.DistanceMeters=45.f;
 TestEqual(TEXT("Classic trickshot label"),Describe(Classic),FString(TEXT("Mid-air 360 No-scope Headshot Long shot - Sniper, 45 m")));
 TestTrue(TEXT("Classic trickshot gets the banner"),IsTrickshot(Classic));
 FKillContext Spin=Classic; Spin.SpinDegrees=760.f;
 TestTrue(TEXT("Two full spins label as 720"),Describe(Spin).Contains(TEXT("720")) && !Describe(Spin).Contains(TEXT("360")));
 FKillContext Bang=Plain; Bang.bWallbang=true;
 TestTrue(TEXT("Wallbang with any gun is a trickshot"),IsTrickshot(Bang) && Describe(Bang).StartsWith(TEXT("Wallbang")));
 FKillContext Axe; Axe.Weapon=TEXT("Tomahawk"); Axe.bLethal=true; Axe.DistanceMeters=16.f;
 TestTrue(TEXT("Lethals never count as no-scope/hip-fire; 15 m+ tomahawk is a long shot"),Describe(Axe)==TEXT("Long shot - Tomahawk, 16 m") && IsTrickshot(Axe));
 TestFalse(TEXT("Labels never contain the menu separator"),Describe(Classic).Contains(TEXT(": ")));
 return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "../CrosshairLocomotion.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCrosshairLocomotionWeightsTest, "Crosshair.Animation.LocomotionWeights", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FCrosshairLocomotionWeightsTest::RunTest(const FString& Parameters)
{
 using namespace CrosshairLocomotion;
 auto Sum=[](const FWeights& W){ float Total=W.Idle; for (int32 i=0;i<Directions;++i) Total+=W.Walk[i]+W.Jog[i]; return Total; };
 const FWeights Still=Compute(0.f,0.f);
 TestEqual(TEXT("Standing still is pure idle"),Still.Idle,1.f);
 const FWeights SlowForward=Compute(WalkSpeed*.5f,0.f);
 TestTrue(TEXT("Half walk speed blends idle and forward walk"),FMath::IsNearlyEqual(SlowForward.Idle,.5f) && FMath::IsNearlyEqual(SlowForward.Walk[0],.5f));
 const FWeights RunRight=Compute(JogSpeed,90.f);
 TestTrue(TEXT("Jog speed to the right is the right jog cycle"),FMath::IsNearlyEqual(RunRight.Jog[2],1.f) && RunRight.Idle<=0.f);
 const FWeights Diagonal=Compute(JogSpeed,22.5f);
 TestTrue(TEXT("Between forward and forward-right blends both evenly"),FMath::IsNearlyEqual(Diagonal.Jog[0],.5f) && FMath::IsNearlyEqual(Diagonal.Jog[1],.5f));
 const FWeights Back=Compute(JogSpeed,-180.f);
 TestTrue(TEXT("Moving backwards uses the backward cycle"),FMath::IsNearlyEqual(Back.Jog[4],1.f));
 const FWeights BackLeft=Compute((WalkSpeed+JogSpeed)*.5f,-135.f);
 TestTrue(TEXT("Backward-left between walk and jog"),FMath::IsNearlyEqual(BackLeft.Walk[5],.5f) && FMath::IsNearlyEqual(BackLeft.Jog[5],.5f));
 for (const FWeights& W:{Still,SlowForward,RunRight,Diagonal,Back,BackLeft}) TestTrue(TEXT("Weights always sum to one"),FMath::IsNearlyEqual(Sum(W),1.f,1e-4f));
 TestTrue(TEXT("Sprint speeds up the jog cycle (no foot sliding)"),PlayRate(680.f)>1.4f && PlayRate(JogSpeed)==1.f);
 return true;
}
#endif
