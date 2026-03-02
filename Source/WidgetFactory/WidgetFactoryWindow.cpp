// WidgetFactoryWindow.cpp
#include "WidgetFactoryWindow.h"
#include "WidgetFactoryGenerator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FName FWidgetFactoryTabManager::TabId = FName("WidgetFactoryTab");

// Chinese text constants (UTF-8)
#define WF_TEXT_GENERATE_SELECTED  TEXT("生成选中")
#define WF_TEXT_GENERATE_ALL       TEXT("全部生成")
#define WF_TEXT_REFRESH            TEXT("刷新列表")
#define WF_TEXT_OPEN_FOLDER        TEXT("打开模板目录")
#define WF_TEXT_OUTPUT_PATH        TEXT("输出路径:")
#define WF_TEXT_TEMPLATE_DIR       TEXT("模板目录:")
#define WF_TEXT_UNLUA_TAG          TEXT("[UnLua]")
#define WF_TEXT_STATUS_READY       TEXT("就绪")
#define WF_TEXT_UNLUA_AVAILABLE    TEXT("UnLua: 可用")
#define WF_TEXT_UNLUA_UNAVAILABLE  TEXT("UnLua: 未安装")

void SWidgetFactoryWindow::Construct(const FArguments& InArgs)
{
	// Initialize template dir from current setting
	TemplateDirRelative = TEXT("Config/WidgetTemplates");

	RefreshList();

	FString UnLuaStatus = UWidgetFactoryGenerator::IsUnLuaAvailable()
		? WF_TEXT_UNLUA_AVAILABLE : WF_TEXT_UNLUA_UNAVAILABLE;

	ChildSlot
	[
		SNew(SVerticalBox)

		// 工具栏
		+ SVerticalBox::Slot().AutoHeight().Padding(8)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(WF_TEXT_GENERATE_SELECTED))
				.OnClicked(this, &SWidgetFactoryWindow::OnGenerateSelected)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(WF_TEXT_GENERATE_ALL))
				.OnClicked(this, &SWidgetFactoryWindow::OnGenerateAll)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 4, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(WF_TEXT_REFRESH))
				.OnClicked(this, &SWidgetFactoryWindow::OnRefresh)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
			[
				SNew(SButton)
				.Text(FText::FromString(WF_TEXT_OPEN_FOLDER))
				.OnClicked(this, &SWidgetFactoryWindow::OnOpenTemplateFolder)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(UnLuaStatus))
				.ColorAndOpacity(UWidgetFactoryGenerator::IsUnLuaAvailable()
					? FSlateColor(FLinearColor(0.2f, 0.8f, 0.4f))
					: FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
			]
		]

		// 模板目录
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 2)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(WF_TEXT_TEMPLATE_DIR))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(TemplateDirRelative))
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					TemplateDirRelative = Text.ToString();
					UWidgetFactoryGenerator::SetTemplateDirectory(TemplateDirRelative);
					RefreshList();
				})
			]
		]

		// 输出路径
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 2, 8, 4)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
			[
				SNew(STextBlock).Text(FText::FromString(WF_TEXT_OUTPUT_PATH))
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text(FText::FromString(OutputPath))
				.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type)
				{
					OutputPath = Text.ToString();
				})
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(8, 0)
		[
			SNew(SSeparator)
		]

		// 模板列表
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(8)
		[
			SAssignNew(ListView, SListView<TSharedPtr<FWidgetTemplateItem>>)
			.ListItemsSource(&Items)
			.OnGenerateRow(this, &SWidgetFactoryWindow::OnGenerateRow)
		]

		// 状态栏
		+ SVerticalBox::Slot().AutoHeight().Padding(8, 4)
		[
			SAssignNew(StatusText, STextBlock)
			.Text(FText::FromString(WF_TEXT_STATUS_READY))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
		]
	];
}


TSharedRef<ITableRow> SWidgetFactoryWindow::OnGenerateRow(
	TSharedPtr<FWidgetTemplateItem> Item,
	const TSharedRef<STableViewBase>& Owner)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(4, 2)
		[
			SNew(SCheckBox)
			.IsChecked(Item->bSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
			.OnCheckStateChanged(this, &SWidgetFactoryWindow::OnCheckChanged, Item)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4, 2).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString(Item->FileName))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(12, 2, 4, 2).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("%s %s"), UTF8_TO_TCHAR("\xe2\x86\x92"), *Item->WidgetName)))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.8f, 1.0f)))
		];

	// UnLua tag
	if (Item->bHasUnLua)
	{
		Row->AddSlot()
			.AutoWidth()
			.Padding(8, 2, 4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(WF_TEXT_UNLUA_TAG))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.8f, 0.4f)))
			];
	}

	// Description
	if (!Item->Description.IsEmpty())
	{
		Row->AddSlot()
			.FillWidth(1.0f)
			.Padding(8, 2, 4, 2)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Description))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
			];
	}

	return SNew(STableRow<TSharedPtr<FWidgetTemplateItem>>, Owner)[ Row ];
}

void SWidgetFactoryWindow::RefreshList()
{
	Items.Empty();
	FString ConfigDir = UWidgetFactoryGenerator::GetTemplateDirectory();

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(ConfigDir / TEXT("*.json")), true, false);

	for (const FString& File : Files)
	{
		FString BaseName = FPaths::GetBaseFilename(File);
		FString JsonPath = ConfigDir / File;
		FString Content;
		FString WidgetName = BaseName;
		FString Desc;
		bool bHasUnLua = false;

		if (FFileHelper::LoadFileToString(Content, *JsonPath))
		{
			TSharedPtr<FJsonObject> Json;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
			if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
			{
				Json->TryGetStringField(TEXT("WidgetName"), WidgetName);
				Json->TryGetStringField(TEXT("Description"), Desc);

				const TSharedPtr<FJsonObject>* UnLuaObj;
				if (Json->TryGetObjectField(TEXT("UnLuaBinding"), UnLuaObj))
				{
					bool bEnabled;
					if ((*UnLuaObj)->TryGetBoolField(TEXT("Enabled"), bEnabled) && bEnabled)
						bHasUnLua = true;
				}
			}
		}
		Items.Add(MakeShared<FWidgetTemplateItem>(BaseName, WidgetName, Desc, bHasUnLua));
	}

	if (ListView.IsValid()) ListView->RequestListRefresh();

	if (StatusText.IsValid())
	{
		StatusText->SetText(FText::FromString(
			FString::Printf(TEXT("共 %d 个模板  |  目录: %s"), Items.Num(), *ConfigDir)));
	}
}

void SWidgetFactoryWindow::OnCheckChanged(ECheckBoxState State, TSharedPtr<FWidgetTemplateItem> Item)
{
	if (Item.IsValid()) Item->bSelected = (State == ECheckBoxState::Checked);
}

FReply SWidgetFactoryWindow::OnGenerateSelected()
{
	int32 Total = 0, Ok = 0;
	for (const auto& Item : Items)
	{
		if (Item.IsValid() && Item->bSelected)
		{
			Total++;
			if (UWidgetFactoryGenerator::GenerateFromJson(Item->FileName, OutputPath)) Ok++;
		}
	}

	if (StatusText.IsValid())
	{
		if (Total == 0)
			StatusText->SetText(FText::FromString(TEXT("请先勾选要生成的模板")));
		else
			StatusText->SetText(FText::FromString(
				FString::Printf(TEXT("生成完成: %d/%d 成功"), Ok, Total)));
	}
	return FReply::Handled();
}

FReply SWidgetFactoryWindow::OnGenerateAll()
{
	UWidgetFactoryGenerator::GenerateAllWidgets(OutputPath);

	if (StatusText.IsValid())
		StatusText->SetText(FText::FromString(TEXT("全部生成完成")));

	return FReply::Handled();
}

FReply SWidgetFactoryWindow::OnRefresh()
{
	RefreshList();
	return FReply::Handled();
}

FReply SWidgetFactoryWindow::OnOpenTemplateFolder()
{
	FString Dir = UWidgetFactoryGenerator::GetTemplateDirectory();
	// Create directory if it doesn't exist
	if (!FPaths::DirectoryExists(Dir))
	{
		IFileManager::Get().MakeDirectory(*Dir, true);
	}
	FPlatformProcess::ExploreFolder(*Dir);
	return FReply::Handled();
}

// ─── Tab Manager ────────────────────────────────────────────────────────────

void FWidgetFactoryTabManager::Register()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId,
		FOnSpawnTab::CreateStatic(&FWidgetFactoryTabManager::SpawnTab))
		.SetDisplayName(FText::FromString(TEXT("Widget 工厂")))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FWidgetFactoryTabManager::Unregister()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
}

TSharedRef<SDockTab> FWidgetFactoryTabManager::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).TabRole(ETabRole::NomadTab)
	[
		SNew(SWidgetFactoryWindow)
	];
}
