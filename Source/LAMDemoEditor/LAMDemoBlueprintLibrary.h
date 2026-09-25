// SPDX-License-Identifier: MIT
#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LAMDemoBlueprintLibrary.generated.h"

/** Development-only asset authoring. Generated graphs use standard UE and public LAM nodes. */
UCLASS()
class ULAMDemoBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
  public:
    /** Explicitly rebuilds the shipped demo graphs. Do not run on a customized copy. */
    UFUNCTION(BlueprintCallable, Category = "LAM Demo|Editor")
    static bool RebuildFaceDemoBlueprints();
    UFUNCTION(BlueprintCallable, Category = "LAM Demo|Editor")
    static bool ValidateFaceDemoBlueprints();
};
