#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LAMTypes.generated.h"

namespace LAM
{
constexpr int32 CurveCount = 52, Rate = 16000, FPS = 30, Window = 34133, WindowFrames = 64;
LAMAUDIO2EXPRESSION_API const TArray<FName> &CurveNames();
} // namespace LAM

USTRUCT(BlueprintType)
struct LAMAUDIO2EXPRESSION_API FLAMAnalysisSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM", meta = (ClampMin = "0", ClampMax = "11"))
    int32 Style = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") bool bSmooth = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") bool bSuppressSilentMouth = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") bool bSymmetrize = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") bool bAutoBlink = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") int32 BlinkSeed = 1234;
};

USTRUCT(BlueprintType)
struct LAMAUDIO2EXPRESSION_API FLAMExpressionFrame
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "LAM") float TimeSeconds = 0;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") TArray<float> Values;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") bool bValid = false;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") float Weight = 0;
};

USTRUCT(BlueprintType)
struct LAMAUDIO2EXPRESSION_API FLAMCurveRule
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") FName SourceName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") FName TargetName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") float Scale = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") float Offset = 0;
};

UCLASS(BlueprintType)
class LAMAUDIO2EXPRESSION_API ULAMCurveProfile : public UDataAsset
{
    GENERATED_BODY()
  public:
    // Missing rules retain their canonical ARKit name. Disabled rules leave the incoming curve alone.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LAM") TArray<FLAMCurveRule> Rules;
};

UCLASS(BlueprintType)
class LAMAUDIO2EXPRESSION_API ULAMExpressionClip : public UObject
{
    GENERATED_BODY()
  public:
    UPROPERTY(BlueprintReadOnly, Category = "LAM") TObjectPtr<class USoundWave> SoundWave;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") float Duration = 0;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") int32 FrameRate = 30;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") int32 NumSamples = 0;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") FLAMAnalysisSettings Settings;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") FString Backend;
    UPROPERTY(BlueprintReadOnly, Category = "LAM") float AnalysisSeconds = 0;
    UPROPERTY() TArray<float> Curves;
    UFUNCTION(BlueprintPure, Category = "LAM") FLAMExpressionFrame Sample(float TimeSeconds) const;
};
