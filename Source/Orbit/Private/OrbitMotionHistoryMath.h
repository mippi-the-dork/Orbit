#pragma once

#include "CoreMinimal.h"

/** Helpers shared by the runtime recorder and lightweight automation tests.
 *  A reversal is tangential motion changing sign, NOT a change of the radial fall velocity. */
namespace OrbitMotionHistoryMath
{
    inline FVector Tangent(const FVector& Velocity, const FVector& GravityDown)
    {
        return GravityDown.IsNearlyZero() ? Velocity : FVector::VectorPlaneProject(Velocity, GravityDown.GetSafeNormal());
    }

    inline bool IsTangentialReversal(const FVector& PreviousVelocity, const FVector& CurrentVelocity,
        const FVector& GravityDown, double MinimumSpeedCmS = 1.0)
    {
        const FVector A = Tangent(PreviousVelocity, GravityDown);
        const FVector B = Tangent(CurrentVelocity, GravityDown);
        return A.SizeSquared() >= FMath::Square(MinimumSpeedCmS) &&
            B.SizeSquared() >= FMath::Square(MinimumSpeedCmS) && FVector::DotProduct(A, B) < 0.0;
    }

    // Computed from live physics velocity, never from the last awake diagnostic snapshot.
    inline double CurrentTangentialSpeed(const FVector& Velocity, const FVector& GravityAcceleration)
    {
        return Tangent(Velocity, GravityAcceleration).Size();
    }

    inline bool IsFirstVerifiedSupport(bool bAlreadySeen, bool bFreshSupported)
    {
        return !bAlreadySeen && bFreshSupported;
    }

    inline bool IsSleepTransition(bool bHadPrevious, bool bPreviouslyAwake, bool bCurrentlyAwake)
    {
        return bHadPrevious && bPreviouslyAwake && !bCurrentlyAwake;
    }

    inline bool IsFirstSleepTransition(bool bAlreadySeen, bool bHadPrevious,
        bool bPreviouslyAwake, bool bCurrentlyAwake)
    {
        return !bAlreadySeen && IsSleepTransition(bHadPrevious, bPreviouslyAwake, bCurrentlyAwake);
    }

    // Describe why the rest sweep was skipped or what the sweep actually observed.
    // Does not change the criteria for supporting, damping or sleeping a body.
    inline const TCHAR* ProbeOutcome(bool bAwake, bool bEligible, bool bGentle,
        bool bFreshSupport, bool bGraceSupport, int32 BlockingCandidates, double Alignment)
    {
        if (!bAwake && !bEligible) { return TEXT("Sleeping (no new probe)"); }
        if (!bEligible) { return TEXT("Ineligible"); }
        if (!bGentle) { return TEXT("Normal impact too fast"); }
        if (bFreshSupport) { return Alignment < 0.90 ? TEXT("Verified (normal mismatch)") : TEXT("Verified"); }
        if (bGraceSupport) { return TEXT("Query miss (grace retained)"); }
        return BlockingCandidates > 0 ? TEXT("Blocking hit rejected by normal") : TEXT("No qualifying blocking hit");
    }

    inline FString CsvCell(FString Value)
    {
        Value.ReplaceInline(TEXT("\""), TEXT("\"\""));
        return FString::Printf(TEXT("\"%s\""), *Value);
    }

    inline FString SafeExportName(const FString& Input)
    {
        FString Result;
        for (TCHAR C : Input)
        {
            if ((C >= TEXT('a') && C <= TEXT('z')) ||
                (C >= TEXT('A') && C <= TEXT('Z')) ||
                (C >= TEXT('0') && C <= TEXT('9')) || C == TEXT('_') || C == TEXT('-'))
            {
                Result.AppendChar(C);
            }
            else
            {
                Result.AppendChar(TEXT('_'));
            }
        }
        Result = Result.Left(64);
        return Result.IsEmpty() ? TEXT("Capture") : Result;
    }

    // Hard safety limits for bulk recording. Limits affect diagnostics only, never physics.
    constexpr int32 MaxCapturedBodies = 256;
    constexpr int32 MaxBulkRows = 250000;

    inline bool ShouldCaptureSample(double Now, double Last, int32 RateHz)
    {
        return Last < 0.0 || Now - Last + 1e-6 >= 1.0 / FMath::Max(1, RateHz);
    }

    inline int32 BulkRowLimit(int32 BodyCount, double DurationSeconds, int32 SamplesPerSecond)
    {
        const int64 Estimated = static_cast<int64>(FMath::Max(0, BodyCount)) *
            static_cast<int64>(FMath::Clamp(FMath::CeilToInt(DurationSeconds * SamplesPerSecond) + 2, 1, 15002));
        return static_cast<int32>(FMath::Min<int64>(Estimated, MaxBulkRows));
    }

    inline int32 SampleLimit(double DurationSeconds, int32 SamplesPerSecond)
    {
        return FMath::Clamp(FMath::CeilToInt(DurationSeconds * SamplesPerSecond) + 2, 1, 15002);
    }
}
