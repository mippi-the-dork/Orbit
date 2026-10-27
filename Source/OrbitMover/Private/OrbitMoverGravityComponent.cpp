// Copyright Mippithedork 2026, Inc. All Rights Reserved.

#include "OrbitMoverGravityComponent.h"

#include "OrbitMoverGravityPolicy.h"
#include "OrbitWorldSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitMover, Log, All);

namespace OrbitMoverReflection
{
    static bool DerivesFromName(const UClass* Class, FName Name)
    {
        for (const UClass* Type = Class; Type; Type = Type->GetSuperClass())
        {
            if (Type->GetFName() == Name) { return true; }
        }
        return false;
    }

    static bool HasVectorProperty(const FStructProperty* Property)
    {
        return Property && Property->Struct == TBaseStructure<FVector>::Get();
    }
}

UOrbitMoverGravityComponent::UOrbitMoverGravityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

bool UOrbitMoverGravityComponent::ReadMoverAsyncFlag(bool& bOutAsync) const
{
    bOutAsync = true;
    if (!IsValid(MoverComponent)) { return false; }
    UFunction* Function = MoverComponent->FindFunction(FName(TEXT("IsBackendAsync")));
    FBoolProperty* Result = Function ? FindFProperty<FBoolProperty>(Function, TEXT("ReturnValue")) : nullptr;
    if (!Function || !Result || Function->ParmsSize > 64) { return false; }
    uint8 Parameters[64] = {};
    MoverComponent->ProcessEvent(Function, Parameters);
    bOutAsync = Result->GetPropertyValue_InContainer(Parameters);
    return true;
}

bool UOrbitMoverGravityComponent::ReadMoverVector(const FName FunctionName, FVector& OutValue) const
{
    if (!IsValid(MoverComponent)) { return false; }
    UFunction* Function = MoverComponent->FindFunction(FunctionName);
    FStructProperty* Result = Function ? FindFProperty<FStructProperty>(Function, TEXT("ReturnValue")) : nullptr;
    if (!Function || !OrbitMoverReflection::HasVectorProperty(Result) || Function->ParmsSize > 128) { return false; }
    alignas(16) uint8 Parameters[128] = {};
    MoverComponent->ProcessEvent(Function, Parameters);
    const FVector* Value = Result->ContainerPtrToValuePtr<FVector>(Parameters);
    if (!Value || Value->ContainsNaN()) { return false; }
    OutValue = *Value;
    return true;
}

bool UOrbitMoverGravityComponent::ApplyOverride(const FName FunctionName, const FName FlagName, const FName VectorName, bool bEnabled, const FVector& Value)
{
    if (!IsValid(MoverComponent)) { return false; }
    UFunction* Function = MoverComponent->FindFunction(FunctionName);
    FBoolProperty* Flag = Function ? FindFProperty<FBoolProperty>(Function, FlagName) : nullptr;
    FStructProperty* Vector = Function ? FindFProperty<FStructProperty>(Function, VectorName) : nullptr;
    if (!Function || !Flag || !OrbitMoverReflection::HasVectorProperty(Vector) || Function->ParmsSize > 128) { return false; }

    alignas(16) uint8 Parameters[128] = {};
    Flag->SetPropertyValue_InContainer(Parameters, bEnabled);
    *Vector->ContainerPtrToValuePtr<FVector>(Parameters) = Value;
    MoverComponent->ProcessEvent(Function, Parameters);
    return true;
}

bool UOrbitMoverGravityComponent::IsSupportedMover() const
{
    if (!IsValid(MoverComponent) || !OrbitMoverReflection::DerivesFromName(MoverComponent->GetClass(), FName(TEXT("MoverComponent"))))
    {
        return false;
    }
    if (OrbitMoverReflection::DerivesFromName(MoverComponent->GetClass(), FName(TEXT("ChaosCharacterMoverComponent"))))
    {
        return false;
    }
    bool bAsync = true;
    if (!ReadMoverAsyncFlag(bAsync) || bAsync) { return false; }
    return MoverComponent->FindFunction(FName(TEXT("SetGravityOverride"))) &&
           MoverComponent->FindFunction(FName(TEXT("SetUpDirectionOverride"))) &&
           MoverComponent->FindFunction(FName(TEXT("GetUpDirection")));
}

void UOrbitMoverGravityComponent::BeginPlay()
{
    Super::BeginPlay();
    if (AActor* Owner = GetOwner())
    {
        TInlineComponentArray<UActorComponent*> Components;
        Owner->GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            if (IsValid(Component) && OrbitMoverReflection::DerivesFromName(Component->GetClass(), FName(TEXT("MoverComponent"))))
            {
                MoverComponent = Component;
                break;
            }
        }
    }

    bBackendSupported = IsSupportedMover();
    if (!bBackendSupported)
    {
        UE_LOG(LogOrbitMover, Warning,
            TEXT("%s: Requires a synchronous kinematic Mover component and Epic Mover enabled; Chaos/async modes are unsupported."),
            *GetPathName());
        bWarnedUnsupported = true;
        return;
    }
    FVector CurrentUp;
    if (ReadMoverVector(TEXT("GetUpDirection"), CurrentUp) && !CurrentUp.IsNearlyZero())
    {
        LastStableUp = CurrentUp.GetSafeNormal();
    }

    // Establish ordering when the Mover component participates in the normal tick graph.
    // Other prediction/simulation backends are NOT guaranteed to honor this prerequisite.
    MoverComponent->AddTickPrerequisiteComponent(this);
    ApplyOrbitGravity();
}

void UOrbitMoverGravityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(MoverComponent))
    {
        MoverComponent->RemoveTickPrerequisiteComponent(this);
        ReleaseOverrides();
    }
    MoverComponent = nullptr;
    Super::EndPlay(EndPlayReason);
}

void UOrbitMoverGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (IsInGameThread()) { ApplyOrbitGravity(); }
}

void UOrbitMoverGravityComponent::ReleaseOverrides()
{
    if (!bOwnsOverrides || !IsValid(MoverComponent))
    {
        bOwnsOverrides = false;
        return;
    }
    ApplyOverride(FName(TEXT("SetGravityOverride")), FName(TEXT("bOverrideGravity")), FName(TEXT("GravityAcceleration")), false, FVector::ZeroVector);
    ApplyOverride(FName(TEXT("SetUpDirectionOverride")), FName(TEXT("bOverrideUpDirection")), FName(TEXT("UpDirection")), false, FVector::UpVector);
    bOwnsOverrides = false;
}

void UOrbitMoverGravityComponent::SetOrbitMoverGravityEnabled(bool bEnabled)
{
    bEnableOrbitGravity = bEnabled;
    if (bEnabled)
    {
        RefreshOrbitMoverGravity();
    }
    else
    {
        ReleaseOverrides();
        bInsideOrbitField = false;
        LastOrbitAcceleration = FVector::ZeroVector;
    }
}

void UOrbitMoverGravityComponent::RefreshOrbitMoverGravity()
{
    if (IsInGameThread()) { ApplyOrbitGravity(); }
}

void UOrbitMoverGravityComponent::ApplyOrbitGravity()
{
    if (!IsValid(MoverComponent)) { return; }
    bBackendSupported = IsSupportedMover();
    if (!bBackendSupported || !bEnableOrbitGravity)
    {
        ReleaseOverrides();
        bInsideOrbitField = false;
        if (!bBackendSupported && !bWarnedUnsupported)
        {
            UE_LOG(LogOrbitMover, Warning, TEXT("%s: Unsupported Mover backend or missing reflection API."), *GetPathName());
            bWarnedUnsupported = true;
        }
        return;
    }

    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    const UOrbitWorldSubsystem* Gravity = World ? World->GetSubsystem<UOrbitWorldSubsystem>() : nullptr;
    if (!Gravity || !Owner) { ReleaseOverrides(); return; }

    LastSamplePosition = Owner->GetActorLocation();
    const FOrbitGravitySample Sample = Gravity->SampleGravityForActor(
        Owner, LastSamplePosition, EOrbitGravityChannel::Character);
    bInsideOrbitField = Sample.bHasInfluence;
    LastOrbitAcceleration = bInsideOrbitField ? Sample.Acceleration : FVector::ZeroVector;

    const FOrbitMoverGravityDecision Decision = FOrbitMoverGravityPolicy::Resolve(Sample, LastStableUp);
    if (!Decision.bOverrideGravity)
    {
        ReleaseOverrides();
        return;
    }

    const bool bAppliedGravity = ApplyOverride(FName(TEXT("SetGravityOverride")), FName(TEXT("bOverrideGravity")), FName(TEXT("GravityAcceleration")), true, Decision.Acceleration);
    const bool bAppliedUp = ApplyOverride(FName(TEXT("SetUpDirectionOverride")), FName(TEXT("bOverrideUpDirection")), FName(TEXT("UpDirection")), true, Decision.Up);
    if (!bAppliedGravity || !bAppliedUp)
    {
        // Relinquish any partial override rather than leaving an inconsistent Mover state.
        bOwnsOverrides = true;
        ReleaseOverrides();
        bBackendSupported = false;
        return;
    }

    bOwnsOverrides = true;
    LastStableUp = Decision.Up;
    LastOrbitUp = Decision.Up;
    ++SamplesApplied;
}
