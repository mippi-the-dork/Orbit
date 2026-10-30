#include "OrbitWorldSubsystem.h"

#include "OrbitMotionHistoryMath.h"
#include "OrbitSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "Containers/StringConv.h"
#include "Serialization/Archive.h"

DEFINE_LOG_CATEGORY_STATIC(LogOrbitMotionHistory, Log, All);

FString UOrbitWorldSubsystem::DescribeCollisionGeometry(const UPrimitiveComponent* Component)
{
    if (!IsValid(Component)) { return TEXT("Unavailable"); }
    FString Description = FString::Printf(TEXT("%s profile=%s collision=%d"),
        *Component->GetClass()->GetName(), *Component->GetCollisionProfileName().ToString(),
        static_cast<int32>(Component->GetCollisionEnabled()));
    const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
    UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
    const UBodySetup* Setup = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (Setup)
    {
        const FKAggregateGeom& G = Setup->AggGeom;
        Description += FString::Printf(TEXT(" trace_flag=%d simple_spheres=%d boxes=%d capsules=%d convex=%d tapered_capsules=%d"),
            static_cast<int32>(Setup->CollisionTraceFlag), G.SphereElems.Num(), G.BoxElems.Num(),
            G.SphylElems.Num(), G.ConvexElems.Num(), G.TaperedCapsuleElems.Num());
    }
    else
    {
        Description += TEXT(" simple_geometry=unknown");
    }
    return Description;
}

namespace
{
    FString FriendlyActorLabel(const AActor* Owner)
    {
#if WITH_EDITOR
        return IsValid(Owner) ? Owner->GetActorLabel() : TEXT("none");
#else
        return GetNameSafe(Owner);
#endif
    }

    FString MeshAssetName(const UPrimitiveComponent* Component)
    {
        const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
        return MeshComponent && MeshComponent->GetStaticMesh()
            ? MeshComponent->GetStaticMesh()->GetPathName() : FString();
    }
}

TArray<FString> UOrbitWorldSubsystem::ListMotionCaptureBodies() const
{
    TArray<FString> List;
    for (const FManagedBody& Body : ManagedBodies)
    {
        const UPrimitiveComponent* Component = Body.Component.Get();
        if (!IsValid(Component)) { continue; }
        const AActor* Owner = Component->GetOwner();
        List.Add(FString::Printf(TEXT("%s : %s (%s) [label=%s; mesh=%s]"),
            *GetNameSafe(Owner), *Component->GetName(), *Component->GetPathName(),
            *FriendlyActorLabel(Owner), *MeshAssetName(Component)));
    }
    List.Sort();
    return List;
}

bool UOrbitWorldSubsystem::BeginMotionHistory(const TArray<UPrimitiveComponent*>& Targets,
    bool bAllBodies, double DurationSeconds, int32 SamplesPerSecond)
{
    if (Targets.IsEmpty() || Targets.Num() > OrbitMotionHistoryMath::MaxCapturedBodies)
    {
        UE_LOG(LogOrbitMotionHistory, Warning,
            TEXT("Motion recording needs 1-%d bodies (found %d). Refusing a partial capture."),
            OrbitMotionHistoryMath::MaxCapturedBodies, Targets.Num());
        return false;
    }
    if (!MotionHistoryRows.IsEmpty() && !bMotionHistoryExported)
    {
        if (!ExportMotionHistory(TEXT("OrbitMotion_AutoReplaced")))
        {
            UE_LOG(LogOrbitMotionHistory, Warning,
                TEXT("Previous unsaved capture could not be exported; recording NOT replaced."));
            return false;
        }
    }
    MotionHistoryBodies.Reset();
    MotionHistoryBodyLookup.Reset();
    MotionHistoryRows.Reset();
    bMotionHistoryAllBodies = bAllBodies;
    MotionHistoryDuration = FMath::Clamp(FMath::IsFinite(DurationSeconds) ? DurationSeconds : 20.0, 0.25, 120.0);
    MotionHistoryRateHz = FMath::Clamp(SamplesPerSecond, 1, 120);
    MotionHistoryRowLimit = bAllBodies ? OrbitMotionHistoryMath::BulkRowLimit(
        Targets.Num(), MotionHistoryDuration, MotionHistoryRateHz) :
        OrbitMotionHistoryMath::SampleLimit(MotionHistoryDuration, MotionHistoryRateHz);
    for (UPrimitiveComponent* Component : Targets)
    {
        if (!IsValid(Component) || MotionHistoryBodyLookup.Contains(Component)) { continue; }
        FMotionHistoryBody& Entry = MotionHistoryBodies.AddDefaulted_GetRef();
        Entry.Component = Component;
        Entry.Path = Component->GetPathName();
        Entry.OwnerName = GetNameSafe(Component->GetOwner());
        Entry.OwnerLabel = FriendlyActorLabel(Component->GetOwner());
        Entry.MeshAssetPath = MeshAssetName(Component);
        Entry.CollisionGeometry = DescribeCollisionGeometry(Component);
        Entry.InitialPosition = Component->GetComponentLocation();
        Entry.bInitiallyAwake = Component->IsAnyRigidBodyAwake();
        Entry.InitialLinearSpeed = Component->GetPhysicsLinearVelocity().Size();
        Entry.InitialAngularSpeed = Component->GetPhysicsAngularVelocityInRadians().Size();
        MotionHistoryBodyLookup.Add(Component, MotionHistoryBodies.Num() - 1);
    }
    if (MotionHistoryBodies.IsEmpty()) { return false; }
    MotionHistoryRows.Reserve(FMath::Min(MotionHistoryRowLimit, 50000));
    MotionHistoryStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    MotionHistoryReversalCount = 0;
    MotionHistoryLastStopReason.Reset();
    bMotionHistoryRecording = true;
    bMotionHistoryExported = false;
    UE_LOG(LogOrbitMotionHistory, Display,
        TEXT("Recording %d Orbit physics bodies (%s) for up to %.1fs at up to %d Hz; row limit=%d. Export with Orbit.Motion.Export."),
        MotionHistoryBodies.Num(), bMotionHistoryAllBodies ? TEXT("all managed at start") : TEXT("selected"),
        MotionHistoryDuration, MotionHistoryRateHz, MotionHistoryRowLimit);
    return true;
}

bool UOrbitWorldSubsystem::StartAllMotionHistory(double DurationSeconds, int32 SamplesPerSecond)
{
    TArray<UPrimitiveComponent*> Targets;
    for (const FManagedBody& Body : ManagedBodies)
    {
        UPrimitiveComponent* Component = Body.Component.Get();
        if (IsValid(Component) && Component->IsRegistered() && Component->IsSimulatingPhysics())
        {
            Targets.AddUnique(Component);
        }
    }
    Targets.Sort([](const UPrimitiveComponent& A, const UPrimitiveComponent& B)
    {
        return A.GetPathName() < B.GetPathName();
    });
    return BeginMotionHistory(Targets, true, DurationSeconds, SamplesPerSecond);
}

bool UOrbitWorldSubsystem::StartMotionHistory(const FString& BodyName, double DurationSeconds, int32 SamplesPerSecond)
{
    if (BodyName.IsEmpty())
    {
        UE_LOG(LogOrbitMotionHistory, Warning, TEXT("Specify a unique actor/component name or use Orbit.Motion.StartAll. Run Orbit.Motion.List."));
        return false;
    }
    UPrimitiveComponent* Selected = nullptr;
    int32 Matches = 0;
    // Exact actor/component/path matches win over substring matches.
    for (int32 Pass = 0; Pass != 2; ++Pass)
    {
        Selected = nullptr;
        Matches = 0;
        for (const FManagedBody& Body : ManagedBodies)
        {
            UPrimitiveComponent* Component = Body.Component.Get();
            if (!IsValid(Component)) { continue; }
            const AActor* Owner = Component->GetOwner();
            const FString OwnerName = GetNameSafe(Owner);
            const FString ComponentName = Component->GetName();
            const FString PathName = Component->GetPathName();
            const bool bMatched = Pass == 0 ?
                (OwnerName.Equals(BodyName, ESearchCase::IgnoreCase) ||
                 ComponentName.Equals(BodyName, ESearchCase::IgnoreCase) ||
                 PathName.Equals(BodyName, ESearchCase::IgnoreCase)) :
                (OwnerName.Contains(BodyName, ESearchCase::IgnoreCase) ||
                 ComponentName.Contains(BodyName, ESearchCase::IgnoreCase) ||
                 PathName.Contains(BodyName, ESearchCase::IgnoreCase));
            if (bMatched) { Selected = Component; ++Matches; }
        }
        if (Matches > 0) { break; }
    }
    if (Matches != 1 || !Selected)
    {
        UE_LOG(LogOrbitMotionHistory, Warning,
            TEXT("Orbit.Motion.Start: %s matched %d managed bodies (requires exactly one). Use Orbit.Motion.StartAll for all bodies."),
            *BodyName, Matches);
        return false;
    }
    TArray<UPrimitiveComponent*> OneTarget;
    OneTarget.Add(Selected);
    return BeginMotionHistory(OneTarget, false, DurationSeconds, SamplesPerSecond);
}

void UOrbitWorldSubsystem::FinishMotionHistory(const FString& Reason)
{
    if (!bMotionHistoryRecording) { return; }
    bMotionHistoryRecording = false;
    MotionHistoryLastStopReason = Reason;
    UE_LOG(LogOrbitMotionHistory, Display,
        TEXT("Orbit motion capture stopped: %s. %d rows, %d tangential reversals. Run Orbit.Motion.Export before stopping PIE."),
        *Reason, MotionHistoryRows.Num(), MotionHistoryReversalCount);
}

void UOrbitWorldSubsystem::StopMotionHistory()
{
    FinishMotionHistory(TEXT("Manual stop"));
}

void UOrbitWorldSubsystem::CaptureMotionHistory(UPrimitiveComponent* Component, const FManagedBody& Body)
{
    if (!bMotionHistoryRecording || !IsValid(Component) || !GetWorld()) { return; }
    const int32* FoundIndex = MotionHistoryBodyLookup.Find(Component);
    if (!FoundIndex || !MotionHistoryBodies.IsValidIndex(*FoundIndex)) { return; }
    FMotionHistoryBody& CaptureBody = MotionHistoryBodies[*FoundIndex];
    if (CaptureBody.Component.Get() != Component) { return; }
    const double Now = GetWorld()->GetTimeSeconds();
    if (!OrbitMotionHistoryMath::ShouldCaptureSample(Now, CaptureBody.LastSampleTime, MotionHistoryRateHz))
    {
        return;
    }
    if (MotionHistoryRows.Num() >= MotionHistoryRowLimit)
    {
        FinishMotionHistory(TEXT("Global row budget reached"));
        return;
    }
    const double Elapsed = FMath::Max(0.0, Now - MotionHistoryStartTime);
    FMotionHistoryRow& Row = MotionHistoryRows.AddDefaulted_GetRef();
    Row.BodyIndex = *FoundIndex;
    bMotionHistoryExported = false;
    Row.TimeSeconds = Elapsed;
    Row.Position = Component->GetComponentLocation();
    Row.CenterOfMass = Component->GetCenterOfMass(NAME_None);
    const FOrbitFieldDefinition* Field = ActiveFields.FindByPredicate(
        [&Body](const FOrbitFieldDefinition& Candidate)
        { return Candidate.SourceId == Body.LastDiagnostics.DominantSourceId; });
    Row.bHasDominantSourceCenter = Field != nullptr;
    if (Field) { Row.DominantSourceCenter = Field->Center; }
    Row.Rotation = Component->GetComponentQuat();
    Row.LinearVelocity = Component->GetPhysicsLinearVelocity();
    Row.AngularVelocity = Component->GetPhysicsAngularVelocityInRadians();
    Row.Diagnostics = Body.LastDiagnostics;
    Row.PieTimeSeconds = Now;
    // Always read the current Chaos velocities, including when the body is sleeping.
    // LastDiagnostics is updated once per Orbit tick, but it must never be a source
    // of stale speed numbers for a persisted sample.
    Row.Diagnostics.TangentialSpeed = OrbitMotionHistoryMath::CurrentTangentialSpeed(
        Row.LinearVelocity, Row.Diagnostics.GravityAcceleration);
    Row.Diagnostics.AngularSpeed = Row.AngularVelocity.Size();
    Row.Diagnostics.bBodyAwake = Component->IsAnyRigidBodyAwake();
    Row.EffectiveLinearDamping = Component->GetLinearDamping();
    Row.EffectiveAngularDamping = Component->GetAngularDamping();
    const UOrbitSettings* CaptureSettings = GetDefault<UOrbitSettings>();
    Row.bConfiguredSupportedDamping = CaptureSettings->bDampSupportedRigidBodyMotion;
    Row.bConfiguredRestSleep = CaptureSettings->bStabilizeRestingRigidBodies;
    Row.ConfiguredLinearDamping = CaptureSettings->SupportedTangentialDamping;
    Row.ConfiguredAngularDamping = CaptureSettings->SupportedAngularDamping;
    Row.ConfiguredProbeDistance = CaptureSettings->RestSupportProbeDistance;
    Row.ConfiguredMaxContactSpeed = CaptureSettings->RestMaxNormalContactSpeed;
    Row.bFreshSupport = Body.bLastFreshSupport;
    Row.bGraceSupport = Body.bLastGraceSupport;
    Row.bRestCandidate = Body.bLastRestCandidate;
    Row.bGentleContact = Body.bLastGentleContact;
    Row.bProbeAttempted = Body.bLastProbeAttempted;
    Row.ProbeBlockingCandidates = Body.LastProbe.BlockingCandidates;
    Row.ProbeRejectedNormals = Body.LastProbe.RejectedNormals;
    Row.BestRejectedAlignment = Body.LastProbe.BestRejectedAlignment;
    Row.BestRejectedNormal = Body.LastProbe.BestRejectedNormal;
    Row.BestRejectedImpact = Body.LastProbe.BestRejectedImpact;
    Row.bBestRejectedStartedPenetrating = Body.LastProbe.bBestRejectedStartedPenetrating;
    const UPrimitiveComponent* Rejected = Body.LastProbe.BestRejectedComponent.Get();
    Row.BestRejectedComponentName = GetPathNameSafe(Rejected);
    if (IsValid(Rejected)) { Row.BestRejectedGeometry = DescribeCollisionGeometry(Rejected); }
    Row.NormalSpeedCmS = Body.LastNormalSpeedCmS;
    Row.GraceRemainingSeconds = Body.LastProbeGraceSeconds;
    Row.ProbeOutcome = OrbitMotionHistoryMath::ProbeOutcome(Row.Diagnostics.bBodyAwake,
        Row.bRestCandidate, Row.bGentleContact, Row.bFreshSupport, Row.bGraceSupport,
        Row.ProbeBlockingCandidates, Row.Diagnostics.SupportUpNormalAlignment);
    Row.bFirstVerifiedSupport = OrbitMotionHistoryMath::IsFirstVerifiedSupport(
        CaptureBody.bSeenFreshSupport, Row.bFreshSupport);
    if (Row.bFirstVerifiedSupport)
    {
        CaptureBody.bSeenFreshSupport = true;
        CaptureBody.FirstVerifiedSupportPIETime = Now;
    }
    Row.bFirstSleepTransition = OrbitMotionHistoryMath::IsFirstSleepTransition(
        CaptureBody.FirstSleepTransitionPIETime >= 0.0, CaptureBody.bHasPreviousAwake,
        CaptureBody.bPreviousAwake, Row.Diagnostics.bBodyAwake);
    if (Row.bFirstSleepTransition && CaptureBody.FirstSleepTransitionPIETime < 0.0)
    {
        CaptureBody.FirstSleepTransitionPIETime = Now;
    }
    Row.FirstVerifiedSupportPIETime = CaptureBody.FirstVerifiedSupportPIETime;
    Row.FirstSleepTransitionPIETime = CaptureBody.FirstSleepTransitionPIETime;
    Row.bAwakeStateChanged = CaptureBody.bHasPreviousAwake &&
        CaptureBody.bPreviousAwake != Row.Diagnostics.bBodyAwake;
    Row.bSupportStateChanged = CaptureBody.bHasPreviousSupport &&
        CaptureBody.PreviousSupportState != Row.Diagnostics.SupportState;
    CaptureBody.bHasPreviousAwake = true;
    CaptureBody.bPreviousAwake = Row.Diagnostics.bBodyAwake;
    CaptureBody.bHasPreviousSupport = true;
    CaptureBody.PreviousSupportState = Row.Diagnostics.SupportState;
    if (Row.Diagnostics.bHasSupportNormal)
    {
        Row.SupportNormal = Body.LastSupportNormal;
        Row.SupportImpactPoint = Body.LastSupportImpactPoint;
        Row.bSupportStartedPenetrating = Body.bLastSupportStartedPenetrating;
        const UPrimitiveComponent* Support = Body.LastSupportComponent.Get();
        Row.SupportComponentName = GetPathNameSafe(Support);
        Row.SupportCollisionGeometry = DescribeCollisionGeometry(Support);
    }
    if (CaptureBody.bHasPreviousVelocity)
    {
        Row.bReversedTangentialMotion = OrbitMotionHistoryMath::IsTangentialReversal(
            CaptureBody.PreviousVelocity, Row.LinearVelocity, Row.Diagnostics.GravityAcceleration.GetSafeNormal());
        if (Row.bReversedTangentialMotion)
        {
            ++MotionHistoryReversalCount;
            ++CaptureBody.ReversalCount;
        }
    }
    CaptureBody.PreviousVelocity = Row.LinearVelocity;
    CaptureBody.bHasPreviousVelocity = true;
    CaptureBody.LastSampleTime = Now;
    ++CaptureBody.SampleCount;
    if (MotionHistoryRows.Num() >= MotionHistoryRowLimit)
    {
        FinishMotionHistory(TEXT("Global row budget reached"));
    }
}

FString UOrbitWorldSubsystem::GetMotionHistoryStatus() const
{
    int32 SampledBodies = 0;
    int32 MissingBodies = 0;
    for (const FMotionHistoryBody& Entry : MotionHistoryBodies)
    {
        if (Entry.SampleCount > 0) { ++SampledBodies; }
        if (!Entry.Component.IsValid() || !HasManagedBody(Entry.Component.Get())) { ++MissingBodies; }
    }
    return FString::Printf(TEXT("Orbit motion capture: %s; mode=%s; bodies=%d; sampled=%d; unavailable=%d; rows=%d/%d; reversals=%d; target=%.1fs/%dHz; stop=%s"),
        bMotionHistoryRecording ? TEXT("RECORDING") : TEXT("STOPPED"),
        bMotionHistoryAllBodies ? TEXT("ALL") : TEXT("SINGLE"),
        MotionHistoryBodies.Num(), SampledBodies, MissingBodies,
        MotionHistoryRows.Num(), MotionHistoryRowLimit, MotionHistoryReversalCount,
        MotionHistoryDuration, MotionHistoryRateHz,
        MotionHistoryLastStopReason.IsEmpty() ? TEXT("none") : *MotionHistoryLastStopReason);
}

bool UOrbitWorldSubsystem::ExportMotionHistory(const FString& Label, FString* OutPath) const
{
    if (MotionHistoryRows.IsEmpty())
    {
        UE_LOG(LogOrbitMotionHistory, Warning, TEXT("Nothing to export. Run Orbit.Motion.Start first."));
        return false;
    }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Orbit"), TEXT("MotionHistory"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true))
    {
        UE_LOG(LogOrbitMotionHistory, Error, TEXT("Unable to create output folder: %s"), *Directory);
        return false;
    }
    const FString Base = OrbitMotionHistoryMath::SafeExportName(Label.IsEmpty() ? TEXT("OrbitMotion") : Label);
    const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    const FString Filename = FString::Printf(TEXT("%s_%s_%s.csv"),
        *Base, *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), *Unique);
    const FString FullPath = FPaths::Combine(Directory, Filename);

    // Stream batches to disk rather than constructing an enormous multi-body CSV in RAM.
    TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*FullPath));
    if (!Writer)
    {
        UE_LOG(LogOrbitMotionHistory, Error, TEXT("Unable to open CSV: %s"), *FullPath);
        return false;
    }
    auto WriteUtf8 = [&Writer](const FString& Text)
    {
        FTCHARToUTF8 Utf8(*Text);
        Writer->Serialize((void*)Utf8.Get(), Utf8.Length());
    };
    WriteUtf8(TEXT("body,body_geometry,t_sec,position_x,position_y,position_z,com_x,com_y,com_z,source_center_valid,source_center_x,source_center_y,source_center_z,quat_x,quat_y,quat_z,quat_w,velocity_x,velocity_y,velocity_z,angular_rad_x,angular_rad_y,angular_rad_z,gravity_x,gravity_y,gravity_z,gravity_source_id,support_state,support_name,support_geometry,support_normal_x,support_normal_y,support_normal_z,support_impact_x,support_impact_y,support_impact_z,support_start_penetrating,has_support_normal,up_normal_alignment,radial_alignment,has_radial_alignment,has_source_support_bounds_offset,source_support_bounds_offset_cm,native_gravity_enabled,using_native_damping,awake,tangential_speed,angular_speed,tangential_reversal,body_id,actor_name,actor_label,mesh_asset,capture_start_pie_sec,pie_time_sec,initial_x,initial_y,initial_z,initial_awake,initial_linear_speed,initial_angular_speed,first_verified_support_pie_sec,first_sleep_transition_pie_sec,first_verified_support_event,first_sleep_transition_event,support_state_changed,awake_state_changed,probe_outcome,rest_candidate,gentle_contact,probe_attempted,probe_blocking_hits,probe_rejected_normals,best_rejected_alignment,best_rejected_normal_x,best_rejected_normal_y,best_rejected_normal_z,best_rejected_impact_x,best_rejected_impact_y,best_rejected_impact_z,best_rejected_start_penetrating,best_rejected_component,best_rejected_geometry,normal_speed_cm_s,grace_remaining_sec,fresh_support,grace_support,effective_linear_damping,effective_angular_damping,config_supported_damping_enabled,config_rest_sleep_enabled,config_supported_linear_damping,config_supported_angular_damping,config_probe_distance_cm,config_max_contact_speed_cm_s\n"));
    FString CsvBatch;
    CsvBatch.Reserve(128 * 1024);
    for (const FMotionHistoryRow& Row : MotionHistoryRows)
    {
        if (!MotionHistoryBodies.IsValidIndex(Row.BodyIndex)) { continue; }
        const FMotionHistoryBody& CaptureBody = MotionHistoryBodies[Row.BodyIndex];
        const FString BodyColumn = OrbitMotionHistoryMath::CsvCell(CaptureBody.Path);
        const FString GeometryColumn = OrbitMotionHistoryMath::CsvCell(CaptureBody.CollisionGeometry);
        const FOrbitPhysicsBodyDiagnostics& D = Row.Diagnostics;
        CsvBatch += FString::Printf(TEXT("%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.9f,%.9f,%.9f,%.9f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f,%.6f,%d,%d,%.6f,%d,%d,%d,%.6f,%.6f,%d,%d,%s,%s,%s"),
            *BodyColumn, *GeometryColumn, Row.TimeSeconds,
            Row.Position.X, Row.Position.Y, Row.Position.Z,
            Row.CenterOfMass.X, Row.CenterOfMass.Y, Row.CenterOfMass.Z,
            static_cast<int32>(Row.bHasDominantSourceCenter),
            Row.DominantSourceCenter.X, Row.DominantSourceCenter.Y, Row.DominantSourceCenter.Z,
            Row.Rotation.X, Row.Rotation.Y, Row.Rotation.Z, Row.Rotation.W,
            Row.LinearVelocity.X, Row.LinearVelocity.Y, Row.LinearVelocity.Z,
            Row.AngularVelocity.X, Row.AngularVelocity.Y, Row.AngularVelocity.Z,
            D.GravityAcceleration.X, D.GravityAcceleration.Y, D.GravityAcceleration.Z,
            D.DominantSourceId,
            *OrbitMotionHistoryMath::CsvCell(D.SupportState),
            *OrbitMotionHistoryMath::CsvCell(Row.SupportComponentName),
            *OrbitMotionHistoryMath::CsvCell(Row.SupportCollisionGeometry),
            Row.SupportNormal.X, Row.SupportNormal.Y, Row.SupportNormal.Z,
            Row.SupportImpactPoint.X, Row.SupportImpactPoint.Y, Row.SupportImpactPoint.Z,
            static_cast<int32>(Row.bSupportStartedPenetrating), static_cast<int32>(D.bHasSupportNormal),
            D.SupportUpNormalAlignment, D.PointRadialAlignment,
            static_cast<int32>(D.bHasPointRadialAlignment),
            static_cast<int32>(D.bHasSourceToSupportBoundsCenter), D.SourceToSupportBoundsCenterCm,
            static_cast<int32>(D.bNativeGravityEnabled), static_cast<int32>(D.bUsingNativeContactDamping),
            static_cast<int32>(D.bBodyAwake), D.TangentialSpeed, D.AngularSpeed,
            static_cast<int32>(Row.bReversedTangentialMotion), Row.BodyIndex,
            *OrbitMotionHistoryMath::CsvCell(CaptureBody.OwnerName),
            *OrbitMotionHistoryMath::CsvCell(CaptureBody.OwnerLabel),
            *OrbitMotionHistoryMath::CsvCell(CaptureBody.MeshAssetPath));
        // New columns are appended to retain every v0.2.16 CSV index unchanged.
        CsvBatch += FString::Printf(
            TEXT(",%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d,%s,%d,%d,%d,%d,%d,%.6f,"),
            MotionHistoryStartTime, Row.PieTimeSeconds,
            CaptureBody.InitialPosition.X, CaptureBody.InitialPosition.Y, CaptureBody.InitialPosition.Z,
            static_cast<int32>(CaptureBody.bInitiallyAwake), CaptureBody.InitialLinearSpeed,
            CaptureBody.InitialAngularSpeed, Row.FirstVerifiedSupportPIETime, Row.FirstSleepTransitionPIETime,
            static_cast<int32>(Row.bFirstVerifiedSupport), static_cast<int32>(Row.bFirstSleepTransition),
            static_cast<int32>(Row.bSupportStateChanged), static_cast<int32>(Row.bAwakeStateChanged),
            *OrbitMotionHistoryMath::CsvCell(Row.ProbeOutcome),
            static_cast<int32>(Row.bRestCandidate), static_cast<int32>(Row.bGentleContact),
            static_cast<int32>(Row.bProbeAttempted), Row.ProbeBlockingCandidates, Row.ProbeRejectedNormals,
            Row.BestRejectedAlignment);
        CsvBatch += FString::Printf(
            TEXT("%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%s,%s,%.6f,%.6f,%d,%d,%.6f,%.6f,%d,%d,%.6f,%.6f,%.6f,%.6f\n"),
            Row.BestRejectedNormal.X, Row.BestRejectedNormal.Y, Row.BestRejectedNormal.Z,
            Row.BestRejectedImpact.X, Row.BestRejectedImpact.Y, Row.BestRejectedImpact.Z,
            static_cast<int32>(Row.bBestRejectedStartedPenetrating),
            *OrbitMotionHistoryMath::CsvCell(Row.BestRejectedComponentName),
            *OrbitMotionHistoryMath::CsvCell(Row.BestRejectedGeometry),
            Row.NormalSpeedCmS, static_cast<double>(Row.GraceRemainingSeconds),
            static_cast<int32>(Row.bFreshSupport), static_cast<int32>(Row.bGraceSupport),
            static_cast<double>(Row.EffectiveLinearDamping), static_cast<double>(Row.EffectiveAngularDamping),
            static_cast<int32>(Row.bConfiguredSupportedDamping),
            static_cast<int32>(Row.bConfiguredRestSleep),
            static_cast<double>(Row.ConfiguredLinearDamping),
            static_cast<double>(Row.ConfiguredAngularDamping),
            static_cast<double>(Row.ConfiguredProbeDistance),
            static_cast<double>(Row.ConfiguredMaxContactSpeed));
        if (CsvBatch.Len() >= 65536)
        {
            WriteUtf8(CsvBatch);
            CsvBatch.Reset();
            if (Writer->IsError()) { break; }
        }
    }
    if (!Writer->IsError() && !CsvBatch.IsEmpty()) { WriteUtf8(CsvBatch); }
    const bool bWriteFailed = Writer->IsError();
    Writer->Close();
    const bool bFailed = bWriteFailed || Writer->IsError();
    Writer.Reset();
    if (bFailed)
    {
        IFileManager::Get().Delete(*FullPath);
        UE_LOG(LogOrbitMotionHistory, Error, TEXT("Unable to save complete CSV: %s"), *FullPath);
        return false;
    }
    bMotionHistoryExported = true;
    if (OutPath) { *OutPath = FullPath; }
    UE_LOG(LogOrbitMotionHistory, Display, TEXT("Orbit motion history exported: %s (%d rows, %d reversals)"),
        *FullPath, MotionHistoryRows.Num(), MotionHistoryReversalCount);
    return true;
}

#if !UE_BUILD_SHIPPING
namespace OrbitMotionConsole
{
    static UOrbitWorldSubsystem* Resolve(UWorld* World)
    {
        if (!World || !World->IsGameWorld())
        {
            UE_LOG(LogOrbitMotionHistory, Warning, TEXT("Run Orbit.Motion commands from a PIE or game-world console."));
            return nullptr;
        }
        return World->GetSubsystem<UOrbitWorldSubsystem>();
    }

    static void List(const TArray<FString>& /*Args*/, UWorld* World)
    {
        if (const UOrbitWorldSubsystem* Sub = Resolve(World))
        {
            const TArray<FString> Bodies = Sub->ListMotionCaptureBodies();
            UE_LOG(LogOrbitMotionHistory, Display, TEXT("Orbit-managed physics bodies: %d"), Bodies.Num());
            for (const FString& Body : Bodies) { UE_LOG(LogOrbitMotionHistory, Display, TEXT("  %s"), *Body); }
        }
    }
    static void Start(const TArray<FString>& Args, UWorld* World)
    {
        if (UOrbitWorldSubsystem* Sub = Resolve(World))
        {
            if (Args.IsEmpty())
            {
                UE_LOG(LogOrbitMotionHistory, Warning,
                    TEXT("Usage: Orbit.Motion.Start <actor-or-component-name> [duration-seconds=20] [rate-hz=60]"));
                return;
            }
            Sub->StartMotionHistory(Args[0], Args.Num() > 1 ? FCString::Atod(*Args[1]) : 20.0,
                Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 60);
        }
    }
    static void StartAll(const TArray<FString>& Args, UWorld* World)
    {
        if (UOrbitWorldSubsystem* Sub = Resolve(World))
        {
            Sub->StartAllMotionHistory(Args.Num() > 0 ? FCString::Atod(*Args[0]) : 20.0,
                Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 60);
        }
    }
    static void Stop(const TArray<FString>& /*Args*/, UWorld* World)
    {
        if (UOrbitWorldSubsystem* Sub = Resolve(World)) { Sub->StopMotionHistory(); }
    }
    static void Export(const TArray<FString>& Args, UWorld* World)
    {
        if (const UOrbitWorldSubsystem* Sub = Resolve(World))
        {
            Sub->ExportMotionHistory(Args.IsEmpty() ? FString() : Args[0]);
        }
    }
    static void Status(const TArray<FString>& /*Args*/, UWorld* World)
    {
        if (const UOrbitWorldSubsystem* Sub = Resolve(World))
        {
            UE_LOG(LogOrbitMotionHistory, Display, TEXT("%s"), *Sub->GetMotionHistoryStatus());
        }
    }

    static FAutoConsoleCommandWithWorldAndArgs ListCommand(
        TEXT("Orbit.Motion.List"), TEXT("List managed Orbit physics objects available for motion recording."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&List));
    static FAutoConsoleCommandWithWorldAndArgs StartCommand(
        TEXT("Orbit.Motion.Start"), TEXT("Orbit.Motion.Start <name> [seconds=20] [Hz=60]. Target exactly one managed primitive."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
    static FAutoConsoleCommandWithWorldAndArgs StartAllCommand(
        TEXT("Orbit.Motion.StartAll"), TEXT("Orbit.Motion.StartAll [seconds=20] [Hz=60]. Record a bounded snapshot of ALL currently managed primitives."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&StartAll));
    static FAutoConsoleCommandWithWorldAndArgs StopCommand(
        TEXT("Orbit.Motion.Stop"), TEXT("Stop recording, retain captured rows for export."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stop));
    static FAutoConsoleCommandWithWorldAndArgs ExportCommand(
        TEXT("Orbit.Motion.Export"), TEXT("Export captured CSV in Project/Saved/Orbit/MotionHistory. Optional filename label."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Export));
    static FAutoConsoleCommandWithWorldAndArgs StatusCommand(
        TEXT("Orbit.Motion.Status"), TEXT("Show motion recording mode, body and sample counts, reversals, and limits."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));
}
#endif // !UE_BUILD_SHIPPING
