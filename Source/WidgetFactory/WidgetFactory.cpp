// WidgetFactory.cpp
#include "WidgetFactory.h"
#include "WidgetFactoryGenerator.h"
#include "WidgetFactoryWindow.h"
#include "ToolMenus.h"
#include "LevelEditor.h"

#define LOCTEXT_NAMESPACE "WidgetFactory"

void FWidgetFactoryModule::StartupModule()
{
	RegisterConsoleCommands();
	FWidgetFactoryTabManager::Register();
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWidgetFactoryModule::RegisterMenu));

	UE_LOG(LogTemp, Log, TEXT("[WidgetFactory] 插件已加载 (UnLua: %s)"),
		UWidgetFactoryGenerator::IsUnLuaAvailable() ? TEXT("可用") : TEXT("未安装"));
}

void FWidgetFactoryModule::ShutdownModule()
{
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
