#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorAssetUtils.generated.h"


UCLASS()
class ASSETVAULT_API UEditorAssetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
	public:
	
	UFUNCTION(BlueprintCallable, Category = "Editor Utility")
	static void AddUniqueAssets_ByPath(UPARAM(ref) TArray<UObject*>& ExistingAssets,const TArray<UObject*>& SelectedAssets,TArray<UObject*>& NewlyAdded);

	
};