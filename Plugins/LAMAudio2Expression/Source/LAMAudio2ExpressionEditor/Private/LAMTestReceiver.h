#pragma once
#include "CoreMinimal.h"
#include "LAMTypes.h"
#include "LAMTestReceiver.generated.h"

// Editor automation receiver; never linked into packaged games.
UCLASS()
class ULAMTestReceiver : public UObject
{
    GENERATED_BODY()
  public:
    int32 Completions = 0, Failures = 0, Cancellations = 0;
    float LatestProgress = 0;
    UFUNCTION() void Completed(ULAMExpressionClip *Clip, float Value, FString Error)
    {
        ++Completions;
    }
    UFUNCTION() void Failed(ULAMExpressionClip *Clip, float Value, FString Error)
    {
        ++Failures;
    }
    UFUNCTION() void Cancelled(ULAMExpressionClip *Clip, float Value, FString Error)
    {
        ++Cancellations;
    }
    UFUNCTION() void Progress(ULAMExpressionClip *Clip, float Value, FString Error)
    {
        LatestProgress = Value;
    }
};
