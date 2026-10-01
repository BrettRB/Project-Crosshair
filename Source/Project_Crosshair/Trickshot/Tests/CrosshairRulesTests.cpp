#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "../CrosshairData.h"
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
#endif
