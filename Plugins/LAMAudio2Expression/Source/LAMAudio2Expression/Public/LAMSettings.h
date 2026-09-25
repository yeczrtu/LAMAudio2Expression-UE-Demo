#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "NNEModelData.h"
#include "LAMSettings.generated.h"
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "LAM Audio2Expression"))
class LAMAUDIO2EXPRESSION_API ULAMSettings : public UDeveloperSettings
{
    GENERATED_BODY()
  public:
    UPROPERTY(Config, EditAnywhere, Category = "Model")
    TSoftObjectPtr<UNNEModelData> Model =
        TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(TEXT("/LAMAudio2Expression/Models/LAM_A2E.LAM_A2E")));
    UPROPERTY(Config, EditAnywhere, Category = "Model") bool bPreferGPU = true;
    UPROPERTY(Config, EditAnywhere, Category = "Cache", meta = (ClampMin = "0", ClampMax = "1024")) int32 CacheMiB = 64;
};
