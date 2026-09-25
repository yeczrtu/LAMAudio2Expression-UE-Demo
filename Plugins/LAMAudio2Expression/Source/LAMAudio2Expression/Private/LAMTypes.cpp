// SPDX-License-Identifier: Apache-2.0
// Adapted from aigc3d/LAM_Audio2Expression (02a703c3ea7d8e360eb43098eca85ee98a083529).
// Modified 2026: UE C++ runtime integration, deterministic timing and optional postprocessing.
// See ../../../Licenses/Apache-2.0.txt and ../../../THIRD_PARTY_NOTICES.md.
#include "LAMTypes.h"
const TArray<FName> &LAM::CurveNames()
{
    static const TArray<FName> Names = {
        TEXT("browDownLeft"),      TEXT("browDownRight"),      TEXT("browInnerUp"),         TEXT("browOuterUpLeft"),
        TEXT("browOuterUpRight"),  TEXT("cheekPuff"),          TEXT("cheekSquintLeft"),     TEXT("cheekSquintRight"),
        TEXT("eyeBlinkLeft"),      TEXT("eyeBlinkRight"),      TEXT("eyeLookDownLeft"),     TEXT("eyeLookDownRight"),
        TEXT("eyeLookInLeft"),     TEXT("eyeLookInRight"),     TEXT("eyeLookOutLeft"),      TEXT("eyeLookOutRight"),
        TEXT("eyeLookUpLeft"),     TEXT("eyeLookUpRight"),     TEXT("eyeSquintLeft"),       TEXT("eyeSquintRight"),
        TEXT("eyeWideLeft"),       TEXT("eyeWideRight"),       TEXT("jawForward"),          TEXT("jawLeft"),
        TEXT("jawOpen"),           TEXT("jawRight"),           TEXT("mouthClose"),          TEXT("mouthDimpleLeft"),
        TEXT("mouthDimpleRight"),  TEXT("mouthFrownLeft"),     TEXT("mouthFrownRight"),     TEXT("mouthFunnel"),
        TEXT("mouthLeft"),         TEXT("mouthLowerDownLeft"), TEXT("mouthLowerDownRight"), TEXT("mouthPressLeft"),
        TEXT("mouthPressRight"),   TEXT("mouthPucker"),        TEXT("mouthRight"),          TEXT("mouthRollLower"),
        TEXT("mouthRollUpper"),    TEXT("mouthShrugLower"),    TEXT("mouthShrugUpper"),     TEXT("mouthSmileLeft"),
        TEXT("mouthSmileRight"),   TEXT("mouthStretchLeft"),   TEXT("mouthStretchRight"),   TEXT("mouthUpperUpLeft"),
        TEXT("mouthUpperUpRight"), TEXT("noseSneerLeft"),      TEXT("noseSneerRight"),      TEXT("tongueOut")};
    return Names;
}
FLAMExpressionFrame ULAMExpressionClip::Sample(float Time) const
{
    FLAMExpressionFrame R;
    const int32 Count = Curves.Num() / LAM::CurveCount;
    if (Count == 0 || !FMath::IsFinite(Time))
        return R;
    R.TimeSeconds = FMath::Clamp(Time, 0.f, Duration);
    R.bValid = true;
    R.Weight = 1;
    R.Values.SetNumUninitialized(52);
    const float Position = FMath::Min(R.TimeSeconds * FrameRate, float(Count - 1));
    const int32 A = FMath::FloorToInt(Position), B = FMath::Min(A + 1, Count - 1);
    for (int32 C = 0; C < 52; ++C)
        R.Values[C] = FMath::Lerp(Curves[A * 52 + C], Curves[B * 52 + C], Position - A);
    return R;
}
