#include "Misc/AutomationTest.h"
#include "OrbitGravitySource.h"
#include "OrbitGravitySourceComponent.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOrbitDetailsMetadataTest,
    "Orbit.Editor.Details.SourcePresentationMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOrbitDetailsMetadataTest::RunTest(const FString& Parameters)
{
    struct FExpectedField
    {
        const TCHAR* Name;
        const TCHAR* Category;
    };
    const FExpectedField Expected[] =
    {
        {TEXT("Shape"), TEXT("Orbit")},
        {TEXT("Direction"), TEXT("Orbit")},
        {TEXT("Strength"), TEXT("Orbit")},
        {TEXT("Falloff"), TEXT("Orbit")},
        {TEXT("MaxDistance"), TEXT("Orbit")},
        {TEXT("GravityChannels"), TEXT("Orbit")},
        {TEXT("bAffectCharacters"), TEXT("Orbit")},
        {TEXT("bAffectPhysics"), TEXT("Orbit")},
        {TEXT("Combine"), TEXT("Orbit")},
        {TEXT("Priority"), TEXT("Orbit")},
        {TEXT("bLimitToBox"), TEXT("Orbit")},
        {TEXT("VolumeShape"), TEXT("Orbit")},
        {TEXT("SplineSampleSpacing"), TEXT("Orbit")},
        {TEXT("SplineMaxSegments"), TEXT("Orbit")},
        {TEXT("SplineUpdateMode"), TEXT("Orbit")},
        {TEXT("SplineCheckInterval"), TEXT("Orbit")},
        {TEXT("VortexSpin"), TEXT("Orbit")},
    };
    for (const UClass* Class : {AOrbitGravitySource::StaticClass(), UOrbitGravitySourceComponent::StaticClass()})
    {
        for (const FExpectedField& ExpectedField : Expected)
        {
            const FProperty* Property = Class->FindPropertyByName(FName(ExpectedField.Name));
            TestNotNull(FString::Printf(TEXT("%s.%s exists"), *Class->GetName(), ExpectedField.Name), Property);
            if (!Property) { continue; }
            TestEqual(FString::Printf(TEXT("%s.%s category"), *Class->GetName(), ExpectedField.Name),
                Property->GetMetaData(TEXT("Category")), FString(ExpectedField.Category));
            TestTrue(FString::Printf(TEXT("%s.%s tooltip"), *Class->GetName(), ExpectedField.Name),
                !Property->GetMetaData(TEXT("ToolTip")).IsEmpty());
        }
        const FProperty* Enabled = Class->FindPropertyByName(TEXT("bEnabled"));
        TestNotNull(FString::Printf(TEXT("%s.bEnabled retained"), *Class->GetName()), Enabled);
    }
    const AOrbitGravitySource* SourceDefaults = GetDefault<AOrbitGravitySource>();
    TestFalse(TEXT("New Orbit sources default to no in-game debug rendering"), SourceDefaults->bShowDebugInGame);
    const FProperty* ShowInGame = AOrbitGravitySource::StaticClass()->FindPropertyByName(TEXT("bShowDebugInGame"));
    TestNotNull(TEXT("Show In Game debug option exists"), ShowInGame);
    if (ShowInGame)
    {
        TestEqual(TEXT("Show In Game remains within visualization settings"),
            ShowInGame->GetMetaData(TEXT("Category")), FString(TEXT("Orbit")));
        TestTrue(TEXT("Show In Game has a meaningful tooltip"),
            !ShowInGame->GetMetaData(TEXT("ToolTip")).IsEmpty());
    }
    return true;
}
#endif
