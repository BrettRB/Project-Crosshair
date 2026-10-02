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

#endif
