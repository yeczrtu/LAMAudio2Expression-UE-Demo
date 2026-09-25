#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LAMEditorLibrary.generated.h"
UCLASS()
class LAMAUDIO2EXPRESSIONEDITOR_API ULAMEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
  public:
    UFUNCTION(BlueprintCallable, Category = "LAM|Editor") static bool CreateExamples();
};
