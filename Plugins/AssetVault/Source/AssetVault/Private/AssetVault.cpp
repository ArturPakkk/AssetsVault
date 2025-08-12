#include "AssetVault.h"
#include "Modules/ModuleManager.h"

class FAssetVaultModule : public IModuleInterface
{
public:
	
	virtual void StartupModule() override
	{
		
		UE_LOG(LogTemp, Warning, TEXT("AssetVault Plugin Started"));
	}
	
	virtual void ShutdownModule() override
	{
		
		UE_LOG(LogTemp, Warning, TEXT("AssetVault Plugin Shut Down"));
	}
};


IMPLEMENT_MODULE(FAssetVaultModule, AssetVault)
