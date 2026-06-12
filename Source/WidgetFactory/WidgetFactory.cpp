// WidgetFactory.cpp
#include "WidgetFactory.h"
#include "WidgetFactoryGenerator.h"
#include "WidgetFactoryWindow.h"
#include "ToolMenus.h"
#include "LevelEditor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "WidgetFactory"

namespace
{
FString NormalizePackagePath(const FString& InPath)
{
	FString Normalized = InPath;
	Normalized.TrimStartAndEndInline();
	if (Normalized.IsEmpty())
	{
		return Normalized;
	}

	FString Left;
	FString Right;
	if (Normalized.Split(TEXT("."), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd)
		&& Left.StartsWith(TEXT("/Game/")))
	{
		return Left;
	}

	return Normalized;
}

FString BuildAssetObjectPath(const FString& PackagePath)
{
	const FString NormalizedPackagePath = NormalizePackagePath(PackagePath);
	const FString AssetName = FPaths::GetBaseFilename(NormalizedPackagePath);
	if (NormalizedPackagePath.IsEmpty() || AssetName.IsEmpty())
	{
		return FString();
	}

	return FString::Printf(TEXT("%s.%s"), *NormalizedPackagePath, *AssetName);
}

UObject* LoadSavedAsset(const FString& PackagePath)
{
	const FString ObjectPath = BuildAssetObjectPath(PackagePath);
	return ObjectPath.IsEmpty() ? nullptr : LoadObject<UObject>(nullptr, *ObjectPath);
}

bool IsClassNamedOrDerived(const UClass* Class, const TCHAR* TargetClassName)
{
	for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
	{
		if (Current->GetName() == TargetClassName)
		{
			return true;
		}
	}
	return false;
}

bool LoadJsonObjectFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutJsonObject)
{
	OutJsonObject.Reset();

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FilePath))
	{
		return false;
	}

	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	return FJsonSerializer::Deserialize(Reader, OutJsonObject) && OutJsonObject.IsValid();
}

FString ResolveJsonTemplatePathField(const TSharedPtr<FJsonObject>& IngredientObject)
{
	if (!IngredientObject.IsValid())
	{
		return FString();
	}

	FString Value;
	if (IngredientObject->TryGetStringField(TEXT("currentFilePath"), Value) && !Value.IsEmpty())
	{
		return Value;
	}
	if (IngredientObject->TryGetStringField(TEXT("defaultFilePath"), Value) && !Value.IsEmpty())
	{
		return Value;
	}
	if (IngredientObject->TryGetStringField(TEXT("filePath"), Value) && !Value.IsEmpty())
	{
		return Value;
	}
	return FString();
}

bool TryResolveWidgetJsonPathFromManifest(const FString& PackagePath, FString& OutJsonPath)
{
	OutJsonPath.Reset();

	const FString MasterManifestPath = FPaths::ProjectConfigDir() / TEXT("AssetPipeline/AssetManifest.json");
	TSharedPtr<FJsonObject> MasterManifest;
	if (!LoadJsonObjectFromFile(MasterManifestPath, MasterManifest))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SubManifestValues = nullptr;
	if (!MasterManifest->TryGetArrayField(TEXT("subManifests"), SubManifestValues) || !SubManifestValues)
	{
		return false;
	}

	const FString NormalizedPackagePath = NormalizePackagePath(PackagePath);
	for (const TSharedPtr<FJsonValue>& SubManifestValue : *SubManifestValues)
	{
		FString RelativeSubManifestPath;
		if (!SubManifestValue.IsValid() || !SubManifestValue->TryGetString(RelativeSubManifestPath) || RelativeSubManifestPath.IsEmpty())
		{
			continue;
		}

		const FString FullSubManifestPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeSubManifestPath);
		TSharedPtr<FJsonObject> SubManifestObject;
		if (!LoadJsonObjectFromFile(FullSubManifestPath, SubManifestObject))
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Tasks = nullptr;
		if (!SubManifestObject->TryGetArrayField(TEXT("tasks"), Tasks) || !Tasks)
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& TaskValue : *Tasks)
		{
			const TSharedPtr<FJsonObject> TaskObject = TaskValue.IsValid() ? TaskValue->AsObject() : nullptr;
			const TSharedPtr<FJsonObject>* RecipeObjectPtr = nullptr;
			if (!TaskObject.IsValid()
				|| !TaskObject->TryGetObjectField(TEXT("recipe"), RecipeObjectPtr)
				|| !RecipeObjectPtr
				|| !RecipeObjectPtr->IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject> RecipeObject = *RecipeObjectPtr;

			FString GenerationType;
			FString OutputPath;
			if (!RecipeObject->TryGetStringField(TEXT("generationType"), GenerationType)
				|| GenerationType != TEXT("WidgetBlueprint")
				|| !RecipeObject->TryGetStringField(TEXT("outputPath"), OutputPath)
				|| NormalizePackagePath(OutputPath) != NormalizedPackagePath)
			{
				continue;
			}

			const TArray<TSharedPtr<FJsonValue>>* Ingredients = nullptr;
			if (!RecipeObject->TryGetArrayField(TEXT("ingredients"), Ingredients) || !Ingredients)
			{
				continue;
			}

			for (const TSharedPtr<FJsonValue>& IngredientValue : *Ingredients)
			{
				const TSharedPtr<FJsonObject> IngredientObject = IngredientValue.IsValid() ? IngredientValue->AsObject() : nullptr;
				FString IngredientType;
				if (!IngredientObject.IsValid()
					|| !IngredientObject->TryGetStringField(TEXT("type"), IngredientType)
					|| IngredientType != TEXT("json_template"))
				{
					continue;
				}

				OutJsonPath = ResolveJsonTemplatePathField(IngredientObject);
				return !OutJsonPath.IsEmpty();
			}
		}
	}

	return false;
}
}

void FWidgetFactoryModule::StartupModule()
{
	RegisterConsoleCommands();
	FWidgetFactoryTabManager::Register();
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWidgetFactoryModule::RegisterMenu));

	PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddLambda(
		[](const FString&, UPackage* SavedPackage, FObjectPostSaveContext)
		{
			if (!SavedPackage)
			{
				return;
			}

			const FString PackagePath = SavedPackage->GetName();
			if (PackagePath.IsEmpty() || !PackagePath.StartsWith(TEXT("/Game/")))
			{
				return;
			}

			UObject* SavedAsset = LoadSavedAsset(PackagePath);
			if (!SavedAsset || !IsClassNamedOrDerived(SavedAsset->GetClass(), TEXT("WidgetBlueprint")))
			{
				return;
			}

			FString JsonPath;
			const bool bHasExplicitJsonPath = TryResolveWidgetJsonPathFromManifest(PackagePath, JsonPath);
			if (!UWidgetFactoryGenerator::ExportToJson(PackagePath, bHasExplicitJsonPath ? JsonPath : FString()))
			{
				UE_LOG(LogTemp, Warning, TEXT("[WidgetFactory] 自动保存回写失败: %s"), *PackagePath);
				return;
			}

			const FString ExportTarget = bHasExplicitJsonPath ? JsonPath : TEXT("<default>");
			UE_LOG(LogTemp, Log, TEXT("[WidgetFactory] 保存后已回写 JSON: %s -> %s"), *PackagePath, *ExportTarget);
		});

	UE_LOG(LogTemp, Log, TEXT("[WidgetFactory] 插件已加载 (UnLua: %s)"),
		UWidgetFactoryGenerator::IsUnLuaAvailable() ? TEXT("可用") : TEXT("未安装"));
}

void FWidgetFactoryModule::ShutdownModule()
{
	if (PackageSavedHandle.IsValid())
	{
		UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
		PackageSavedHandle.Reset();
	}

	if (UObjectInitialized() && UToolMenus::Get())
	{
		UToolMenus::Get()->RemoveSection("LevelEditor.MainMenu.Tools", "WidgetFactory");
	}
	FWidgetFactoryTabManager::Unregister();
	UnregisterConsoleCommands();
}

void FWidgetFactoryModule::RegisterConsoleCommands()
{
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("WidgetFactory.Generate"),
		TEXT("从 JSON 模板生成 Widget Blueprint。用法: WidgetFactory.Generate <模板名> [输出路径]"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("用法: WidgetFactory.Generate <模板名> [/Game/UI]"));
				return;
			}
			FString Path = Args.Num() >= 2 ? Args[1] : TEXT("/Game/UI");
			UWidgetFactoryGenerator::GenerateFromJson(Args[0], Path);
		}),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("WidgetFactory.GenerateAll"),
		TEXT("批量生成 Config/WidgetTemplates/ 下所有模板"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			FString Path = Args.Num() >= 1 ? Args[0] : TEXT("/Game/UI");
			UWidgetFactoryGenerator::GenerateAllWidgets(Path);
		}),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("WidgetFactory.SetTemplateDir"),
		TEXT("设置模板目录（相对项目根目录）。用法: WidgetFactory.SetTemplateDir Config/MyTemplates"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("用法: WidgetFactory.SetTemplateDir <相对路径>"));
				UE_LOG(LogTemp, Log, TEXT("当前模板目录: %s"), *UWidgetFactoryGenerator::GetTemplateDirectory());
				return;
			}
			UWidgetFactoryGenerator::SetTemplateDirectory(Args[0]);
		}),
		ECVF_Default);

	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("WidgetFactory.Export"),
		TEXT("从 Widget Blueprint 导出 JSON 模板。用法: WidgetFactory.Export <资源路径> [输出文件名]  例: WidgetFactory.Export /Game/UI/WBP_GameHUD"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("用法: WidgetFactory.Export <资源路径> [输出文件名]"));
				UE_LOG(LogTemp, Log, TEXT("例: WidgetFactory.Export /Game/UI/WBP_GameHUD"));
				return;
			}
			FString OutputName = Args.Num() >= 2 ? Args[1] : TEXT("");
			UWidgetFactoryGenerator::ExportToJson(Args[0], OutputName);
		}),
		ECVF_Default);
}

void FWidgetFactoryModule::UnregisterConsoleCommands()
{
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("WidgetFactory.Generate"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("WidgetFactory.GenerateAll"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("WidgetFactory.SetTemplateDir"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("WidgetFactory.Export"));
}

void FWidgetFactoryModule::RegisterMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("WidgetFactory");
	Section.Label = FText::FromString(TEXT("Widget 工厂"));

	Section.AddMenuEntry(
		"OpenWidgetFactory",
		FText::FromString(TEXT("Widget 工厂")),
		FText::FromString(TEXT("从 JSON 模板生成 Widget Blueprint")),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FWidgetFactoryTabManager::TabId);
		})));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWidgetFactoryModule, WidgetFactory)
