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

		UPackage* OldPkg = FindPackage(nullptr, *FullPath);
		if (OldPkg)
		{
			OldPkg->ClearFlags(RF_Standalone);
			OldPkg->SetFlags(RF_Transient);
			TArray<UObject*> ToDelete;
			ForEachObjectWithPackage(OldPkg, [&ToDelete](UObject* Obj) { ToDelete.Add(Obj); return true; }, false);
			for (UObject* Obj : ToDelete)
			{
				if (Obj) { Obj->ClearFlags(RF_Standalone | RF_Public); Obj->SetFlags(RF_Transient); Obj->MarkAsGarbage(); }
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
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
	Widget->Rename(*Name);
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

		FTopLevelAssetPath InterfacePath(UnLuaInterfaceClass->GetPackage()->GetFName(), UnLuaInterfaceClass->GetFName());
		FBlueprintEditorUtils::ImplementNewInterface(WidgetBP, InterfacePath);
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
