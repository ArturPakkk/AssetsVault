#include "EditorAssetUtils.h"
#include "EditorUtilityLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"


void UEditorAssetUtils::AddUniqueAssets_ByPath(TArray<UObject*>& ExistingAssets,const TArray<UObject*>& SelectedAssets,TArray<UObject*>& NewlyAdded)
{
	NewlyAdded.Empty();

	TSet<FString> ExistingPaths;
	for (UObject* Existing : ExistingAssets)
	{
		if (Existing)
		{
			ExistingPaths.Add(Existing->GetPathName());
		}
	}

	for (UObject* Asset : SelectedAssets)
	{
		if (!Asset) continue;

		const FString AssetPath = Asset->GetPathName();
		if (!ExistingPaths.Contains(AssetPath))
		{
			ExistingAssets.Add(Asset);
			NewlyAdded.Add(Asset);

			UE_LOG(LogTemp, Log, TEXT("Added by path: %s"), *AssetPath);
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("Duplicate skipped: %s"), *AssetPath);
		}
	}
}
