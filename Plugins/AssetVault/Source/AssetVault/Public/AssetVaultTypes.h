#pragma once
#include "CoreMinimal.h"
#include "AssetVaultTypes.generated.h"

UENUM(BlueprintType)
enum class EAssetType : uint8
{
	All			  UMETA(DisplayName = "All"),
	Blueprint     UMETA(DisplayName = "Blueprint"),
	Material      UMETA(DisplayName = "Material"),
	Level         UMETA(DisplayName = "Level"),
	Texture       UMETA(DisplayName = "Texture"),
	StaticMesh    UMETA(DisplayName = "Static Mesh"),
	Sound         UMETA(DisplayName = "Sound"),
	Other         UMETA(DisplayName = "Other")
};

USTRUCT(BlueprintType)
struct FAssetMainInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	EAssetType AssetType = EAssetType::Other;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString EngineVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString Version;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString VersionComment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString CustomFolder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	TArray<FString> CustomSubfolders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	FString RelativeExportPath;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Info")
	TArray<FString> ExportedAssetNames;

	FAssetMainInfo() {}

	FAssetMainInfo(
		const FString& InName,
		EAssetType InType,
		const FString& InDescription,
		const FString& InEngineVersion,
		const FString& InVersion = TEXT(""),
		const FString& InVersionComment = TEXT(""),
		const FString& InCustomFolder = TEXT(""),
		
		const FString& InRelativeExportPath = TEXT(""),
		const TArray<FString>& InExportedAssetNames = TArray<FString>()
	)
		: Name(InName)
		, AssetType(InType)
		, Description(InDescription)
		, EngineVersion(InEngineVersion)
		, Version(InVersion)
		, VersionComment(InVersionComment)
		, CustomFolder(InCustomFolder)
		, RelativeExportPath(InRelativeExportPath)
		, ExportedAssetNames(InExportedAssetNames)
	{}
};

USTRUCT(BlueprintType)
struct FAssetAdditionalInfo
{
	GENERATED_BODY()


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional Info")
	TArray<FString> PreviewImagePaths;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Additional Info")
	TArray<FString> Tags;

	
	FAssetAdditionalInfo() {}

	
	FAssetAdditionalInfo(const TArray<FString>& InPreviewImagePaths, const TArray<FString>& InTags)
		: PreviewImagePaths(InPreviewImagePaths)
		, Tags(InTags)
	{}
};

USTRUCT(BlueprintType)
struct FAssetExportOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
	FAssetMainInfo MainInfo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Export")
	FAssetAdditionalInfo AdditionalInfo;
};



