#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "LAMTypes.h"
#include "AnimNode_LAMARKit.generated.h"
USTRUCT(BlueprintInternalUseOnly)
struct LAMAUDIO2EXPRESSION_API FAnimNode_LAMARKit : public FAnimNode_Base
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links") FPoseLink SourcePose;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM", meta = (PinShownByDefault))
    TObjectPtr<class ULAMAudio2ExpressionComponent> SourceComponent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM", meta = (PinShownByDefault)) float Alpha = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") TObjectPtr<ULAMCurveProfile> CurveProfile;
    void Initialize_AnyThread(const FAnimationInitializeContext &Context) override;
    void CacheBones_AnyThread(const FAnimationCacheBonesContext &Context) override;
    void Update_AnyThread(const FAnimationUpdateContext &Context) override;
    void Evaluate_AnyThread(FPoseContext &Output) override;
    bool HasPreUpdate() const override
    {
        return true;
    }
    void PreUpdate(const UAnimInstance *Instance) override;

  private:
    FLAMExpressionFrame Snapshot;
    TArray<FName> Names;
    TArray<float> Scales, Offsets;
    bool bSourceAvailable = false;
};
