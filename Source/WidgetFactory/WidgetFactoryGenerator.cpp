// WidgetFactoryGenerator.cpp
#include "WidgetFactoryGenerator.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Spacer.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/GridPanel.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/RichTextBlock.h"
#include "Components/ScaleBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Throbber.h"
#include "Components/CircularThrobber.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionResult.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Regex.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/LinkerLoad.h"
#include "Editor.h"

#if WITH_UNLUA
#include "UnLuaInterface.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWidgetFactory, Log, All);

// ─── Static member ──────────────────────────────────────────────────────────

FString UWidgetFactoryGenerator::CustomTemplateDir;

// ─── Widget class registry ──────────────────────────────────────────────────

static TMap<FString, UClass*> GWidgetClassMap;

static void EnsureClassMapInitialized()
{
	if (GWidgetClassMap.Num() > 0) return;

	GWidgetClassMap.Add(TEXT("CanvasPanel"),       UCanvasPanel::StaticClass());
	GWidgetClassMap.Add(TEXT("VerticalBox"),        UVerticalBox::StaticClass());
	GWidgetClassMap.Add(TEXT("HorizontalBox"),      UHorizontalBox::StaticClass());
	GWidgetClassMap.Add(TEXT("ScrollBox"),           UScrollBox::StaticClass());
	GWidgetClassMap.Add(TEXT("Overlay"),             UOverlay::StaticClass());
	GWidgetClassMap.Add(TEXT("SizeBox"),             USizeBox::StaticClass());
	GWidgetClassMap.Add(TEXT("ScaleBox"),            UScaleBox::StaticClass());
	GWidgetClassMap.Add(TEXT("Border"),              UBorder::StaticClass());
	GWidgetClassMap.Add(TEXT("GridPanel"),           UGridPanel::StaticClass());
	GWidgetClassMap.Add(TEXT("UniformGridPanel"),    UUniformGridPanel::StaticClass());
	GWidgetClassMap.Add(TEXT("WrapBox"),             UWrapBox::StaticClass());
	GWidgetClassMap.Add(TEXT("WidgetSwitcher"),      UWidgetSwitcher::StaticClass());
	GWidgetClassMap.Add(TEXT("Button"),              UButton::StaticClass());
	GWidgetClassMap.Add(TEXT("TextBlock"),           UTextBlock::StaticClass());
	GWidgetClassMap.Add(TEXT("RichTextBlock"),       URichTextBlock::StaticClass());
	GWidgetClassMap.Add(TEXT("Image"),               UImage::StaticClass());
	GWidgetClassMap.Add(TEXT("Spacer"),              USpacer::StaticClass());
	GWidgetClassMap.Add(TEXT("ProgressBar"),         UProgressBar::StaticClass());
	GWidgetClassMap.Add(TEXT("Slider"),              USlider::StaticClass());
	GWidgetClassMap.Add(TEXT("CheckBox"),            UCheckBox::StaticClass());
	GWidgetClassMap.Add(TEXT("EditableText"),        UEditableText::StaticClass());
	GWidgetClassMap.Add(TEXT("EditableTextBox"),     UEditableTextBox::StaticClass());
	GWidgetClassMap.Add(TEXT("ComboBoxString"),      UComboBoxString::StaticClass());
	GWidgetClassMap.Add(TEXT("Throbber"),            UThrobber::StaticClass());
	GWidgetClassMap.Add(TEXT("CircularThrobber"),    UCircularThrobber::StaticClass());
}

UClass* UWidgetFactoryGenerator::GetWidgetClass(const FString& TypeName)
{
	EnsureClassMapInitialized();
	if (UClass** Found = GWidgetClassMap.Find(TypeName))
	{
		return *Found;
	}
	UE_LOG(LogWidgetFactory, Warning, TEXT("未知控件类型: %s"), *TypeName);
	return nullptr;
}

// ─── Template directory ─────────────────────────────────────────────────────

FString UWidgetFactoryGenerator::GetTemplateDirectory()
{
	if (!CustomTemplateDir.IsEmpty())
	{
		return FPaths::ProjectDir() / CustomTemplateDir;
	}
	return FPaths::ProjectDir() / TEXT("Config") / TEXT("WidgetTemplates");
}

void UWidgetFactoryGenerator::SetTemplateDirectory(const FString& RelativePath)
{
	CustomTemplateDir = RelativePath;
	UE_LOG(LogWidgetFactory, Log, TEXT("模板目录已设置为: %s"), *GetTemplateDirectory());
}

// ─── UnLua detection ────────────────────────────────────────────────────────

bool UWidgetFactoryGenerator::IsUnLuaAvailable()
{
#if WITH_UNLUA
	return true;
#else
	return false;
#endif
}

// ─── Helpers ────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> UWidgetFactoryGenerator::LoadJsonConfig(const FString& JsonPath)
{
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *JsonPath))
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("无法读取文件: %s"), *JsonPath);
		return nullptr;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
	if (!FJsonSerializer::Deserialize(Reader, Json))
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("JSON 解析失败: %s"), *JsonPath);
		return nullptr;
	}
	return Json;
}


// ─── Blueprint creation ─────────────────────────────────────────────────────

UWidgetBlueprint* UWidgetFactoryGenerator::CreateWidgetBlueprint(const FString& PackagePath, const FString& WidgetName)
{
	FString FullPath = PackagePath / WidgetName;
	FString AssetFile = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetFile))
	{
		UE_LOG(LogWidgetFactory, Warning, TEXT("覆盖已有资源: %s"), *FullPath);

		// 1. 关闭该资产的编辑器 Tab（防止编辑器持有引用导致 GC 悬空指针）
		UPackage* OldPkg = FindPackage(nullptr, *FullPath);
		if (OldPkg)
		{
			TArray<UObject*> AssetsInPkg;
			ForEachObjectWithPackage(OldPkg, [&AssetsInPkg](UObject* Obj) { AssetsInPkg.Add(Obj); return true; }, false);

			// 关闭所有引用该资产的编辑器
			for (UObject* Obj : AssetsInPkg)
			{
				if (Obj && Obj->IsAsset())
				{
					GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->CloseAllEditorsForAsset(Obj);
				}
			}

			// 2. 将旧 Package 重命名到临时路径（避免同名冲突），清除引用
			FString TrashName = FString::Printf(TEXT("/Temp/WidgetFactory_Trash_%s_%d"), *WidgetName, FMath::Rand());

			// 2a. 先清理 Blueprint 的 GeneratedClass/CDO（防止 GC 悬空指针）
			for (UObject* Obj : AssetsInPkg)
			{
				UBlueprint* AsBP = Cast<UBlueprint>(Obj);
				if (AsBP)
				{
					if (AsBP->GeneratedClass)
					{
						UClass* OldClass = AsBP->GeneratedClass;
						UObject* OldCDO = OldClass->GetDefaultObject(false);
						if (OldCDO)
						{
							OldCDO->ClearFlags(RF_Standalone | RF_Public);
							OldCDO->SetFlags(RF_Transient);
							OldCDO->MarkAsGarbage();
						}
						OldClass->ClearFlags(RF_Standalone | RF_Public);
						OldClass->SetFlags(RF_Transient);
						OldClass->MarkAsGarbage();
						AsBP->GeneratedClass = nullptr;
					}
					if (AsBP->SkeletonGeneratedClass)
					{
						UClass* OldSkel = AsBP->SkeletonGeneratedClass;
						UObject* OldSkelCDO = OldSkel->GetDefaultObject(false);
						if (OldSkelCDO)
						{
							OldSkelCDO->ClearFlags(RF_Standalone | RF_Public);
							OldSkelCDO->SetFlags(RF_Transient);
							OldSkelCDO->MarkAsGarbage();
						}
						OldSkel->ClearFlags(RF_Standalone | RF_Public);
						OldSkel->SetFlags(RF_Transient);
						OldSkel->MarkAsGarbage();
						AsBP->SkeletonGeneratedClass = nullptr;
					}
				}
			}

			if (OldPkg->Rename(*TrashName, nullptr, REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders))
			{
				OldPkg->ClearFlags(RF_Standalone | RF_Public);
				OldPkg->SetFlags(RF_Transient);
				for (UObject* Obj : AssetsInPkg)
				{
					if (Obj)
					{
						Obj->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
						Obj->ClearFlags(RF_Standalone | RF_Public);
						Obj->SetFlags(RF_Transient);
						Obj->MarkAsGarbage();
					}
				}
				OldPkg->MarkAsGarbage();
			}
			else
			{
				// Rename 失败，回退到旧方式
				UE_LOG(LogWidgetFactory, Warning, TEXT("Rename 旧 Package 失败，尝试直接清理"));
				for (UObject* Obj : AssetsInPkg)
				{
					if (Obj) { Obj->ClearFlags(RF_Standalone | RF_Public); Obj->SetFlags(RF_Transient); Obj->MarkAsGarbage(); }
				}
				OldPkg->ClearFlags(RF_Standalone);
				OldPkg->SetFlags(RF_Transient);
				OldPkg->MarkAsGarbage();
			}

			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// 3. 释放文件句柄
			ResetLoaders(OldPkg);
		}

		IFileManager::Get().Delete(*AssetFile, false, true);
	}

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package) { UE_LOG(LogWidgetFactory, Error, TEXT("创建 Package 失败: %s"), *FullPath); return nullptr; }

	UWidgetBlueprint* BP = CastChecked<UWidgetBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(
			UUserWidget::StaticClass(), Package, FName(*WidgetName),
			BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass()));

	if (!BP) { UE_LOG(LogWidgetFactory, Error, TEXT("创建 Blueprint 失败: %s"), *WidgetName); }
	return BP;
}

// ─── Property setters ───────────────────────────────────────────────────────

static FLinearColor ParseColor(const TSharedPtr<FJsonObject>& Obj)
{
	return FLinearColor(
		Obj->GetNumberField(TEXT("R")),
		Obj->GetNumberField(TEXT("G")),
		Obj->GetNumberField(TEXT("B")),
		Obj->GetNumberField(TEXT("A")));
}

static FMargin ParseMargin(const TSharedPtr<FJsonObject>& Obj)
{
	return FMargin(
		Obj->GetNumberField(TEXT("Left")),
		Obj->GetNumberField(TEXT("Top")),
		Obj->GetNumberField(TEXT("Right")),
		Obj->GetNumberField(TEXT("Bottom")));
}

void UWidgetFactoryGenerator::SetWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& Props)
{
	if (!Widget || !Props.IsValid()) return;

	// Visibility
	FString Vis;
	if (Props->TryGetStringField(TEXT("Visibility"), Vis))
	{
		if      (Vis == TEXT("Collapsed"))           Widget->SetVisibility(ESlateVisibility::Collapsed);
		else if (Vis == TEXT("Hidden"))              Widget->SetVisibility(ESlateVisibility::Hidden);
		else if (Vis == TEXT("HitTestInvisible"))    Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
		else if (Vis == TEXT("SelfHitTestInvisible"))Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		else Widget->SetVisibility(ESlateVisibility::Visible);
	}

	double Opacity;
	if (Props->TryGetNumberField(TEXT("RenderOpacity"), Opacity))
		Widget->SetRenderOpacity(Opacity);

	// TextBlock
	if (UTextBlock* TB = Cast<UTextBlock>(Widget))
	{
		FString Text;
		if (Props->TryGetStringField(TEXT("Text"), Text)) TB->SetText(FText::FromString(Text));

		int32 FontSize;
		if (Props->TryGetNumberField(TEXT("FontSize"), FontSize))
		{
			FSlateFontInfo Font = TB->GetFont(); Font.Size = FontSize; TB->SetFont(Font);
		}

		FString FontFamily;
		if (Props->TryGetStringField(TEXT("FontFamily"), FontFamily))
		{
			FSlateFontInfo Font = TB->GetFont();
			UObject* FontObj = LoadObject<UObject>(nullptr, *FontFamily);
			if (FontObj) Font.FontObject = FontObj;
			TB->SetFont(Font);
		}

		const TSharedPtr<FJsonObject>* ColorObj;
		if (Props->TryGetObjectField(TEXT("Color"), ColorObj))
			TB->SetColorAndOpacity(FSlateColor(ParseColor(*ColorObj)));

		bool bWrap;
		if (Props->TryGetBoolField(TEXT("AutoWrap"), bWrap) && bWrap)
			TB->SetAutoWrapText(true);

		FString Justify;
		if (Props->TryGetStringField(TEXT("Justification"), Justify))
		{
			if      (Justify == TEXT("Center")) TB->SetJustification(ETextJustify::Center);
			else if (Justify == TEXT("Right"))  TB->SetJustification(ETextJustify::Right);
			else TB->SetJustification(ETextJustify::Left);
		}
	}

	// Image
	if (UImage* Img = Cast<UImage>(Widget))
	{
		const TSharedPtr<FJsonObject>* ColorObj;
		if (Props->TryGetObjectField(TEXT("Color"), ColorObj))
			Img->SetColorAndOpacity(ParseColor(*ColorObj));

		FString BrushPath;
		if (Props->TryGetStringField(TEXT("Brush"), BrushPath))
		{
			UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *BrushPath);
			if (Tex) Img->SetBrushFromTexture(Tex);
		}
	}

	// Spacer
	if (USpacer* Sp = Cast<USpacer>(Widget))
	{
		double Size;
		if (Props->TryGetNumberField(TEXT("Size"), Size))
			Sp->SetSize(FVector2D(Size, Size));
	}

	// ProgressBar
	if (UProgressBar* PB = Cast<UProgressBar>(Widget))
	{
		double Pct;
		if (Props->TryGetNumberField(TEXT("Percent"), Pct)) PB->SetPercent(Pct);
		const TSharedPtr<FJsonObject>* FillColor;
		if (Props->TryGetObjectField(TEXT("FillColor"), FillColor))
			PB->SetFillColorAndOpacity(ParseColor(*FillColor));
	}

	// SizeBox
	if (USizeBox* SB = Cast<USizeBox>(Widget))
	{
		double W, H;
		if (Props->TryGetNumberField(TEXT("WidthOverride"), W))    SB->SetWidthOverride(W);
		if (Props->TryGetNumberField(TEXT("HeightOverride"), H))   SB->SetHeightOverride(H);
		if (Props->TryGetNumberField(TEXT("MinDesiredWidth"), W))  SB->SetMinDesiredWidth(W);
		if (Props->TryGetNumberField(TEXT("MinDesiredHeight"), H)) SB->SetMinDesiredHeight(H);
		if (Props->TryGetNumberField(TEXT("MaxDesiredWidth"), W))  SB->SetMaxDesiredWidth(W);
		if (Props->TryGetNumberField(TEXT("MaxDesiredHeight"), H)) SB->SetMaxDesiredHeight(H);
	}
}


// ─── Slot property setters ──────────────────────────────────────────────────

static EHorizontalAlignment ParseHAlign(const FString& S)
{
	if (S == TEXT("Center")) return HAlign_Center;
	if (S == TEXT("Right"))  return HAlign_Right;
	if (S == TEXT("Fill"))   return HAlign_Fill;
	return HAlign_Left;
}

static EVerticalAlignment ParseVAlign(const FString& S)
{
	if (S == TEXT("Center")) return VAlign_Center;
	if (S == TEXT("Bottom")) return VAlign_Bottom;
	if (S == TEXT("Fill"))   return VAlign_Fill;
	return VAlign_Top;
}

void UWidgetFactoryGenerator::SetCanvasSlotProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& Slot)
{
	UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Widget->Slot);
	if (!CS) return;

	FString Anchors;
	if (Slot->TryGetStringField(TEXT("Anchors"), Anchors))
	{
		if (Anchors == TEXT("Fill"))
		{
			CS->SetAnchors(FAnchors(0, 0, 1, 1));
			CS->SetOffsets(FMargin(0));
		}
		else if (Anchors == TEXT("Center"))
		{
			CS->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			CS->SetAlignment(FVector2D(0.5f, 0.5f));
		}
		else if (Anchors == TEXT("TopLeft"))      CS->SetAnchors(FAnchors(0, 0, 0, 0));
		else if (Anchors == TEXT("TopCenter"))    { CS->SetAnchors(FAnchors(0.5f, 0, 0.5f, 0)); CS->SetAlignment(FVector2D(0.5f, 0)); }
		else if (Anchors == TEXT("TopRight"))     { CS->SetAnchors(FAnchors(1, 0, 1, 0)); CS->SetAlignment(FVector2D(1, 0)); }
		else if (Anchors == TEXT("BottomLeft"))   { CS->SetAnchors(FAnchors(0, 1, 0, 1)); CS->SetAlignment(FVector2D(0, 1)); }
		else if (Anchors == TEXT("BottomCenter")) { CS->SetAnchors(FAnchors(0.5f, 1, 0.5f, 1)); CS->SetAlignment(FVector2D(0.5f, 1)); }
		else if (Anchors == TEXT("BottomRight"))  { CS->SetAnchors(FAnchors(1, 1, 1, 1)); CS->SetAlignment(FVector2D(1, 1)); }
		else if (Anchors == TEXT("CenterLeft"))   { CS->SetAnchors(FAnchors(0, 0.5f, 0, 0.5f)); CS->SetAlignment(FVector2D(0, 0.5f)); }
		else if (Anchors == TEXT("CenterRight"))  { CS->SetAnchors(FAnchors(1, 0.5f, 1, 0.5f)); CS->SetAlignment(FVector2D(1, 0.5f)); }
		else if (Anchors == TEXT("FillHorizontal")) CS->SetAnchors(FAnchors(0, 0.5f, 1, 0.5f));
		else if (Anchors == TEXT("FillVertical"))   CS->SetAnchors(FAnchors(0.5f, 0, 0.5f, 1));
	}

	const TSharedPtr<FJsonObject>* AnchorsObj;
	if (Slot->TryGetObjectField(TEXT("AnchorsCustom"), AnchorsObj))
	{
		CS->SetAnchors(FAnchors(
			(*AnchorsObj)->GetNumberField(TEXT("MinX")),
			(*AnchorsObj)->GetNumberField(TEXT("MinY")),
			(*AnchorsObj)->GetNumberField(TEXT("MaxX")),
			(*AnchorsObj)->GetNumberField(TEXT("MaxY"))));
	}

	const TSharedPtr<FJsonObject>* PosObj;
	if (Slot->TryGetObjectField(TEXT("Position"), PosObj))
		CS->SetPosition(FVector2D((*PosObj)->GetNumberField(TEXT("X")), (*PosObj)->GetNumberField(TEXT("Y"))));

	const TSharedPtr<FJsonObject>* SizeObj;
	if (Slot->TryGetObjectField(TEXT("Size"), SizeObj))
		CS->SetSize(FVector2D((*SizeObj)->GetNumberField(TEXT("Width")), (*SizeObj)->GetNumberField(TEXT("Height"))));

	const TSharedPtr<FJsonObject>* AlignObj;
	if (Slot->TryGetObjectField(TEXT("Alignment"), AlignObj))
		CS->SetAlignment(FVector2D((*AlignObj)->GetNumberField(TEXT("X")), (*AlignObj)->GetNumberField(TEXT("Y"))));

	const TSharedPtr<FJsonObject>* OffsetObj;
	if (Slot->TryGetObjectField(TEXT("Offsets"), OffsetObj))
		CS->SetOffsets(ParseMargin(*OffsetObj));

	bool bAutoSize;
	if (Slot->TryGetBoolField(TEXT("AutoSize"), bAutoSize))
		CS->SetAutoSize(bAutoSize);

	int32 ZOrder;
	if (Slot->TryGetNumberField(TEXT("ZOrder"), ZOrder))
		CS->SetZOrder(ZOrder);
}

void UWidgetFactoryGenerator::SetSlotProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& Slot)
{
	if (!Widget || !Slot.IsValid() || !Widget->Slot) return;

	// HorizontalBoxSlot
	if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		FString SizeRule;
		if (Slot->TryGetStringField(TEXT("SizeRule"), SizeRule))
			HS->SetSize(FSlateChildSize(SizeRule == TEXT("Fill") ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		double FillW;
		if (Slot->TryGetNumberField(TEXT("FillWidth"), FillW))
		{
			FSlateChildSize S(ESlateSizeRule::Fill); S.Value = FillW; HS->SetSize(S);
		}
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) HS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) HS->SetVerticalAlignment(ParseVAlign(VA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) HS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// VerticalBoxSlot
	if (UVerticalBoxSlot* VS = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		FString SizeRule;
		if (Slot->TryGetStringField(TEXT("SizeRule"), SizeRule))
			VS->SetSize(FSlateChildSize(SizeRule == TEXT("Fill") ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic));
		double FillH;
		if (Slot->TryGetNumberField(TEXT("FillHeight"), FillH))
		{
			FSlateChildSize S(ESlateSizeRule::Fill); S.Value = FillH; VS->SetSize(S);
		}
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) VS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) VS->SetVerticalAlignment(ParseVAlign(VA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) VS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// ScrollBoxSlot
	if (UScrollBoxSlot* SS = Cast<UScrollBoxSlot>(Widget->Slot))
	{
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) SS->SetHorizontalAlignment(ParseHAlign(HA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) SS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// OverlaySlot
	if (UOverlaySlot* OS = Cast<UOverlaySlot>(Widget->Slot))
	{
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) OS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) OS->SetVerticalAlignment(ParseVAlign(VA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) OS->SetPadding(ParseMargin(*Pad));
		return;
	}

	SetCanvasSlotProperties(Widget, Slot);
}


// ─── Widget tree construction ───────────────────────────────────────────────

void UWidgetFactoryGenerator::SetWidgetAsVariable(UWidget* Widget, const FString& Name)
{
	if (!Widget || Name.IsEmpty()) return;
	Widget->bIsVariable = true;

	// Check if an object with this name already exists in the same outer
	UObject* Existing = StaticFindObjectFast(nullptr, Widget->GetOuter(), FName(*Name));
	if (Existing && Existing != Widget)
	{
		// Append a unique suffix to avoid fatal rename collision
		FString UniqueName = FString::Printf(TEXT("%s_%d"), *Name, FMath::Rand());
		UE_LOG(LogWidgetFactory, Warning, TEXT("命名冲突: %s 已存在，改用 %s"), *Name, *UniqueName);
		Widget->Rename(*UniqueName);
	}
	else
	{
		Widget->Rename(*Name);
	}
}

UWidget* UWidgetFactoryGenerator::CreateWidgetFromJson(
	UWidgetTree* WidgetTree,
	const TSharedPtr<FJsonObject>& Json,
	UPanelWidget* Parent)
{
	if (!Json.IsValid()) return nullptr;

	FString TypeName;
	if (!Json->TryGetStringField(TEXT("Type"), TypeName)) return nullptr;

	UClass* WidgetClass = GetWidgetClass(TypeName);
	if (!WidgetClass) return nullptr;

	UWidget* NewWidget = WidgetTree->ConstructWidget<UWidget>(WidgetClass);
	if (!NewWidget) { UE_LOG(LogWidgetFactory, Error, TEXT("创建控件失败: %s"), *TypeName); return nullptr; }

	// Properties
	const TSharedPtr<FJsonObject>* PropsJson;
	if (Json->TryGetObjectField(TEXT("Properties"), PropsJson))
		SetWidgetProperties(NewWidget, *PropsJson);

	// Add to parent or set as root
	if (Parent)
	{
		Parent->AddChild(NewWidget);
		const TSharedPtr<FJsonObject>* SlotJson;
		if (Json->TryGetObjectField(TEXT("Slot"), SlotJson))
			SetSlotProperties(NewWidget, *SlotJson);
		else if (PropsJson && PropsJson->IsValid())
			SetSlotProperties(NewWidget, *PropsJson);
	}
	else
	{
		WidgetTree->RootWidget = NewWidget;
	}

	// IsVariable
	bool bIsVar = false;
	FString WidgetName;
	Json->TryGetStringField(TEXT("Name"), WidgetName);
	Json->TryGetBoolField(TEXT("IsVariable"), bIsVar);
	if (bIsVar && !WidgetName.IsEmpty())
		SetWidgetAsVariable(NewWidget, WidgetName);

	// Recurse children
	const TArray<TSharedPtr<FJsonValue>>* Children;
	if (Json->TryGetArrayField(TEXT("Children"), Children))
	{
		UPanelWidget* Panel = Cast<UPanelWidget>(NewWidget);
		if (Panel)
		{
			for (const TSharedPtr<FJsonValue>& ChildVal : *Children)
			{
				const TSharedPtr<FJsonObject>* ChildObj;
				if (ChildVal->TryGetObject(ChildObj))
					CreateWidgetFromJson(WidgetTree, *ChildObj, Panel);
			}
		}
	}

	return NewWidget;
}

// ─── UnLua binding (optional) ───────────────────────────────────────────────

void UWidgetFactoryGenerator::SetupUnLuaBinding(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& UnLuaConfig)
{
#if WITH_UNLUA
	if (!WidgetBP || !UnLuaConfig.IsValid()) return;

	bool bEnabled;
	if (!UnLuaConfig->TryGetBoolField(TEXT("Enabled"), bEnabled) || !bEnabled) return;

	FString ModuleName;
	if (!UnLuaConfig->TryGetStringField(TEXT("ModuleName"), ModuleName)) return;

	UClass* UnLuaInterfaceClass = UUnLuaInterface::StaticClass();
	if (!UnLuaInterfaceClass) return;

	// Check if already implemented
	bool bAlreadyImplemented = false;
	for (const FBPInterfaceDescription& Iface : WidgetBP->ImplementedInterfaces)
	{
		if (Iface.Interface == UnLuaInterfaceClass) { bAlreadyImplemented = true; break; }
	}

	if (!bAlreadyImplemented)
	{
		FBPInterfaceDescription InterfaceDesc;
		InterfaceDesc.Interface = UnLuaInterfaceClass;
		WidgetBP->ImplementedInterfaces.Add(InterfaceDesc);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
		FTopLevelAssetPath InterfacePath(UnLuaInterfaceClass->GetPackage()->GetFName(), UnLuaInterfaceClass->GetFName());
		FBlueprintEditorUtils::ImplementNewInterface(WidgetBP, InterfacePath);
#else
		FBlueprintEditorUtils::ImplementNewInterface(WidgetBP, FName(*UnLuaInterfaceClass->GetPathName()));
#endif
		UE_LOG(LogWidgetFactory, Log, TEXT("已添加 UnLuaInterface: %s"), *WidgetBP->GetName());
	}

	FKismetEditorUtilities::CompileBlueprint(WidgetBP);

	// Find GetModuleName graph and set return value
	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(WidgetBP->FunctionGraphs);
	for (FBPInterfaceDescription& InterfaceDesc : WidgetBP->ImplementedInterfaces)
		AllGraphs.Append(InterfaceDesc.Graphs);

	bool bFoundAndSet = false;
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Contains(TEXT("GetModuleName")))
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node);
				if (!ResultNode) continue;

				for (UEdGraphPin* Pin : ResultNode->Pins)
				{
					if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
					{
						Pin->DefaultValue = ModuleName;
						Pin->DefaultTextValue = FText::GetEmpty();
						bFoundAndSet = true;
						UE_LOG(LogWidgetFactory, Log, TEXT("设置 GetModuleName 返回值: %s"), *ModuleName);
					}
				}
				if (bFoundAndSet) { ResultNode->Modify(); break; }
			}
			if (bFoundAndSet) { Graph->Modify(); break; }
		}
	}

	if (bFoundAndSet)
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(WidgetBP);
		FKismetEditorUtilities::CompileBlueprint(WidgetBP);
	}
	else
	{
		UE_LOG(LogWidgetFactory, Warning, TEXT("未找到 GetModuleName 返回引脚"));
	}
#else
	if (UnLuaConfig.IsValid())
	{
		bool bEnabled;
		if (UnLuaConfig->TryGetBoolField(TEXT("Enabled"), bEnabled) && bEnabled)
		{
			UE_LOG(LogWidgetFactory, Warning, TEXT("JSON 中配置了 UnLua 绑定，但 UnLua 插件未安装，已跳过"));
		}
	}
#endif
}

void UWidgetFactoryGenerator::AddEventTickNode(UWidgetBlueprint* WidgetBP)
{
	UE_LOG(LogWidgetFactory, Log, TEXT("请手动在 EventGraph 中添加 Event Tick 节点"));
}


// ─── Export: Widget → JSON ───────────────────────────────────────────────────

TSharedPtr<FJsonObject> UWidgetFactoryGenerator::ColorToJson(const FLinearColor& C)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("R"), FMath::RoundToFloat(C.R * 1000) / 1000);
	Obj->SetNumberField(TEXT("G"), FMath::RoundToFloat(C.G * 1000) / 1000);
	Obj->SetNumberField(TEXT("B"), FMath::RoundToFloat(C.B * 1000) / 1000);
	Obj->SetNumberField(TEXT("A"), FMath::RoundToFloat(C.A * 1000) / 1000);
	return Obj;
}

TSharedPtr<FJsonObject> UWidgetFactoryGenerator::MarginToJson(const FMargin& M)
{
	auto Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("Left"), M.Left);
	Obj->SetNumberField(TEXT("Top"), M.Top);
	Obj->SetNumberField(TEXT("Right"), M.Right);
	Obj->SetNumberField(TEXT("Bottom"), M.Bottom);
	return Obj;
}

FString UWidgetFactoryGenerator::GetWidgetTypeName(UWidget* Widget)
{
	if (!Widget) return TEXT("Unknown");
	EnsureClassMapInitialized();
	for (const auto& Pair : GWidgetClassMap)
	{
		if (Widget->GetClass() == Pair.Value || Widget->GetClass()->IsChildOf(Pair.Value))
			return Pair.Key;
	}
	return Widget->GetClass()->GetName();
}

TSharedPtr<FJsonObject> UWidgetFactoryGenerator::ExportPropertiesToJson(UWidget* Widget)
{
	if (!Widget) return nullptr;
	auto Props = MakeShared<FJsonObject>();
	bool bHasProps = false;

	// Visibility
	if (Widget->GetVisibility() != ESlateVisibility::Visible)
	{
		FString Vis;
		switch (Widget->GetVisibility())
		{
		case ESlateVisibility::Collapsed:           Vis = TEXT("Collapsed"); break;
		case ESlateVisibility::Hidden:              Vis = TEXT("Hidden"); break;
		case ESlateVisibility::HitTestInvisible:    Vis = TEXT("HitTestInvisible"); break;
		case ESlateVisibility::SelfHitTestInvisible:Vis = TEXT("SelfHitTestInvisible"); break;
		default: break;
		}
		if (!Vis.IsEmpty()) { Props->SetStringField(TEXT("Visibility"), Vis); bHasProps = true; }
	}

	if (Widget->GetRenderOpacity() < 1.0f - KINDA_SMALL_NUMBER)
	{
		Props->SetNumberField(TEXT("RenderOpacity"), Widget->GetRenderOpacity());
		bHasProps = true;
	}

	// TextBlock
	if (UTextBlock* TB = Cast<UTextBlock>(Widget))
	{
		FString Text = TB->GetText().ToString();
		if (!Text.IsEmpty()) { Props->SetStringField(TEXT("Text"), Text); bHasProps = true; }

		FSlateFontInfo Font = TB->GetFont();
		if (Font.Size != 24) { Props->SetNumberField(TEXT("FontSize"), Font.Size); bHasProps = true; }

		FLinearColor Color = TB->GetColorAndOpacity().GetSpecifiedColor();
		if (Color != FLinearColor::White) { Props->SetObjectField(TEXT("Color"), ColorToJson(Color)); bHasProps = true; }

		if (TB->GetAutoWrapText()) { Props->SetBoolField(TEXT("AutoWrap"), true); bHasProps = true; }

		// Justification — skip export (getter not available in UE5.7)
	}

	// Image
	if (UImage* Img = Cast<UImage>(Widget))
	{
		FLinearColor Color = Img->GetColorAndOpacity();
		if (Color != FLinearColor::White) { Props->SetObjectField(TEXT("Color"), ColorToJson(Color)); bHasProps = true; }

		if (Img->GetBrush().GetResourceObject())
		{
			Props->SetStringField(TEXT("Brush"), Img->GetBrush().GetResourceObject()->GetPathName());
			bHasProps = true;
		}
	}

	// ProgressBar
	if (UProgressBar* PB = Cast<UProgressBar>(Widget))
	{
		Props->SetNumberField(TEXT("Percent"), PB->GetPercent());
		Props->SetObjectField(TEXT("FillColor"), ColorToJson(PB->GetFillColorAndOpacity()));
		bHasProps = true;
	}

	// Spacer
	if (USpacer* Sp = Cast<USpacer>(Widget))
	{
		FVector2D SpSize = Sp->GetSize();
		if (SpSize.X > 0 || SpSize.Y > 0)
		{
			Props->SetNumberField(TEXT("Size"), SpSize.X);
			bHasProps = true;
		}
	}

	// SizeBox — override values not easily readable at runtime, skip

	return bHasProps ? Props : TSharedPtr<FJsonObject>(nullptr);
}

TSharedPtr<FJsonObject> UWidgetFactoryGenerator::ExportSlotToJson(UWidget* Widget)
{
	if (!Widget || !Widget->Slot) return nullptr;
	auto SlotJson = MakeShared<FJsonObject>();
	bool bHasSlot = false;

	// CanvasPanelSlot
	if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Widget->Slot))
	{
		FAnchors Anch = CS->GetAnchors();
		// Try to match named anchors
		FString AnchName;
		if      (Anch.Minimum == FVector2D(0,0) && Anch.Maximum == FVector2D(1,1)) AnchName = TEXT("Fill");
		else if (Anch.Minimum == FVector2D(0.5,0.5) && Anch.Maximum == FVector2D(0.5,0.5)) AnchName = TEXT("Center");
		else if (Anch.Minimum == FVector2D(0,0) && Anch.Maximum == FVector2D(0,0)) AnchName = TEXT("TopLeft");
		else if (Anch.Minimum == FVector2D(0.5,0) && Anch.Maximum == FVector2D(0.5,0)) AnchName = TEXT("TopCenter");
		else if (Anch.Minimum == FVector2D(1,0) && Anch.Maximum == FVector2D(1,0)) AnchName = TEXT("TopRight");
		else if (Anch.Minimum == FVector2D(0,1) && Anch.Maximum == FVector2D(0,1)) AnchName = TEXT("BottomLeft");
		else if (Anch.Minimum == FVector2D(0.5,1) && Anch.Maximum == FVector2D(0.5,1)) AnchName = TEXT("BottomCenter");
		else if (Anch.Minimum == FVector2D(1,1) && Anch.Maximum == FVector2D(1,1)) AnchName = TEXT("BottomRight");
		else if (Anch.Minimum == FVector2D(0,0.5) && Anch.Maximum == FVector2D(0,0.5)) AnchName = TEXT("CenterLeft");
		else if (Anch.Minimum == FVector2D(1,0.5) && Anch.Maximum == FVector2D(1,0.5)) AnchName = TEXT("CenterRight");
		else if (Anch.Minimum == FVector2D(0,0.5) && Anch.Maximum == FVector2D(1,0.5)) AnchName = TEXT("FillHorizontal");
		else if (Anch.Minimum == FVector2D(0.5,0) && Anch.Maximum == FVector2D(0.5,1)) AnchName = TEXT("FillVertical");

		if (!AnchName.IsEmpty())
		{
			SlotJson->SetStringField(TEXT("Anchors"), AnchName);
		}
		else
		{
			auto AnchObj = MakeShared<FJsonObject>();
			AnchObj->SetNumberField(TEXT("MinX"), Anch.Minimum.X);
			AnchObj->SetNumberField(TEXT("MinY"), Anch.Minimum.Y);
			AnchObj->SetNumberField(TEXT("MaxX"), Anch.Maximum.X);
			AnchObj->SetNumberField(TEXT("MaxY"), Anch.Maximum.Y);
			SlotJson->SetObjectField(TEXT("AnchorsCustom"), AnchObj);
		}

		FVector2D Pos = CS->GetPosition();
		if (!Pos.IsNearlyZero())
		{
			auto PosObj = MakeShared<FJsonObject>();
			PosObj->SetNumberField(TEXT("X"), Pos.X);
			PosObj->SetNumberField(TEXT("Y"), Pos.Y);
			SlotJson->SetObjectField(TEXT("Position"), PosObj);
		}

		FVector2D Size = CS->GetSize();
		if (Size.X > 0 || Size.Y > 0)
		{
			auto SizeObj = MakeShared<FJsonObject>();
			SizeObj->SetNumberField(TEXT("Width"), Size.X);
			SizeObj->SetNumberField(TEXT("Height"), Size.Y);
			SlotJson->SetObjectField(TEXT("Size"), SizeObj);
		}

		FVector2D Align = CS->GetAlignment();
		// Only export non-default alignment (skip if it matches the anchor preset default)
		bool bDefaultAlign = false;
		if (AnchName == TEXT("TopLeft") && Align.IsNearlyZero()) bDefaultAlign = true;
		else if (AnchName == TEXT("Center") && Align.Equals(FVector2D(0.5, 0.5), 0.01)) bDefaultAlign = true;
		else if (AnchName == TEXT("Fill") && Align.IsNearlyZero()) bDefaultAlign = true;
		else if (Align.IsNearlyZero()) bDefaultAlign = true;

		if (!bDefaultAlign)
		{
			auto AlignObj = MakeShared<FJsonObject>();
			AlignObj->SetNumberField(TEXT("X"), Align.X);
			AlignObj->SetNumberField(TEXT("Y"), Align.Y);
			SlotJson->SetObjectField(TEXT("Alignment"), AlignObj);
		}

		if (AnchName == TEXT("Fill"))
		{
			// For Fill anchors, offsets represent margins
			FMargin Offsets = CS->GetOffsets();
			if (FMath::Abs(Offsets.Left) > 0.1f || FMath::Abs(Offsets.Top) > 0.1f ||
				FMath::Abs(Offsets.Right) > 0.1f || FMath::Abs(Offsets.Bottom) > 0.1f)
				SlotJson->SetObjectField(TEXT("Offsets"), MarginToJson(Offsets));
		}
		// Also export offsets for custom anchors with stretch
		if (!AnchName.IsEmpty() && AnchName != TEXT("Fill"))
		{
			// non-Fill named anchors: offsets not needed
		}
		else if (AnchName.IsEmpty())
		{
			// Custom anchors: export offsets if stretch involved
			FAnchors A = CS->GetAnchors();
			if (!FMath::IsNearlyEqual(A.Minimum.X, A.Maximum.X) || !FMath::IsNearlyEqual(A.Minimum.Y, A.Maximum.Y))
			{
				FMargin Offsets = CS->GetOffsets();
				if (FMath::Abs(Offsets.Left) > 0.1f || FMath::Abs(Offsets.Top) > 0.1f ||
					FMath::Abs(Offsets.Right) > 0.1f || FMath::Abs(Offsets.Bottom) > 0.1f)
					SlotJson->SetObjectField(TEXT("Offsets"), MarginToJson(Offsets));
			}
		}

		if (!CS->GetAutoSize())
			SlotJson->SetBoolField(TEXT("AutoSize"), false);

		if (CS->GetZOrder() != 0)
			SlotJson->SetNumberField(TEXT("ZOrder"), CS->GetZOrder());

		bHasSlot = true;
	}

	// HorizontalBoxSlot
	if (UHorizontalBoxSlot* HS = Cast<UHorizontalBoxSlot>(Widget->Slot))
	{
		FSlateChildSize Size = HS->GetSize();
		if (Size.SizeRule == ESlateSizeRule::Fill)
		{
			SlotJson->SetStringField(TEXT("SizeRule"), TEXT("Fill"));
			if (!FMath::IsNearlyEqual(Size.Value, 1.0f))
				SlotJson->SetNumberField(TEXT("FillWidth"), Size.Value);
		}
		else
		{
			SlotJson->SetStringField(TEXT("SizeRule"), TEXT("Auto"));
		}
		if (HS->GetHorizontalAlignment() != HAlign_Fill)
		{
			FString HA = HS->GetHorizontalAlignment() == HAlign_Center ? TEXT("Center") :
			             HS->GetHorizontalAlignment() == HAlign_Right ? TEXT("Right") : TEXT("Left");
			SlotJson->SetStringField(TEXT("HAlign"), HA);
		}
		if (HS->GetVerticalAlignment() != VAlign_Fill)
		{
			FString VA = HS->GetVerticalAlignment() == VAlign_Center ? TEXT("Center") :
			             HS->GetVerticalAlignment() == VAlign_Bottom ? TEXT("Bottom") : TEXT("Top");
			SlotJson->SetStringField(TEXT("VAlign"), VA);
		}
		FMargin Pad = HS->GetPadding();
		if (Pad.Left != 0 || Pad.Top != 0 || Pad.Right != 0 || Pad.Bottom != 0)
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		bHasSlot = true;
	}

	// VerticalBoxSlot
	if (UVerticalBoxSlot* VS = Cast<UVerticalBoxSlot>(Widget->Slot))
	{
		FSlateChildSize Size = VS->GetSize();
		if (Size.SizeRule == ESlateSizeRule::Fill)
		{
			SlotJson->SetStringField(TEXT("SizeRule"), TEXT("Fill"));
			if (!FMath::IsNearlyEqual(Size.Value, 1.0f))
				SlotJson->SetNumberField(TEXT("FillHeight"), Size.Value);
		}
		else
		{
			SlotJson->SetStringField(TEXT("SizeRule"), TEXT("Auto"));
		}
		if (VS->GetHorizontalAlignment() != HAlign_Fill)
		{
			FString HA = VS->GetHorizontalAlignment() == HAlign_Center ? TEXT("Center") :
			             VS->GetHorizontalAlignment() == HAlign_Right ? TEXT("Right") : TEXT("Left");
			SlotJson->SetStringField(TEXT("HAlign"), HA);
		}
		if (VS->GetVerticalAlignment() != VAlign_Fill)
		{
			FString VA = VS->GetVerticalAlignment() == VAlign_Center ? TEXT("Center") :
			             VS->GetVerticalAlignment() == VAlign_Bottom ? TEXT("Bottom") : TEXT("Top");
			SlotJson->SetStringField(TEXT("VAlign"), VA);
		}
		FMargin Pad = VS->GetPadding();
		if (Pad.Left != 0 || Pad.Top != 0 || Pad.Right != 0 || Pad.Bottom != 0)
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		bHasSlot = true;
	}

	return bHasSlot ? SlotJson : TSharedPtr<FJsonObject>(nullptr);
}

TSharedPtr<FJsonObject> UWidgetFactoryGenerator::ExportWidgetToJson(UWidget* Widget)
{
	if (!Widget) return nullptr;

	auto Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("Type"), GetWidgetTypeName(Widget));
	Json->SetStringField(TEXT("Name"), Widget->GetName());

	if (Widget->bIsVariable)
	{
		// Only export IsVariable for meaningfully named widgets
		// Skip auto-generated names like Image_0, TextBlock_5, CanvasPanel_1 etc.
		FString WidgetName = Widget->GetName();
		static const FRegexPattern AutoNamePattern(TEXT("^[A-Za-z]+_\\d+$"));
		FRegexMatcher Matcher(AutoNamePattern, WidgetName);
		if (!Matcher.FindNext())
		{
			Json->SetBoolField(TEXT("IsVariable"), true);
		}
	}

	// Slot
	auto SlotJson = ExportSlotToJson(Widget);
	if (SlotJson.IsValid())
		Json->SetObjectField(TEXT("Slot"), SlotJson);

	// Properties
	auto PropsJson = ExportPropertiesToJson(Widget);
	if (PropsJson.IsValid())
		Json->SetObjectField(TEXT("Properties"), PropsJson);

	// Children
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		if (Panel->GetChildrenCount() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildArray;
			for (int32 i = 0; i < Panel->GetChildrenCount(); i++)
			{
				auto ChildJson = ExportWidgetToJson(Panel->GetChildAt(i));
				if (ChildJson.IsValid())
					ChildArray.Add(MakeShared<FJsonValueObject>(ChildJson));
			}
			if (ChildArray.Num() > 0)
				Json->SetArrayField(TEXT("Children"), ChildArray);
		}
	}

	return Json;
}

FString UWidgetFactoryGenerator::GetUnLuaModuleName(UWidgetBlueprint* WidgetBP)
{
#if WITH_UNLUA
	if (!WidgetBP) return TEXT("");

	TArray<UEdGraph*> AllGraphs;
	AllGraphs.Append(WidgetBP->FunctionGraphs);
	for (FBPInterfaceDescription& Iface : WidgetBP->ImplementedInterfaces)
		AllGraphs.Append(Iface.Graphs);

	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetFName().ToString().Contains(TEXT("GetModuleName")))
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_FunctionResult* ResultNode = Cast<UK2Node_FunctionResult>(Node);
				if (!ResultNode) continue;
				for (UEdGraphPin* Pin : ResultNode->Pins)
				{
					if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String)
						return Pin->DefaultValue;
				}
			}
		}
	}
#endif
	return TEXT("");
}

bool UWidgetFactoryGenerator::ExportToJson(const FString& WidgetPath, const FString& OutputFileName)
{
	UE_LOG(LogWidgetFactory, Log, TEXT("════════════════════════════════════════"));
	UE_LOG(LogWidgetFactory, Log, TEXT("开始导出: %s"), *WidgetPath);
	UE_LOG(LogWidgetFactory, Log, TEXT("════════════════════════════════════════"));

	UWidgetBlueprint* WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *WidgetPath);
	if (!WidgetBP)
	{
		// Try appending _C or common suffixes
		FString TryPath = WidgetPath + TEXT(".") + FPaths::GetBaseFilename(WidgetPath);
		WidgetBP = LoadObject<UWidgetBlueprint>(nullptr, *TryPath);
	}
	if (!WidgetBP)
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("无法加载 Widget Blueprint: %s"), *WidgetPath);
		return false;
	}

	UWidgetTree* Tree = WidgetBP->WidgetTree;
	if (!Tree || !Tree->RootWidget)
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("Widget Blueprint 没有控件树"));
		return false;
	}

	// Build root JSON
	auto Config = MakeShared<FJsonObject>();
	Config->SetStringField(TEXT("WidgetName"), WidgetBP->GetName());
	Config->SetStringField(TEXT("Description"), FString::Printf(TEXT("从 %s 导出"), *WidgetBP->GetName()));

	// Export widget tree
	auto RootJson = ExportWidgetToJson(Tree->RootWidget);
	if (RootJson.IsValid())
		Config->SetObjectField(TEXT("RootWidget"), RootJson);

	// UnLua binding
	FString ModuleName = GetUnLuaModuleName(WidgetBP);
	if (!ModuleName.IsEmpty())
	{
		auto UnLuaJson = MakeShared<FJsonObject>();
		UnLuaJson->SetBoolField(TEXT("Enabled"), true);
		UnLuaJson->SetStringField(TEXT("ModuleName"), ModuleName);
		UnLuaJson->SetBoolField(TEXT("AddEventTick"), true);
		Config->SetObjectField(TEXT("UnLuaBinding"), UnLuaJson);
	}

	// Serialize to string
	FString OutputStr;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutputStr);
	Writer->WriteObjectStart();
	for (const auto& Pair : Config->Values)
	{
		FJsonSerializer::Serialize(Pair.Value, Pair.Key, *Writer, true);
	}
	Writer->WriteObjectEnd();
	Writer->Close();

	// Write file
	FString FileName = OutputFileName.IsEmpty() ? WidgetBP->GetName() : OutputFileName;
	FString OutputPath = GetTemplateDirectory() / (FileName + TEXT(".json"));
	if (FFileHelper::SaveStringToFile(OutputStr, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogWidgetFactory, Log, TEXT("导出成功: %s"), *OutputPath);
		return true;
	}

	UE_LOG(LogWidgetFactory, Error, TEXT("写入文件失败: %s"), *OutputPath);
	return false;
}


// ─── Public API ─────────────────────────────────────────────────────────────

UWidgetBlueprint* UWidgetFactoryGenerator::GenerateFromJson(const FString& JsonFileName, const FString& PackagePath)
{
	UE_LOG(LogWidgetFactory, Log, TEXT("════════════════════════════════════════"));
	UE_LOG(LogWidgetFactory, Log, TEXT("开始生成: %s"), *JsonFileName);
	UE_LOG(LogWidgetFactory, Log, TEXT("════════════════════════════════════════"));

	FString JsonPath = GetTemplateDirectory() / (JsonFileName + TEXT(".json"));
	TSharedPtr<FJsonObject> Config = LoadJsonConfig(JsonPath);
	if (!Config.IsValid()) return nullptr;

	FString WidgetName;
	if (!Config->TryGetStringField(TEXT("WidgetName"), WidgetName))
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("模板缺少 'WidgetName' 字段"));
		return nullptr;
	}

	UWidgetBlueprint* BP = CreateWidgetBlueprint(PackagePath, WidgetName);
	if (!BP) return nullptr;

	UWidgetTree* Tree = BP->WidgetTree;
	if (!Tree) { UE_LOG(LogWidgetFactory, Error, TEXT("获取 WidgetTree 失败")); return nullptr; }

	// Build widget tree
	const TSharedPtr<FJsonObject>* RootJson;
	if (Config->TryGetObjectField(TEXT("RootWidget"), RootJson))
		CreateWidgetFromJson(Tree, *RootJson, nullptr);

	// UnLua binding (optional)
	const TSharedPtr<FJsonObject>* UnLuaConfig;
	if (Config->TryGetObjectField(TEXT("UnLuaBinding"), UnLuaConfig))
		SetupUnLuaBinding(BP, *UnLuaConfig);

	// Compile & save
	FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);

	FString FileName = FPackageName::LongPackageNameToFilename(PackagePath / WidgetName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	UPackage::SavePackage(BP->GetPackage(), BP, *FileName, SaveArgs);

	if (UPackage* Pkg = BP->GetPackage()) Pkg->SetDirtyFlag(false);

	UE_LOG(LogWidgetFactory, Log, TEXT("生成成功: %s → %s/%s"), *JsonFileName, *PackagePath, *WidgetName);
	return BP;
}

void UWidgetFactoryGenerator::GenerateAllWidgets(const FString& PackagePath)
{
	FString ConfigDir = GetTemplateDirectory();
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(ConfigDir / TEXT("*.json")), true, false);

	if (Files.Num() == 0)
	{
		UE_LOG(LogWidgetFactory, Warning, TEXT("模板目录为空: %s"), *ConfigDir);
		return;
	}

	int32 Ok = 0;
	for (const FString& F : Files)
	{
		if (GenerateFromJson(FPaths::GetBaseFilename(F), PackagePath)) Ok++;
	}
	UE_LOG(LogWidgetFactory, Log, TEXT("生成完成: %d/%d"), Ok, Files.Num());
}
