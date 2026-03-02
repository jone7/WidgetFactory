// WidgetFactoryWindow.h
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FWidgetTemplateItem
{
	FString FileName;
	FString WidgetName;
	FString Description;
	bool bHasUnLua;
	bool bSelected;

	FWidgetTemplateItem(const FString& InFile, const FString& InWidget, const FString& InDesc, bool InHasUnLua)
		: FileName(InFile), WidgetName(InWidget), Description(InDesc), bHasUnLua(InHasUnLua), bSelected(false) {}
};

class SWidgetFactoryWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWidgetFactoryWindow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FWidgetTemplateItem> Item, const TSharedRef<STableViewBase>& Owner);
	void RefreshList();
	FReply OnGenerateSelected();
	FReply OnGenerateAll();
	FReply OnExportSelected();
	FReply OnRefresh();
	FReply OnOpenTemplateFolder();
	void OnCheckChanged(ECheckBoxState State, TSharedPtr<FWidgetTemplateItem> Item);

	FString OutputPath = TEXT("/Game/UI");
	FString TemplateDirRelative;

	TArray<TSharedPtr<FWidgetTemplateItem>> Items;
	TSharedPtr<SListView<TSharedPtr<FWidgetTemplateItem>>> ListView;
	TSharedPtr<class STextBlock> StatusText;
	TSharedPtr<class STextBlock> ExportSourceText;
};

class FWidgetFactoryTabManager
{
public:
	static void Register();
	static void Unregister();
	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
	static const FName TabId;
};
