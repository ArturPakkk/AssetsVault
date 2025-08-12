#pragma once

#include "CoreMinimal.h"
#include "AssetVaultTypes.h" // Подключаем файл с вашими структурами
#include "UObject/NoExportTypes.h"
#include "FAssetPackageManager.generated.h"

UCLASS()
class ASSETVAULT_API UAssetPackageManager : public UObject
{
	GENERATED_BODY()

public:
	
	
	UFUNCTION(BlueprintPure, Category = "AssetVault|Enums")
	static int32 GetAssetTypeMaxIndex();
	
	static FString BuildExportPath(const FString& RootPath, const FAssetMainInfo& MainInfo);
	
	
	UFUNCTION(BlueprintCallable, Category = "Vault|Export")static bool ExportAssetToPackage(UObject* Asset,const FString& ExportDirectory,FAssetExportOptions ExportOptions);
	
	UFUNCTION(BlueprintCallable, Category = "AssetVault|Export")
	static TArray<FAssetExportOptions> LoadAllAssetDataFromDirectory(const FString& DirectoryPath);
	
	
	UFUNCTION(BlueprintCallable, Category = "Asset Manager")
	static TArray<FAssetExportOptions> FilterAndSortAssets(const TArray<FAssetExportOptions>& Assets,EAssetType FilterType);

	UFUNCTION(BlueprintCallable, Category="Utilities|File")
	static bool OpenFolderDialog(FString& OutFolderPath);
	
	UFUNCTION(BlueprintCallable, Category = "Asset Vault")
	static void ShowEditorNotification(const FString& Message, bool bSuccess);

	
	UFUNCTION(BlueprintCallable, Category = "Asset Management")
	static void OpenFolderInExplorer(const FString& BaseDirectory, FString RelativeExportPath);

	
	UFUNCTION(BlueprintCallable, Category = "Vault|Import")
	static bool ImportAssetFolderToProject(const FString& DefaultDirectory,const FString& RelativeExportPath,const FString& TargetSubfolder,bool bForceOverwrite);
	
	UFUNCTION(BlueprintCallable, Category = "Vault|Import")
	static bool DoesAssetAlreadyExist(const FString& DefaultDirectory,const FString& RelativeExportPath,const FString& TargetSubfolder,UPARAM(ref) TArray<FString>& OutConflictingAssets);

	UFUNCTION(BlueprintCallable, Category = "Vault")
	static bool DoesExportPathAlreadyContainAssets(const FString& ExportDirectory, const FAssetExportOptions& Options, FString& OutResolvedPath);

	UFUNCTION(BlueprintCallable, Category = "Asset Export")
	static bool ExportMultipleAssetsToPackage(const TArray<UObject*>& Assets,const FString& ExportDirectory,const FAssetExportOptions& ExportOptions);

	UFUNCTION(BlueprintPure, Category = "Vault")
	static FString GetAssetTypeNameByIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Vault")
	static EAssetType GetAssetTypeByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Vault")
	static bool DeleteAssetsAtPath(const FString& TargetFolder);
	
	
private:
	static bool CopyAssetWithDependencies(UObject* Asset, const FString& TargetDirectory);
};
