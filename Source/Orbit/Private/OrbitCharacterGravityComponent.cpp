#include "OrbitCharacterGravityComponent.h"

#include "OrbitWorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

UOrbitCharacterGravityComponent::UOrbitCharacterGravityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UOrbitCharacterGravityComponent::BeginPlay()
{
    Super::BeginPlay();
    // Movement gravity direction must be updated before CharacterMovement's tick.
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->AddTickPrerequisiteComponent(this);
        }
    }
}

void UOrbitCharacterGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    UWorld* World = GetWorld();
    if (!Character || !World) { return; }
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    UOrbitWorldSubsystem* Gravity = World->GetSubsystem<UOrbitWorldSubsystem>();
    if (!Movement || !Gravity) { return; }

    const FOrbitGravitySample Sample = Gravity->SampleGravity(Character->GetActorLocation(), EOrbitGravityChannel::Character);
    if (!Sample.bHasInfluence && !bUseDefaultGravityOutsideFields) { return; }
    if (Sample.bHasValidDirection)
    {
        Movement->SetGravityDirection(Sample.Down);
    }
    // At a gravity null point there is no valid Up. Preserve last orientation rather than normalizing zero.
}
