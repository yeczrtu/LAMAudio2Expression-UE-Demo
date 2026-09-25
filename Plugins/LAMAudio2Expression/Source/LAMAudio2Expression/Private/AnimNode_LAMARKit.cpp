#include "AnimNode_LAMARKit.h"
#include "LAMAudio2ExpressionComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"
void FAnimNode_LAMARKit::Initialize_AnyThread(const FAnimationInitializeContext &C)
{
    SourcePose.Initialize(C);
}
void FAnimNode_LAMARKit::CacheBones_AnyThread(const FAnimationCacheBonesContext &C)
{
    SourcePose.CacheBones(C);
}
void FAnimNode_LAMARKit::Update_AnyThread(const FAnimationUpdateContext &C)
{
    GetEvaluateGraphExposedInputs().Execute(C);
    SourcePose.Update(C);
    if (!bSourceAvailable)
    {
        Snapshot.Weight = FMath::Max(0.f, Snapshot.Weight - C.GetDeltaTime() / 0.1f);
        if (Snapshot.Weight == 0)
            Snapshot.bValid = false;
    }
}
void FAnimNode_LAMARKit::PreUpdate(const UAnimInstance *I)
{
    auto *Component = SourceComponent.Get();
    if (!Component && I->GetOwningActor())
        Component = I->GetOwningActor()->FindComponentByClass<ULAMAudio2ExpressionComponent>();
    bSourceAvailable = IsValid(Component);
    if (bSourceAvailable)
        Snapshot = Component->GetCurrentExpressionFrame();
    Names = LAM::CurveNames();
    Scales.Init(1, 52);
    Offsets.Init(0, 52);
    if (CurveProfile)
        for (const auto &Rule : CurveProfile->Rules)
        {
            const int Index = LAM::CurveNames().IndexOfByKey(Rule.SourceName);
            if (Index != INDEX_NONE)
            {
                Names[Index] =
                    Rule.bEnabled ? (Rule.TargetName.IsNone() ? Rule.SourceName : Rule.TargetName) : NAME_None;
                Scales[Index] = Rule.Scale;
                Offsets[Index] = Rule.Offset;
            }
        }
}
void FAnimNode_LAMARKit::Evaluate_AnyThread(FPoseContext &Out)
{
    SourcePose.Evaluate(Out);
    if (!Snapshot.bValid || Snapshot.Values.Num() != 52 || Names.Num() != 52)
        return;
    const float Weight = FMath::Clamp(Alpha * Snapshot.Weight, 0.f, 1.f);
    for (int I = 0; I < 52; ++I)
        if (!Names[I].IsNone())
        {
            const float Value = FMath::Clamp(Snapshot.Values[I] * Scales[I] + Offsets[I], 0.f, 1.f);
            Out.Curve.Set(Names[I], FMath::Lerp(Out.Curve.Get(Names[I]), Value, Weight));
        }
}
