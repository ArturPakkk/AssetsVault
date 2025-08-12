#include "FAssetPackageManager.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

#include "DesktopPlatformModule.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "Styling/AppStyle.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

EAssetType StringToAssetType(const FString& AssetTypeStr)
{

	if (AssetTypeStr.Equals(TEXT("All"), ESearchCase::IgnoreCase))
		return EAssetType::All;
	if (AssetTypeStr.Equals(TEXT("Blueprint"), ESearchCase::IgnoreCase))
		return EAssetType::Blueprint;
	else if (AssetTypeStr.Equals(TEXT("Material"), ESearchCase::IgnoreCase))
		return EAssetType::Material;
	else if (AssetTypeStr.Equals(TEXT("Level"), ESearchCase::IgnoreCase))
		return EAssetType::Level;
	else if (AssetTypeStr.Equals(TEXT("Texture"), ESearchCase::IgnoreCase))
		return EAssetType::Texture;
	else if (AssetTypeStr.Equals(TEXT("StaticMesh"), ESearchCase::IgnoreCase) || AssetTypeStr.Equals(TEXT("Static Mesh"), ESearchCase::IgnoreCase))
		return EAssetType::StaticMesh;
	else if (AssetTypeStr.Equals(TEXT("Sound"), ESearchCase::IgnoreCase))
		return EAssetType::Sound;
	else
		return EAssetType::Other;
}

FString UAssetPackageManager::BuildExportPath(const FString& RootPath, const FAssetMainInfo& MainInfo)
{
	FString FullPath = RootPath;

	FString AssetTypeStr = UEnum::GetValueAsString(MainInfo.AssetType).Replace(TEXT("EAssetType::"), TEXT(""));
	FullPath = FPaths::Combine(FullPath, AssetTypeStr);

	if (!MainInfo.CustomFolder.IsEmpty())
	{
		FullPath = FPaths::Combine(FullPath, MainInfo.CustomFolder);
	}

	FullPath = FPaths::Combine(FullPath, MainInfo.Name);

	if (!MainInfo.Version.IsEmpty())
	{
		FullPath = FPaths::Combine(FullPath, MainInfo.Version);
	}

	return FullPath;
}

bool UAssetPackageManager::ExportAssetToPackage(UObject* Asset, const FString& ExportDirectory, FAssetExportOptions ExportOptions)
{
	if (!Asset)
	{
		UE_LOG(LogTemp, Error, TEXT("Asset is null!"));
		return false;
	}
	
	FString TargetFolder = ExportDirectory;
	FString AssetTypeStr = UEnum::GetValueAsString(ExportOptions.MainInfo.AssetType).Replace(TEXT("EAssetType::"), TEXT(""));
	TargetFolder = FPaths::Combine(TargetFolder, AssetTypeStr);

	if (!ExportOptions.MainInfo.CustomFolder.IsEmpty())
	{
		TargetFolder = FPaths::Combine(TargetFolder, ExportOptions.MainInfo.CustomFolder);
	}

	TargetFolder = FPaths::Combine(TargetFolder, ExportOptions.MainInfo.Name);

	if (!ExportOptions.MainInfo.Version.IsEmpty())
	{
		TargetFolder = FPaths::Combine(TargetFolder, ExportOptions.MainInfo.Version);
	}

	
	if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*TargetFolder))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create directory: %s"), *TargetFolder);
		return false;
	}
	
	if (!CopyAssetWithDependencies(Asset, TargetFolder))
	{
		return false;
	}

	if (ExportOptions.MainInfo.EngineVersion.IsEmpty())
	{
		FEngineVersion Ver = FEngineVersion::Current();
		FString EngineVerShort = FString::Printf(TEXT("%d.%d"), Ver.GetMajor(), Ver.GetMinor());
		ExportOptions.MainInfo.EngineVersion = EngineVerShort;
	}
	
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("Name"), ExportOptions.MainInfo.Name);
	JsonObject->SetStringField(TEXT("AssetType"), UEnum::GetValueAsString(ExportOptions.MainInfo.AssetType));
	JsonObject->SetStringField(TEXT("Description"), ExportOptions.MainInfo.Description);
	JsonObject->SetStringField(TEXT("EngineVersion"), ExportOptions.MainInfo.EngineVersion);
	JsonObject->SetStringField(TEXT("Version"), ExportOptions.MainInfo.Version);
	JsonObject->SetStringField(TEXT("VersionComment"), ExportOptions.MainInfo.VersionComment);
	JsonObject->SetStringField(TEXT("CustomFolder"), ExportOptions.MainInfo.CustomFolder);

	
	FString RelativePath = TargetFolder;
	FPaths::MakePathRelativeTo(RelativePath, *ExportDirectory);
	const FString RootPrefix = TEXT("ResourceAssets/");
	if (RelativePath.StartsWith(RootPrefix))
	{
		RelativePath = RelativePath.RightChop(RootPrefix.Len());
	}
	JsonObject->SetStringField(TEXT("RelativeExportPath"), RelativePath);

	
	TArray<TSharedPtr<FJsonValue>> TagsJson;
	for (const FString& Tag : ExportOptions.AdditionalInfo.Tags)
	{
		TagsJson.Add(MakeShared<FJsonValueString>(Tag));
	}
	JsonObject->SetArrayField(TEXT("Tags"), TagsJson);

	
	TArray<FString> FoundUAssetFiles;
	IFileManager::Get().FindFilesRecursive(
		FoundUAssetFiles,
		*TargetFolder,
		TEXT("*.uasset"),
		true, false
	);
	
	TArray<TSharedPtr<FJsonValue>> AssetNamesJson;
	ExportOptions.MainInfo.ExportedAssetNames.Empty();

	for (const FString& FilePath : FoundUAssetFiles)
	{
		const FString FileName = FPaths::GetBaseFilename(FilePath);
		AssetNamesJson.Add(MakeShared<FJsonValueString>(FileName));
		ExportOptions.MainInfo.ExportedAssetNames.Add(FileName);
	}

	JsonObject->SetArrayField(TEXT("Assets"), AssetNamesJson);

	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(JsonObject, Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize metadata to JSON."));
		return false;
	}
	
	const FString AssetName = Asset->GetName();
	const FString HashBase = AssetName + AssetTypeStr + FDateTime::Now().ToString();
	const uint32 SymbolCode = FCrc::StrCrc32(*HashBase);
	FString FileName = FString::Printf(TEXT("%s_%s_%X.json"), *AssetName, *AssetTypeStr, SymbolCode);
	FPaths::MakeValidFileName(FileName);
	const FString MetadataPath = FPaths::Combine(TargetFolder, FileName);
	
	if (!FFileHelper::SaveStringToFile(OutputString, *MetadataPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save metadata to: %s"), *MetadataPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Asset and metadata successfully exported to: %s"), *TargetFolder);
	return true;
}

bool UAssetPackageManager::CopyAssetWithDependencies(UObject* Asset, const FString& TargetDirectory)
{
	if (!Asset)
	{
		UE_LOG(LogTemp, Error, TEXT("Asset is null."));
		return false;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	if (AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogTemp, Warning, TEXT("Asset Registry is still loading assets. Try again later."));
		return false;
	}

	TSet<FName> AllPackagesToCopy;
	TQueue<FName> PackagesToProcess;

	FString AssetPackagePath = Asset->GetOutermost()->GetName();
	FName RootPackageName = FName(*AssetPackagePath);

	PackagesToProcess.Enqueue(RootPackageName);
	AllPackagesToCopy.Add(RootPackageName);

	while (!PackagesToProcess.IsEmpty())
	{
		FName CurrentPackageName;
		PackagesToProcess.Dequeue(CurrentPackageName);

		FAssetIdentifier Identifier(CurrentPackageName);

		TArray<FAssetDependency> Dependencies;
		UE::AssetRegistry::FDependencyQuery Query;
		Query.Required = UE::AssetRegistry::EDependencyProperty::Hard;

		AssetRegistry.GetDependencies(Identifier, Dependencies, UE::AssetRegistry::EDependencyCategory::Package, Query);

		for (const FAssetDependency& Dep : Dependencies)
		{
			FName DepPackageName = Dep.AssetId.PackageName;
			if (!AllPackagesToCopy.Contains(DepPackageName))
			{
				AllPackagesToCopy.Add(DepPackageName);
				PackagesToProcess.Enqueue(DepPackageName);
			}
		}
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> ExtensionsToCheck = { TEXT("uasset"), TEXT("uexp"), TEXT("ubulk"), TEXT("map") };

	for (const FName& PackageName : AllPackagesToCopy)
	{
		FString PackageFilePath = FPackageName::LongPackageNameToFilename(PackageName.ToString(), TEXT(".uasset"));

		if (!FPaths::FileExists(PackageFilePath))
		{
			UE_LOG(LogTemp, Warning, TEXT("Main .uasset file does not exist: %s"), *PackageFilePath);
			continue;
		}

		FString RelativePath = PackageFilePath;
		FPaths::MakePathRelativeTo(RelativePath, *FPaths::ProjectContentDir());

		FString TargetPath = FPaths::Combine(TargetDirectory, RelativePath);
		FString TargetFolder = FPaths::GetPath(TargetPath);

		if (!PlatformFile.DirectoryExists(*TargetFolder))
		{
			PlatformFile.CreateDirectoryTree(*TargetFolder);
		}

		for (const FString& Extension : ExtensionsToCheck)
		{
			FString SourceFile = FPaths::ChangeExtension(PackageFilePath, Extension);
			FString TargetFile = FPaths::ChangeExtension(TargetPath, Extension);

			if (FPaths::FileExists(SourceFile))
			{
				if (!PlatformFile.CopyFile(*TargetFile, *SourceFile))
				{
					UE_LOG(LogTemp, Error, TEXT("Failed to copy %s to %s"), *SourceFile, *TargetFile);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Copied %d packages (with dependencies) to %s"), AllPackagesToCopy.Num(), *TargetDirectory);
	return true;
}

TArray<FAssetExportOptions> UAssetPackageManager::LoadAllAssetDataFromDirectory(const FString& DirectoryPath)
{
	TArray<FAssetExportOptions> Results;

	IFileManager& FileManager = IFileManager::Get();

	
	TArray<FString> SubFolders;
	FileManager.FindFilesRecursive(SubFolders, *DirectoryPath, TEXT("*"), false, true);

	for (const FString& FolderPath : SubFolders)
	{
		
		TArray<FString> JsonFiles;
		FileManager.FindFiles(JsonFiles, *(FolderPath / TEXT("*.json")), true, false);

		for (const FString& FileName : JsonFiles)
		{
			FString FullPath = FolderPath / FileName;
			FString FileContents;

			if (!FFileHelper::LoadFileToString(FileContents, *FullPath))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to read file: %s"), *FullPath);
				continue;
			}

			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContents);
			TSharedPtr<FJsonObject> JsonObject;

			if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to parse JSON in file: %s"), *FullPath);
				continue;
			}

			FAssetExportOptions Options;

			
			Options.MainInfo.Name = JsonObject->GetStringField(TEXT("Name"));

			FString AssetTypeStr = JsonObject->GetStringField(TEXT("AssetType"));
			AssetTypeStr = AssetTypeStr.Replace(TEXT("EAssetType::"), TEXT(""));
			Options.MainInfo.AssetType = StringToAssetType(AssetTypeStr);
			Options.MainInfo.Description = JsonObject->GetStringField(TEXT("Description"));
			Options.MainInfo.EngineVersion = JsonObject->GetStringField(TEXT("EngineVersion"));
			Options.MainInfo.CustomFolder = JsonObject->GetStringField(TEXT("CustomFolder"));
			Options.MainInfo.RelativeExportPath = JsonObject->GetStringField(TEXT("RelativeExportPath"));

			if (JsonObject->HasField(TEXT("Version")))
			{
				Options.MainInfo.Version = JsonObject->GetStringField(TEXT("Version"));
			}
			if (JsonObject->HasField(TEXT("VersionComment")))
			{
				Options.MainInfo.VersionComment = JsonObject->GetStringField(TEXT("VersionComment"));
			}

			else
			{
				Options.MainInfo.Version = TEXT("1.0");
			}

			
			TArray<TSharedPtr<FJsonValue>> TagsArray = JsonObject->GetArrayField(TEXT("Tags"));
			for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
			{
				if (TagValue->Type == EJson::String)
				{
					Options.AdditionalInfo.Tags.Add(TagValue->AsString());
				}
			}

		

			
			if (JsonObject->HasField(TEXT("Assets")))
			{
				const TArray<TSharedPtr<FJsonValue>> AssetsArray = JsonObject->GetArrayField(TEXT("Assets"));
				for (const TSharedPtr<FJsonValue>& Value : AssetsArray)
				{
					if (Value->Type == EJson::String)
					{
						Options.MainInfo.ExportedAssetNames.Add(Value->AsString());
					}
				}
			}

			Results.Add(Options);
		}
	}

	return Results;
}

int32 UAssetPackageManager::GetAssetTypeMaxIndex()
{
	const UEnum* EnumPtr = StaticEnum<EAssetType>();
	if (!EnumPtr)
	{
		UE_LOG(LogTemp, Error, TEXT("Не удалось получить StaticEnum<EAssetType>"));
		return 0;
	}
	return EnumPtr->GetMaxEnumValue(); 
}

TArray<FAssetExportOptions> UAssetPackageManager::FilterAndSortAssets(const TArray<FAssetExportOptions>& Assets,EAssetType FilterType)
{
	TArray<FAssetExportOptions> FilteredAssets;

	for (const FAssetExportOptions& Asset : Assets)
	{
		
		if (FilterType != EAssetType::All && Asset.MainInfo.AssetType != FilterType)
		{
			continue;
		}

		FilteredAssets.Add(Asset);
	}
	
	FilteredAssets.Sort([](const FAssetExportOptions& A, const FAssetExportOptions& B)
	{
		return A.MainInfo.Name < B.MainInfo.Name;
	});

	return FilteredAssets;
}

void UAssetPackageManager::OpenFolderInExplorer(const FString& BaseDirectory, FString RelativeExportPath)
{
	
	RelativeExportPath = RelativeExportPath.Replace(TEXT("\\"), TEXT("/"));
	
	FString FinalPath = FPaths::Combine(BaseDirectory, RelativeExportPath);
	
	FinalPath = FinalPath.Replace(TEXT("/"), TEXT("\\"));
	
	IFileManager::Get().MakeDirectory(*FinalPath, true);

	UE_LOG(LogTemp, Display, TEXT("Opening folder: %s"), *FinalPath);
	
	const FString ExplorerArgs = FString::Printf(TEXT("\"%s\""), *FinalPath);
	FPlatformProcess::CreateProc(TEXT("explorer.exe"), *ExplorerArgs, true, false, false, nullptr, 0, nullptr, nullptr);
}

bool UAssetPackageManager::OpenFolderDialog(FString& OutFolderPath)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		UE_LOG(LogTemp, Error, TEXT("Desktop platform is unavailable."));
		return false;
	}

	void* ParentWindowHandle = nullptr;
	if (FSlateApplication::IsInitialized())
	{
		ParentWindowHandle = const_cast<void*>(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr, ESlateParentWindowSearchMethod::ActiveWindow));
	}
	
	const FString DialogTitle = TEXT("Select a folder to export the asset to");
	const FString DefaultPath = FPaths::ProjectDir();

	return DesktopPlatform->OpenDirectoryDialog(
		ParentWindowHandle,
		DialogTitle,
		DefaultPath,
		OutFolderPath
	);
}

bool UAssetPackageManager::ImportAssetFolderToProject(const FString& DefaultDirectory, const FString& RelativeExportPath, const FString& TargetSubfolder, bool bForceOverwrite)
{
    const FString SourceFolder = FPaths::Combine(DefaultDirectory, RelativeExportPath);

    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
    UE_LOG(LogTemp, Warning, TEXT("VAULT IMPORT BEGIN"));
    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
    UE_LOG(LogTemp, Warning, TEXT("[Vault] Step 1: Validate input paths and directories"));
    UE_LOG(LogTemp, Warning, TEXT("[Vault] RelativeExportPath: %s"), *RelativeExportPath);
    UE_LOG(LogTemp, Warning, TEXT("[Vault] DefaultDirectory:   %s"), *DefaultDirectory);
    UE_LOG(LogTemp, Warning, TEXT("[Vault] SourceFolder (combined): %s"), *SourceFolder);

    if (TargetSubfolder.Contains(TEXT("..")))
    {
        ShowEditorNotification(TEXT("Error: Invalid target subfolder path."), false);
        UE_LOG(LogTemp, Error, TEXT("[Vault] Invalid TargetSubfolder: %s"), *TargetSubfolder);
        return false;
    }

    const FString ContentDir = FPaths::ProjectContentDir();
    const FString TargetFolder = TargetSubfolder.IsEmpty()
        ? ContentDir
        : FPaths::ConvertRelativePathToFull(FPaths::Combine(ContentDir, TargetSubfolder));

    UE_LOG(LogTemp, Warning, TEXT("[Vault] TargetSubfolder: %s"), *TargetSubfolder);
    UE_LOG(LogTemp, Warning, TEXT("[Vault] Project ContentDir: %s"), *ContentDir);
    UE_LOG(LogTemp, Warning, TEXT("[Vault] TargetFolder (resolved): %s"), *TargetFolder);

    if (!FPaths::DirectoryExists(SourceFolder))
    {
        ShowEditorNotification(TEXT("Error: Source folder not found."), false);
        UE_LOG(LogTemp, Error, TEXT("[Vault] Source folder does not exist: %s"), *SourceFolder);
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("\n-------- Step 2: Create target folder --------"));
    IFileManager& FileManager = IFileManager::Get();
    const bool bMadeRootDir = FileManager.MakeDirectory(*TargetFolder, true);
    UE_LOG(LogTemp, Display, TEXT("[Vault] Created TargetFolder: %s => %s"), *TargetFolder, bMadeRootDir ? TEXT("OK") : TEXT("EXISTED"));

    TArray<FString> CopiedFiles;
    TArray<UObject*> ImportedAssets;
    const TArray<FString> Extensions = { TEXT("uasset"), TEXT("uexp"), TEXT("ubulk") };

    UE_LOG(LogTemp, Warning, TEXT("\n-------- Step 3: Copy asset files --------"));
    for (const FString& Ext : Extensions)
    {
        TArray<FString> FoundFiles;
        FileManager.FindFilesRecursive(FoundFiles, *SourceFolder, *(FString("*.") + Ext), true, false);
        UE_LOG(LogTemp, Display, TEXT("[Vault] Found %d files with extension .%s"), FoundFiles.Num(), *Ext);

        for (const FString& SourceFile : FoundFiles)
        {
            FString SourcePrefix = FPaths::Combine(DefaultDirectory, RelativeExportPath);
            FPaths::NormalizeFilename(SourcePrefix);
            FPaths::CollapseRelativeDirectories(SourcePrefix);

            FString FullSource = SourceFile;
            FPaths::NormalizeFilename(FullSource);
            FPaths::CollapseRelativeDirectories(FullSource);

            if (!FullSource.StartsWith(SourcePrefix))
            {
                UE_LOG(LogTemp, Error, TEXT("[Vault] Path mismatch:\n  Full:  %s\n  Prefix: %s"), *FullSource, *SourcePrefix);
                continue;
            }

            FString RelativePath = FullSource.RightChop(SourcePrefix.Len() + 1);
            const FString DestPath = FPaths::Combine(TargetFolder, RelativePath);
            const FString DestDir = FPaths::GetPath(DestPath);

            const bool bMadeDir = FileManager.MakeDirectory(*DestDir, true);
            UE_LOG(LogTemp, Display, TEXT("[Vault] Ensured directory: %s => %s"), *DestDir, bMadeDir ? TEXT("CREATED") : TEXT("EXISTED"));

            UE_LOG(LogTemp, Display, TEXT("[Vault] Copying asset:"));
            UE_LOG(LogTemp, Display, TEXT("  FROM: %s"), *SourceFile);
            UE_LOG(LogTemp, Display, TEXT("  TO:   %s"), *DestPath);

            int64 BytesCopied = 0;

            if (FPaths::FileExists(DestPath))
            {
                if (!bForceOverwrite)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Vault] Skipped (already exists): %s"), *DestPath);
                    continue;
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("[Vault] Replacing existing file: %s"), *DestPath);
                }
            }

            BytesCopied = FileManager.Copy(*DestPath, *SourceFile, true, true);

            if (BytesCopied <= 0 && bForceOverwrite && FPaths::FileExists(SourceFile))
            {
                UE_LOG(LogTemp, Warning, TEXT("[Vault] Fallback: trying overwrite with memory buffer..."));

                TArray<uint8> FileData;
                if (FFileHelper::LoadFileToArray(FileData, *SourceFile))
                {
                    if (FFileHelper::SaveArrayToFile(FileData, *DestPath))
                    {
                        BytesCopied = FileData.Num();
                        UE_LOG(LogTemp, Display, TEXT("[Vault] Fallback write succeeded (%lld bytes): %s"), BytesCopied, *DestPath);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("[Vault] Fallback write failed: %s"), *DestPath);
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("[Vault] Fallback read failed: %s"), *SourceFile);
                }
            }

            if (BytesCopied > 0)
            {
                UE_LOG(LogTemp, Display, TEXT("[Vault] Copied successfully (%lld bytes): %s"), BytesCopied, *DestPath);
                CopiedFiles.Add(DestPath);

                if (Ext == TEXT("uasset"))
                {
                    FString AssetName = FPaths::GetBaseFilename(DestPath);
                    TArray<UObject*> OpenAssets = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->GetAllEditedAssets();
                    for (UObject* OpenAsset : OpenAssets)
                    {
                        if (OpenAsset && OpenAsset->GetName() == AssetName)
                        {
                            UPackage* Pkg = OpenAsset->GetOutermost();
                            ResetLoaders(Pkg);
                            Pkg->ClearFlags(RF_Standalone | RF_Public);
                            GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(OpenAsset);
                            UE_LOG(LogTemp, Warning, TEXT("[Vault] Closed editor and cleared memory for: %s"), *AssetName);
                            break;
                        }
                    }

                    FString DestRelPath = DestPath;
                    FPaths::MakePathRelativeTo(DestRelPath, *FPaths::ProjectContentDir());
                    const FString ObjectPath = "/Game/" + FPaths::ChangeExtension(DestRelPath, TEXT(""));
                    UObject* ImportedAsset = LoadObject<UObject>(nullptr, *ObjectPath);
                    if (ImportedAsset)
                    {
                        ImportedAssets.Add(ImportedAsset);
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[Vault] Copy failed: %s"), *DestPath);
            }
        }
    }

    if (CopiedFiles.Num() == 0)
    {
        ShowEditorNotification(TEXT("Import failed: No files copied."), false);
        UE_LOG(LogTemp, Error, TEXT("[Vault] Nothing copied — returning FALSE."));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("\n-------- Step 4: Rescan Asset Registry --------"));
    TSet<FString> UniquePaths;
    for (const FString& File : CopiedFiles)
    {
        if (FPaths::GetExtension(File) == TEXT("uasset"))
        {
            const FString Path = FPaths::GetPath(File);
            UniquePaths.Add(Path);
            UE_LOG(LogTemp, Display, TEXT("[Vault] Queued path to rescan: %s"), *Path);
        }
    }

    FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    for (const FString& Path : UniquePaths)
    {
        UE_LOG(LogTemp, Display, TEXT("[Vault] Scanning: %s"), *Path);
        ARM.Get().ScanPathsSynchronous({ Path });
    }

    if (ImportedAssets.Num() > 0)
    {
        FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
        ContentBrowserModule.Get().SyncBrowserToAssets(ImportedAssets);
    }

    const FString DisplaySubfolder = TargetSubfolder.IsEmpty()
        ? TEXT("/Content")
        : FString(TEXT("/Content/")) + TargetSubfolder;

    const FString NotifyMessage = (CopiedFiles.Num() == 1)
        ? FString::Printf(TEXT("1 file imported to %s"), *DisplaySubfolder)
        : FString::Printf(TEXT("%d files imported to %s"), CopiedFiles.Num(), *DisplaySubfolder);

    ShowEditorNotification(NotifyMessage, true);

    UE_LOG(LogTemp, Log, TEXT("[Vault] Successfully imported %d file(s) to %s"), CopiedFiles.Num(), *TargetFolder);
    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
    UE_LOG(LogTemp, Warning, TEXT("VAULT IMPORT COMPLETE"));
    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));

    CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
    return true;
}

bool UAssetPackageManager::DoesAssetAlreadyExist(const FString& DefaultDirectory,const FString& RelativeExportPath,const FString& TargetSubfolder,TArray<FString>& OutConflictingAssets)
{
    UE_LOG(LogTemp, Warning, TEXT("\n----------------------------------------"));
    UE_LOG(LogTemp, Warning, TEXT("VAULT CONFLICT CHECK BEGIN"));
    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));

    const FString SourceFolder = FPaths::Combine(DefaultDirectory, RelativeExportPath);
    const FString ContentDir = FPaths::ProjectContentDir();
    const FString TargetFolder = TargetSubfolder.IsEmpty()
        ? ContentDir
        : FPaths::ConvertRelativePathToFull(FPaths::Combine(ContentDir, TargetSubfolder));

    UE_LOG(LogTemp, Display, TEXT("[Vault] SourceFolder: %s"), *SourceFolder);
    UE_LOG(LogTemp, Display, TEXT("[Vault] TargetFolder: %s"), *TargetFolder);

    OutConflictingAssets.Empty();

    if (!FPaths::DirectoryExists(SourceFolder) || !FPaths::DirectoryExists(TargetFolder))
    {
        UE_LOG(LogTemp, Error, TEXT("[Vault] One of the directories does not exist."));
        return false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[Vault] Scanning for conflicting assets..."));
    IFileManager& FileManager = IFileManager::Get();
    TArray<FString> FoundSources;
    FileManager.FindFilesRecursive(FoundSources, *SourceFolder, TEXT("*.uasset"), true, false);

    for (const FString& SourceFile : FoundSources)
    {
        FString SourcePrefix = SourceFolder;
        FPaths::NormalizeFilename(SourcePrefix);
        FPaths::CollapseRelativeDirectories(SourcePrefix);

        FString FullSource = SourceFile;
        FPaths::NormalizeFilename(FullSource);
        FPaths::CollapseRelativeDirectories(FullSource);

        if (!FullSource.StartsWith(SourcePrefix))
        {
            continue;
        }

        FString RelativePath = FullSource.RightChop(SourcePrefix.Len() + 1);
        const FString TargetPath = FPaths::Combine(TargetFolder, RelativePath);

        if (FPaths::FileExists(TargetPath))
        {
            FString ConflictedName = FPaths::GetCleanFilename(TargetPath);
            OutConflictingAssets.Add(ConflictedName);
            UE_LOG(LogTemp, Warning, TEXT("[Vault] Conflict found: %s"), *ConflictedName);
        }
    }

    bool bHasConflicts = OutConflictingAssets.Num() > 0;
    UE_LOG(LogTemp, Warning, TEXT("[Vault] Conflict check result: %s"), bHasConflicts ? TEXT("YES") : TEXT("NO"));
    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
    UE_LOG(LogTemp, Warning, TEXT("VAULT CONFLICT CHECK END"));
    UE_LOG(LogTemp, Warning, TEXT("----------------------------------------\n"));

    return bHasConflicts;
}

bool UAssetPackageManager::DoesExportPathAlreadyContainAssets(const FString& ExportDirectory, const FAssetExportOptions& Options, FString& OutResolvedPath)
{
	FString TargetFolder = ExportDirectory;
	FString AssetTypeStr = UEnum::GetValueAsString(Options.MainInfo.AssetType).Replace(TEXT("EAssetType::"), TEXT(""));
	TargetFolder = FPaths::Combine(TargetFolder, AssetTypeStr);

	if (!Options.MainInfo.CustomFolder.IsEmpty())
	{
		TargetFolder = FPaths::Combine(TargetFolder, Options.MainInfo.CustomFolder);
	}

	TargetFolder = FPaths::Combine(TargetFolder, Options.MainInfo.Name);

	if (!Options.MainInfo.Version.IsEmpty())
	{
		TargetFolder = FPaths::Combine(TargetFolder, Options.MainInfo.Version);
	}

	OutResolvedPath = TargetFolder;
	
	TArray<FString> FoundAssets;
	IFileManager::Get().FindFilesRecursive(
		FoundAssets,
		*TargetFolder,
		TEXT("*.uasset"),
		true, false
	);

	const bool bExists = FoundAssets.Num() > 0;
	UE_LOG(LogTemp, Warning, TEXT("[Vault] Export path conflict check: %s => %s"), bExists ? TEXT("YES") : TEXT("NO"), *TargetFolder);
	return bExists;
}

void UAssetPackageManager::ShowEditorNotification(const FString& Message, bool bSuccess)
{
	
	FString Clean = Message;
	Clean.ReplaceInline(TEXT("\r"), TEXT(""));
	Clean.ReplaceInline(TEXT("\n"), TEXT(" "));
	Clean.ReplaceInline(TEXT("\t"), TEXT(" "));
	if (Clean.IsEmpty())
	{
		Clean = bSuccess
			? TEXT("Операция успешно завершена.")
			: TEXT("Произошла ошибка.");
	}
	
	const FText TitleText = FText::FromString(Clean);
	const FText SubText   = FText::FromString(TEXT("Asset Vault Plugin"));
	
	FNotificationInfo Info(TitleText);
	Info.SubText              = SubText;
	Info.bFireAndForget       = true;
	Info.FadeInDuration       = 0.3f;
	Info.ExpireDuration       = 4.0f;
	Info.FadeOutDuration      = 1.5f;
	Info.bUseThrobber         = false;
	Info.bUseSuccessFailIcons = false;  
	Info.Image = bSuccess?
		 FAppStyle::GetBrush("Icons.Success"):
		 FAppStyle::GetBrush("Icons.ErrorWithColor");
	
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	
	if (Item.IsValid())
	{
		Item->SetCompletionState(
			bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail
		);
	}
}

bool UAssetPackageManager::ExportMultipleAssetsToPackage(const TArray<UObject*>& Assets,const FString& ExportDirectory,const FAssetExportOptions& ExportOptions)
{
	if (Assets.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Export failed: no assets provided."));
		return false;
	}

	
	FString TargetFolder = ExportDirectory;
	FString AssetTypeStr = UEnum::GetValueAsString(ExportOptions.MainInfo.AssetType).Replace(TEXT("EAssetType::"), TEXT(""));
	TargetFolder = FPaths::Combine(TargetFolder, AssetTypeStr);

	if (!ExportOptions.MainInfo.CustomFolder.IsEmpty())
	{
		TargetFolder = FPaths::Combine(TargetFolder, ExportOptions.MainInfo.CustomFolder);
	}

	TargetFolder = FPaths::Combine(TargetFolder, ExportOptions.MainInfo.Name);

	if (!ExportOptions.MainInfo.Version.IsEmpty())
	{
		TargetFolder = FPaths::Combine(TargetFolder, ExportOptions.MainInfo.Version);
	}

	
	if (!FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*TargetFolder))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create directory: %s"), *TargetFolder);
		return false;
	}

	
	for (UObject* Asset : Assets)
	{
		if (!Asset) continue;

		if (!CopyAssetWithDependencies(Asset, TargetFolder))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to export asset and dependencies: %s"), *Asset->GetName());
		}
	}
	
	FString EngineVersion = ExportOptions.MainInfo.EngineVersion;
	if (EngineVersion.IsEmpty())
	{
		FEngineVersion Ver = FEngineVersion::Current();
		EngineVersion = FString::Printf(TEXT("%d.%d"), Ver.GetMajor(), Ver.GetMinor());
	}
	
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("Name"), ExportOptions.MainInfo.Name);
	JsonObject->SetStringField(TEXT("AssetType"), UEnum::GetValueAsString(ExportOptions.MainInfo.AssetType));
	JsonObject->SetStringField(TEXT("Description"), ExportOptions.MainInfo.Description);
	JsonObject->SetStringField(TEXT("EngineVersion"), EngineVersion);
	JsonObject->SetStringField(TEXT("Version"), ExportOptions.MainInfo.Version);
	JsonObject->SetStringField(TEXT("VersionComment"), ExportOptions.MainInfo.VersionComment);
	JsonObject->SetStringField(TEXT("CustomFolder"), ExportOptions.MainInfo.CustomFolder);
	
	FString RelativePath = TargetFolder;
	FPaths::MakePathRelativeTo(RelativePath, *ExportDirectory);
	const FString RootPrefix = TEXT("ResourceAssets/");
	if (RelativePath.StartsWith(RootPrefix))
	{
		RelativePath = RelativePath.RightChop(RootPrefix.Len());
	}
	JsonObject->SetStringField(TEXT("RelativeExportPath"), RelativePath);
	
	TArray<TSharedPtr<FJsonValue>> TagsJson;
	for (const FString& Tag : ExportOptions.AdditionalInfo.Tags)
	{
		TagsJson.Add(MakeShared<FJsonValueString>(Tag));
	}
	JsonObject->SetArrayField(TEXT("Tags"), TagsJson);
	
	TArray<FString> FoundUAssetFiles;
	IFileManager::Get().FindFilesRecursive(
		FoundUAssetFiles,
		*TargetFolder,
		TEXT("*.uasset"),
		true, false
	);

	TArray<TSharedPtr<FJsonValue>> AssetNamesJson;
	for (const FString& FilePath : FoundUAssetFiles)
	{
		const FString FileName = FPaths::GetBaseFilename(FilePath);
		AssetNamesJson.Add(MakeShared<FJsonValueString>(FileName));
	}

	JsonObject->SetArrayField(TEXT("Assets"), AssetNamesJson);
	
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(JsonObject, Writer))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to serialize metadata to JSON."));
		return false;
	}
	
	const FString HashBase = ExportOptions.MainInfo.Name + AssetTypeStr + FDateTime::Now().ToString();
	const uint32 SymbolCode = FCrc::StrCrc32(*HashBase);
	FString FileName = FString::Printf(TEXT("%s_%s_%X.json"), *ExportOptions.MainInfo.Name, *AssetTypeStr, SymbolCode);
	FPaths::MakeValidFileName(FileName);
	const FString MetadataPath = FPaths::Combine(TargetFolder, FileName);

	if (!FFileHelper::SaveStringToFile(OutputString, *MetadataPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save metadata to: %s"), *MetadataPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("Assets and metadata successfully exported to: %s"), *TargetFolder);
	return true;
}

FString UAssetPackageManager::GetAssetTypeNameByIndex(int32 Index)
{
	const UEnum* EnumPtr = StaticEnum<EAssetType>();
	if (!EnumPtr || !EnumPtr->IsValidEnumValue(Index))
	{
		return TEXT("Invalid");
	}

	return EnumPtr->GetDisplayNameTextByIndex(Index).ToString();
}

EAssetType UAssetPackageManager::GetAssetTypeByIndex(int32 Index)
{
	const UEnum* EnumPtr = StaticEnum<EAssetType>();
	if (!EnumPtr || !EnumPtr->IsValidEnumValue(Index))
	{
		return EAssetType::Other;
	}

	return static_cast<EAssetType>(Index);
}

bool UAssetPackageManager::DeleteAssetsAtPath(const FString& TargetFolder)
{
	if (TargetFolder.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("[Vault] DeleteAssetsAtPath failed: TargetFolder is empty."));
		return false;
	}
	
	FString CleanPath = TargetFolder;
	FPaths::NormalizeDirectoryName(CleanPath);
	UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
	UE_LOG(LogTemp, Warning, TEXT("VAULT DELETE BEGIN"));
	UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
	UE_LOG(LogTemp, Warning, TEXT("[Vault] TargetFolder (normalized): %s"), *CleanPath);

	if (!FPaths::DirectoryExists(CleanPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Vault] Directory does not exist: %s"), *CleanPath);
		return true;
	}
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	bool bSuccess = PlatformFile.DeleteDirectoryRecursively(*CleanPath);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Error, TEXT("[Vault] Failed to delete directory: %s"), *CleanPath);
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Vault] Successfully deleted folder and its contents: %s"), *CleanPath);
	UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));
	UE_LOG(LogTemp, Warning, TEXT("VAULT DELETE COMPLETE"));
	UE_LOG(LogTemp, Warning, TEXT("----------------------------------------"));

	return true;
}
