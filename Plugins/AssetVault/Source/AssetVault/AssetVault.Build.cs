using UnrealBuildTool;

public class AssetVault : ModuleRules
{
    public AssetVault(ReadOnlyTargetRules Target) : base(Target)
    {
        // Использование явных или общих PCH (предварительных заголовочных файлов)
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // Публичные пути для инклюдов (для других модулей, которые будут использовать ваши заголовочные файлы)
        PublicIncludePaths.AddRange(
            new string[] {
                // Например, добавьте путь, если используете свой собственный код
                // Path.Combine(ModuleDirectory, "Public")
            }
        );

        // Приватные пути для инклюдов (для вашего собственного кода)
        PrivateIncludePaths.AddRange(
            new string[] {
                // Пример для приватных инклюдов
                // Path.Combine(ModuleDirectory, "Private")
            }
        );

        // Публичные зависимости (модули, которые статически ссылаются на ваш модуль)
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",             // Основные зависимости
                "CoreUObject",      // Основные объекты
                "Engine",           // Взаимодействие с движком
                "InputCore",        // Работа с вводом
                "EditorStyle",      // Для стилизации интерфейса
                "AssetRegistry",    // Регистрация активов
                "Projects",         // Для работы с проектами
                "Json",             // Работа с JSON
                "JsonUtilities",    // Утилиты для работы с JSON
                "DesktopPlatform",  // Для файловых диалогов
                "UnrealEd",
                "AppFramework"
            }
        );

        // Приватные зависимости (модули, используемые только внутри вашего плагина)
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "Engine",
                "Slate",            // Использование Slate для UI
                "SlateCore", "Blutility" // Основные классы для работы с Slate
            }
        );

        // Динамически загружаемые модули (если ваш плагин требует динамической загрузки модулей)
        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                // Пример для динамически загружаемых модулей
                // "YourModuleName"
            }
        );
    }
}
