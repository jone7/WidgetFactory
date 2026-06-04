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
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Spacer.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/GridPanel.h"
#include "Components/UniformGridPanel.h"
#include "Components/WrapBox.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/RichTextBlock.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Throbber.h"
#include "Components/CircularThrobber.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionResult.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Regex.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UObject/LinkerLoad.h"
#include "UObject/UnrealType.h"
#include "Editor.h"

#if WITH_UNLUA
#include "UnLuaInterface.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWidgetFactory, Log, All);

// ─── Static member ──────────────────────────────────────────────────────────

FString UWidgetFactoryGenerator::CustomTemplateDir;

namespace
{
FString ResolveWidgetTemplatePath(const FString& JsonFileNameOrPath)
{
	FString Candidate = JsonFileNameOrPath;
	Candidate.ReplaceInline(TEXT("\\"), TEXT("/"));

	if (Candidate.IsEmpty())
	{
		return FString();
	}

	auto TryResolve = [](const FString& Path) -> FString
	{
		if (Path.IsEmpty())
		{
			return FString();
		}
		if (FPaths::FileExists(Path))
		{
			return Path;
		}
		if (FPaths::IsRelative(Path))
		{
			const FString ProjectRelative = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path);
			if (FPaths::FileExists(ProjectRelative))
			{
				return ProjectRelative;
			}
		}
		return FString();
	};

	FString Direct = TryResolve(Candidate);
	if (!Direct.IsEmpty())
	{
		return Direct;
	}

	FString WithExt = Candidate.EndsWith(TEXT(".json")) ? Candidate : Candidate + TEXT(".json");
	Direct = TryResolve(WithExt);
	if (!Direct.IsEmpty())
	{
		return Direct;
	}

	const FString TemplateDir = UWidgetFactoryGenerator::GetTemplateDirectory();
	const FString FileName = FPaths::GetCleanFilename(WithExt);

	FString TemplateFile = TryResolve(TemplateDir / FileName);
	if (!TemplateFile.IsEmpty())
	{
		return TemplateFile;
	}

	TemplateFile = TryResolve(TemplateDir / WithExt);
	if (!TemplateFile.IsEmpty())
	{
		return TemplateFile;
	}

	return TemplateDir / FileName;
}

bool ShouldTreatExportTargetAsPath(const FString& OutputFileName)
{
	if (OutputFileName.IsEmpty())
	{
		return false;
	}

	return OutputFileName.Contains(TEXT("/"))
		|| OutputFileName.Contains(TEXT("\\"))
		|| OutputFileName.Contains(TEXT(":"));
}

FString ResolveExportJsonPath(const FString& OutputFileNameOrPath, const FString& DefaultFileName)
{
	if (!ShouldTreatExportTargetAsPath(OutputFileNameOrPath))
	{
		FString FileName = OutputFileNameOrPath.IsEmpty() ? DefaultFileName : OutputFileNameOrPath;
		if (FileName.EndsWith(TEXT(".json")))
		{
			FileName.LeftChopInline(5, EAllowShrinking::No);
		}
		return UWidgetFactoryGenerator::GetTemplateDirectory() / (FileName + TEXT(".json"));
	}

	FString OutputPath = OutputFileNameOrPath;
	OutputPath.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (!OutputPath.EndsWith(TEXT(".json")))
	{
		OutputPath += TEXT(".json");
	}
	if (FPaths::IsRelative(OutputPath))
	{
		OutputPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / OutputPath);
	}
	return OutputPath;
}

TSharedPtr<FJsonObject> TryLoadExistingJsonObject(const FString& JsonPath)
{
	if (!FPaths::FileExists(JsonPath))
	{
		return nullptr;
	}

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return nullptr;
	}

	return JsonObject;
}

bool IsIngredientPlaceholderValue(const TSharedPtr<FJsonValue>& JsonValue)
{
	if (!JsonValue.IsValid() || JsonValue->Type != EJson::String)
	{
		return false;
	}

	FString StringValue;
	return JsonValue->TryGetString(StringValue) && StringValue.StartsWith(TEXT("@ingredient:"));
}

TSharedPtr<FJsonValue> PreserveIngredientPlaceholders(
	const TSharedPtr<FJsonValue>& ExportedValue,
	const TSharedPtr<FJsonValue>& ExistingValue);

TSharedPtr<FJsonObject> PreserveIngredientPlaceholders(
	const TSharedPtr<FJsonObject>& ExportedObject,
	const TSharedPtr<FJsonObject>& ExistingObject)
{
	if (!ExportedObject.IsValid() || !ExistingObject.IsValid())
	{
		return ExportedObject;
	}

	for (TPair<FString, TSharedPtr<FJsonValue>>& Pair : ExportedObject->Values)
	{
		const TSharedPtr<FJsonValue>* ExistingValue = ExistingObject->Values.Find(Pair.Key);
		if (!ExistingValue || !ExistingValue->IsValid())
		{
			continue;
		}

		Pair.Value = PreserveIngredientPlaceholders(Pair.Value, *ExistingValue);
	}

	return ExportedObject;
}

TSharedPtr<FJsonValue> PreserveIngredientPlaceholders(
	const TSharedPtr<FJsonValue>& ExportedValue,
	const TSharedPtr<FJsonValue>& ExistingValue)
{
	if (!ExportedValue.IsValid() || !ExistingValue.IsValid())
	{
		return ExportedValue;
	}

	if (IsIngredientPlaceholderValue(ExistingValue))
	{
		return ExistingValue;
	}

	if (ExportedValue->Type == EJson::Object && ExistingValue->Type == EJson::Object)
	{
		TSharedPtr<FJsonObject> ExportedObject = ExportedValue->AsObject();
		const TSharedPtr<FJsonObject> ExistingObject = ExistingValue->AsObject();
		return MakeShared<FJsonValueObject>(PreserveIngredientPlaceholders(ExportedObject, ExistingObject));
	}

	if (ExportedValue->Type == EJson::Array && ExistingValue->Type == EJson::Array)
	{
		TArray<TSharedPtr<FJsonValue>> ExportedArray = ExportedValue->AsArray();
		const TArray<TSharedPtr<FJsonValue>>& ExistingArray = ExistingValue->AsArray();
		const int32 SharedCount = FMath::Min(ExportedArray.Num(), ExistingArray.Num());
		for (int32 Index = 0; Index < SharedCount; ++Index)
		{
			ExportedArray[Index] = PreserveIngredientPlaceholders(ExportedArray[Index], ExistingArray[Index]);
		}
		return MakeShared<FJsonValueArray>(ExportedArray);
	}

	return ExportedValue;
}

FString NormalizeObjectLoadPath(const FString& AssetPath)
{
	FString LoadPath = AssetPath;
	LoadPath.TrimStartAndEndInline();

	if (LoadPath.StartsWith(TEXT("/Game/")) && !LoadPath.Contains(TEXT(".")))
	{
		const FString AssetName = FPaths::GetBaseFilename(LoadPath);
		if (!AssetName.IsEmpty())
		{
			LoadPath = FString::Printf(TEXT("%s.%s"), *LoadPath, *AssetName);
		}
	}

	return LoadPath;
}

template <typename TObject>
TObject* LoadAssetObject(const FString& AssetPath)
{
	const FString LoadPath = NormalizeObjectLoadPath(AssetPath);
	return LoadPath.IsEmpty() ? nullptr : LoadObject<TObject>(nullptr, *LoadPath);
}

bool EndPlaySessionIfActiveForOverwrite(const FString& FullPath)
{
	if (!GEditor || GEditor->PlayWorld == nullptr)
	{
		return true;
	}

	UE_LOG(LogWidgetFactory, Warning,
		TEXT("PIE is active while overwriting an existing widget asset. Stopping PIE first: %s"),
		*FullPath);

	GEditor->RequestEndPlayMap();

	const double DeadlineSeconds = FPlatformTime::Seconds() + 5.0;
	while (GEditor->PlayWorld != nullptr && FPlatformTime::Seconds() < DeadlineSeconds)
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().Tick(ESlateTickType::Time);
		}
		FPlatformProcess::Sleep(0.05f);
	}

	if (GEditor->PlayWorld != nullptr)
	{
		GEditor->EndPlayMap();
	}

	FlushAsyncLoading();
	if (GEditor->PlayWorld != nullptr)
	{
		UE_LOG(LogWidgetFactory, Error,
			TEXT("PIE is still active; aborting widget overwrite to avoid editor crash: %s"),
			*FullPath);
		return false;
	}

	return true;
}
}

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
	GWidgetClassMap.Add(TEXT("SpinBox"),             USpinBox::StaticClass());
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

bool UWidgetFactoryGenerator::PrepareExistingAssetForOverwrite(const FString& FullPath, const FString& WidgetName, bool* bOutHadOpenEditor)
{
	if (bOutHadOpenEditor)
	{
		*bOutHadOpenEditor = false;
	}
	if (!GEditor)
	{
		return true;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return true;
	}

	TArray<UObject*> AssetsToCheck;
	if (UPackage* ExistingPkg = FindPackage(nullptr, *FullPath))
	{
		ForEachObjectWithPackage(ExistingPkg, [&AssetsToCheck](UObject* Obj)
		{
			if (Obj && Obj->IsAsset())
			{
				AssetsToCheck.Add(Obj);
			}
			return true;
		}, false);
	}

	if (AssetsToCheck.Num() == 0)
	{
		const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *FullPath, *WidgetName);
		if (UObject* ExistingAsset = LoadObject<UObject>(nullptr, *ObjectPath))
		{
			AssetsToCheck.Add(ExistingAsset);
		}
	}

	if (AssetsToCheck.Num() == 0)
	{
		return true;
	}

	bool bHasOpenEditor = false;
	for (UObject* Asset : AssetsToCheck)
	{
		if (Asset && AssetEditorSubsystem->FindEditorsForAsset(Asset).Num() > 0)
		{
			bHasOpenEditor = true;
			break;
		}
	}

	if (bOutHadOpenEditor)
	{
		*bOutHadOpenEditor = bHasOpenEditor;
	}

	if (bHasOpenEditor)
	{
		UE_LOG(LogWidgetFactory, Warning,
			TEXT("目标 UI 当前正处于打开状态，自动关闭后继续覆盖生成: %s"),
			*FullPath);
	}
	else
	{
		UE_LOG(LogWidgetFactory, Log,
			TEXT("目标 UI 已存在，执行覆盖前清理: %s"),
			*FullPath);
	}

	for (UObject* Asset : AssetsToCheck)
	{
		if (Asset)
		{
			AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
		}
	}

	FlushAsyncLoading();

	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().Tick(ESlateTickType::Time);
	}

	// 已打开资产在关闭资源页后，继续沿用后面的旧覆盖路径即可。
	// 这里如果抢先 GC，UE 的 TypedElement/编辑器状态偶发会在随后访问旧引用而崩溃。
	// 未打开资产则仍然需要做一次更重的清理，避免 REINST/重命名冲突。
	if (!bHasOpenEditor)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		FlushAsyncLoading();

		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().Tick(ESlateTickType::Time);
		}
	}

	for (UObject* Asset : AssetsToCheck)
	{
		if (Asset && AssetEditorSubsystem->FindEditorsForAsset(Asset).Num() > 0)
		{
			UE_LOG(LogWidgetFactory, Error,
				TEXT("自动关闭目标 UI 编辑器失败，已中止覆盖生成。请手动关闭资源页后重试: %s"),
				*Asset->GetPathName());
			return false;
		}
	}

	return true;
}

bool UWidgetFactoryGenerator::ResetWidgetBlueprintForReuse(UWidgetBlueprint* WidgetBlueprint)
{
	if (!WidgetBlueprint)
	{
		return false;
	}

	WidgetBlueprint->Modify();

	if (WidgetBlueprint->WidgetTree)
	{
		TArray<UWidget*> OldWidgets;
		WidgetBlueprint->WidgetTree->GetAllWidgets(OldWidgets);
		for (UWidget* OldWidget : OldWidgets)
		{
			if (!OldWidget)
			{
				continue;
			}

			OldWidget->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
			OldWidget->ClearFlags(RF_Standalone | RF_Public);
			OldWidget->SetFlags(RF_Transient);
			OldWidget->MarkAsGarbage();
		}

		UWidgetTree* OldTree = WidgetBlueprint->WidgetTree;
		WidgetBlueprint->WidgetTree = nullptr;
		OldTree->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_ForceNoResetLoaders);
		OldTree->ClearFlags(RF_Standalone | RF_Public);
		OldTree->SetFlags(RF_Transient);
		OldTree->MarkAsGarbage();
	}

#if WITH_EDITORONLY_DATA
	WidgetBlueprint->Bindings.Empty();
	WidgetBlueprint->Animations.Empty();
	WidgetBlueprint->WidgetVariableNameToGuidMap.Empty();
#endif

	WidgetBlueprint->WidgetTree = NewObject<UWidgetTree>(WidgetBlueprint, TEXT("WidgetTree"), RF_Transactional);
	return WidgetBlueprint->WidgetTree != nullptr;
}


// ─── Blueprint creation ─────────────────────────────────────────────────────

UWidgetBlueprint* UWidgetFactoryGenerator::CreateWidgetBlueprint(const FString& PackagePath, const FString& WidgetName, UClass* ParentClass)
{
	FString FullPath = PackagePath / WidgetName;
	FString AssetFile = FPackageName::LongPackageNameToFilename(FullPath, FPackageName::GetAssetPackageExtension());

	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetFile))
	{
		UE_LOG(LogWidgetFactory, Warning, TEXT("覆盖已有资源: %s"), *FullPath);

		if (!EndPlaySessionIfActiveForOverwrite(FullPath))
		{
			return nullptr;
		}

		if (!PrepareExistingAssetForOverwrite(FullPath, WidgetName))
		{
			return nullptr;
		}

		UEditorAssetSubsystem* EditorAssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
		if (!EditorAssetSubsystem)
		{
			UE_LOG(LogWidgetFactory, Error, TEXT("无法获取 EditorAssetSubsystem，无法安全删除旧资源: %s"), *FullPath);
			return nullptr;
		}

		if (EditorAssetSubsystem->DoesAssetExist(FullPath))
		{
			UE_LOG(LogWidgetFactory, Log, TEXT("删除旧资源后重建: %s"), *FullPath);
			if (!EditorAssetSubsystem->DeleteAsset(FullPath))
			{
				UE_LOG(LogWidgetFactory, Error, TEXT("删除旧资源失败，已中止生成: %s"), *FullPath);
				return nullptr;
			}

			FlushAsyncLoading();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().Tick(ESlateTickType::Time);
			}

			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			FlushAsyncLoading();
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().Tick(ESlateTickType::Time);
			}

			if (EditorAssetSubsystem->DoesAssetExist(FullPath))
			{
				UE_LOG(LogWidgetFactory, Error, TEXT("旧资源删除后仍然存在，已中止生成: %s"), *FullPath);
				return nullptr;
			}
		}

		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*AssetFile))
		{
			IFileManager::Get().Delete(*AssetFile, false, true);
		}
	}

	UPackage* Package = CreatePackage(*FullPath);
	if (!Package) { UE_LOG(LogWidgetFactory, Error, TEXT("创建 Package 失败: %s"), *FullPath); return nullptr; }

	if (!ParentClass || !ParentClass->IsChildOf(UUserWidget::StaticClass()))
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("无效 Widget 父类: %s"), ParentClass ? *ParentClass->GetPathName() : TEXT("<null>"));
		return nullptr;
	}

	UWidgetBlueprint* BP = CastChecked<UWidgetBlueprint>(
		FKismetEditorUtilities::CreateBlueprint(
			ParentClass, Package, FName(*WidgetName),
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

static TSharedPtr<FJsonObject> MakeColorJson(const FLinearColor& Color)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("R"), FMath::RoundToFloat(Color.R * 1000) / 1000);
	Json->SetNumberField(TEXT("G"), FMath::RoundToFloat(Color.G * 1000) / 1000);
	Json->SetNumberField(TEXT("B"), FMath::RoundToFloat(Color.B * 1000) / 1000);
	Json->SetNumberField(TEXT("A"), FMath::RoundToFloat(Color.A * 1000) / 1000);
	return Json;
}

static TSharedPtr<FJsonObject> MakeMarginJson(const FMargin& Margin)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetNumberField(TEXT("Left"), Margin.Left);
	Json->SetNumberField(TEXT("Top"), Margin.Top);
	Json->SetNumberField(TEXT("Right"), Margin.Right);
	Json->SetNumberField(TEXT("Bottom"), Margin.Bottom);
	return Json;
}

static bool IsMarginNearlyZero(const FMargin& Margin, float Tolerance = KINDA_SMALL_NUMBER)
{
	return FMath::IsNearlyZero(Margin.Left, Tolerance)
		&& FMath::IsNearlyZero(Margin.Top, Tolerance)
		&& FMath::IsNearlyZero(Margin.Right, Tolerance)
		&& FMath::IsNearlyZero(Margin.Bottom, Tolerance);
}

static FVector2D ParseSlateVector2D(const TSharedPtr<FJsonObject>& Obj)
{
	double Width = 0.0;
	double Height = 0.0;

	if (Obj->HasField(TEXT("Width"))) Width = Obj->GetNumberField(TEXT("Width"));
	else if (Obj->HasField(TEXT("X"))) Width = Obj->GetNumberField(TEXT("X"));

	if (Obj->HasField(TEXT("Height"))) Height = Obj->GetNumberField(TEXT("Height"));
	else if (Obj->HasField(TEXT("Y"))) Height = Obj->GetNumberField(TEXT("Y"));

	return FVector2D(Width, Height);
}

static FString BrushDrawTypeToString(ESlateBrushDrawType::Type DrawAs)
{
	switch (DrawAs)
	{
	case ESlateBrushDrawType::NoDrawType: return TEXT("NoDrawType");
	case ESlateBrushDrawType::Box: return TEXT("Box");
	case ESlateBrushDrawType::Border: return TEXT("Border");
	case ESlateBrushDrawType::RoundedBox: return TEXT("RoundedBox");
	case ESlateBrushDrawType::Image:
	default:
		return TEXT("Image");
	}
}

static ESlateBrushDrawType::Type ParseBrushDrawType(const FString& DrawAs)
{
	if (DrawAs == TEXT("NoDrawType")) return ESlateBrushDrawType::NoDrawType;
	if (DrawAs == TEXT("Box")) return ESlateBrushDrawType::Box;
	if (DrawAs == TEXT("Border")) return ESlateBrushDrawType::Border;
	if (DrawAs == TEXT("RoundedBox")) return ESlateBrushDrawType::RoundedBox;
	return ESlateBrushDrawType::Image;
}

static EStretch::Type ParseStretch(const FString& Stretch)
{
	if (Stretch == TEXT("None")) return EStretch::None;
	if (Stretch == TEXT("Fill")) return EStretch::Fill;
	if (Stretch == TEXT("ScaleToFit")) return EStretch::ScaleToFit;
	if (Stretch == TEXT("ScaleToFitX")) return EStretch::ScaleToFitX;
	if (Stretch == TEXT("ScaleToFitY")) return EStretch::ScaleToFitY;
	if (Stretch == TEXT("ScaleToFill")) return EStretch::ScaleToFill;
	if (Stretch == TEXT("ScaleBySafeZone")) return EStretch::ScaleBySafeZone;
	if (Stretch == TEXT("UserSpecified")) return EStretch::UserSpecified;
	return EStretch::ScaleToFit;
}

static FString StretchToString(EStretch::Type Stretch)
{
	switch (Stretch)
	{
	case EStretch::None: return TEXT("None");
	case EStretch::Fill: return TEXT("Fill");
	case EStretch::ScaleToFit: return TEXT("ScaleToFit");
	case EStretch::ScaleToFitX: return TEXT("ScaleToFitX");
	case EStretch::ScaleToFitY: return TEXT("ScaleToFitY");
	case EStretch::ScaleToFill: return TEXT("ScaleToFill");
	case EStretch::ScaleBySafeZone: return TEXT("ScaleBySafeZone");
	case EStretch::UserSpecified: return TEXT("UserSpecified");
	default: return TEXT("ScaleToFit");
	}
}

static EStretchDirection::Type ParseStretchDirection(const FString& StretchDirection)
{
	if (StretchDirection == TEXT("UpOnly")) return EStretchDirection::UpOnly;
	if (StretchDirection == TEXT("DownOnly")) return EStretchDirection::DownOnly;
	return EStretchDirection::Both;
}

static FString StretchDirectionToString(EStretchDirection::Type StretchDirection)
{
	switch (StretchDirection)
	{
	case EStretchDirection::UpOnly: return TEXT("UpOnly");
	case EStretchDirection::DownOnly: return TEXT("DownOnly");
	case EStretchDirection::Both:
	default:
		return TEXT("Both");
	}
}

static FString TextJustificationToString(ETextJustify::Type Justification)
{
	switch (Justification)
	{
	case ETextJustify::Center: return TEXT("Center");
	case ETextJustify::Right: return TEXT("Right");
	case ETextJustify::Left:
	default:
		return TEXT("Left");
	}
}

static bool TryGetTextJustification(UWidget* Widget, ETextJustify::Type& OutJustification)
{
	if (!Widget)
	{
		return false;
	}

	if (const FProperty* JustificationProperty = Widget->GetClass()->FindPropertyByName(TEXT("Justification")))
	{
		if (const FByteProperty* ByteProperty = CastField<FByteProperty>(JustificationProperty))
		{
			const uint8 Value = ByteProperty->GetPropertyValue_InContainer(Widget);
			OutJustification = static_cast<ETextJustify::Type>(Value);
			return true;
		}
	}

	return false;
}

static bool ApplySlateBrushFromJson(FSlateBrush& Brush, const TSharedPtr<FJsonObject>& BrushJson)
{
	if (!BrushJson.IsValid())
	{
		return false;
	}

	bool bApplied = false;
	UTexture2D* Texture = nullptr;

	FString TexturePath;
	if (BrushJson->TryGetStringField(TEXT("Texture"), TexturePath) || BrushJson->TryGetStringField(TEXT("Brush"), TexturePath))
	{
		Texture = LoadAssetObject<UTexture2D>(TexturePath);
		if (Texture)
		{
			Brush.SetResourceObject(Texture);
			Brush.SetImageSize(FVector2D(Texture->GetSizeX(), Texture->GetSizeY()));
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.TintColor = FSlateColor(FLinearColor::White);
			bApplied = true;
		}
	}

	const TSharedPtr<FJsonObject>* TintColorObj = nullptr;
	if (BrushJson->TryGetObjectField(TEXT("TintColor"), TintColorObj) || BrushJson->TryGetObjectField(TEXT("Color"), TintColorObj))
	{
		Brush.TintColor = FSlateColor(ParseColor(*TintColorObj));
		bApplied = true;
	}

	const TSharedPtr<FJsonObject>* MarginObj = nullptr;
	if (BrushJson->TryGetObjectField(TEXT("Margin"), MarginObj))
	{
		Brush.Margin = ParseMargin(*MarginObj);
		bApplied = true;
	}

	const TSharedPtr<FJsonObject>* ImageSizeObj = nullptr;
	if (BrushJson->TryGetObjectField(TEXT("ImageSize"), ImageSizeObj))
	{
		Brush.SetImageSize(ParseSlateVector2D(*ImageSizeObj));
		bApplied = true;
	}

	FString DrawAs;
	if (BrushJson->TryGetStringField(TEXT("DrawAs"), DrawAs))
	{
		Brush.DrawAs = ParseBrushDrawType(DrawAs);
		bApplied = true;
	}

	return bApplied;
}

static bool ApplyButtonStyleFromJson(UButton* Button, const TSharedPtr<FJsonObject>& StyleJson)
{
	if (!Button || !StyleJson.IsValid())
	{
		return false;
	}

	FButtonStyle Style = Button->GetStyle();
	bool bApplied = false;

	const TSharedPtr<FJsonObject>* BrushJson = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("Normal"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.Normal, *BrushJson);
	}
	if (StyleJson->TryGetObjectField(TEXT("Hovered"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.Hovered, *BrushJson);
	}
	if (StyleJson->TryGetObjectField(TEXT("Pressed"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.Pressed, *BrushJson);
	}
	if (StyleJson->TryGetObjectField(TEXT("Disabled"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.Disabled, *BrushJson);
	}

	const TSharedPtr<FJsonObject>* PaddingJson = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("NormalPadding"), PaddingJson))
	{
		Style.NormalPadding = ParseMargin(*PaddingJson);
		bApplied = true;
	}
	if (StyleJson->TryGetObjectField(TEXT("PressedPadding"), PaddingJson))
	{
		Style.PressedPadding = ParseMargin(*PaddingJson);
		bApplied = true;
	}

	if (bApplied)
	{
		Button->SetStyle(Style);
	}

	return bApplied;
}

static bool ApplyTextBlockStyleFromJson(FTextBlockStyle& Style, const TSharedPtr<FJsonObject>& StyleJson)
{
	if (!StyleJson.IsValid())
	{
		return false;
	}

	bool bApplied = false;

	int32 FontSize = 0;
	if (StyleJson->TryGetNumberField(TEXT("FontSize"), FontSize))
	{
		Style.Font.Size = FontSize;
		bApplied = true;
	}

	FString FontFamily;
	if (StyleJson->TryGetStringField(TEXT("FontFamily"), FontFamily))
	{
		UObject* FontObj = LoadAssetObject<UObject>(FontFamily);
		if (FontObj)
		{
			Style.Font.FontObject = FontObj;
			bApplied = true;
		}
	}

	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("Color"), ColorObj)
		|| StyleJson->TryGetObjectField(TEXT("ColorAndOpacity"), ColorObj)
		|| StyleJson->TryGetObjectField(TEXT("TintColor"), ColorObj))
	{
		Style.ColorAndOpacity = FSlateColor(ParseColor(*ColorObj));
		bApplied = true;
	}

	return bApplied;
}

static bool ApplyEditableTextBoxStyleFromJson(FEditableTextBoxStyle& Style, const TSharedPtr<FJsonObject>& StyleJson)
{
	if (!StyleJson.IsValid())
	{
		return false;
	}

	bool bApplied = false;

	const TSharedPtr<FJsonObject>* BrushJson = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("BackgroundImageNormal"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.BackgroundImageNormal, *BrushJson);
	}
	if (StyleJson->TryGetObjectField(TEXT("BackgroundImageHovered"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.BackgroundImageHovered, *BrushJson);
	}
	if (StyleJson->TryGetObjectField(TEXT("BackgroundImageFocused"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.BackgroundImageFocused, *BrushJson);
	}
	if (StyleJson->TryGetObjectField(TEXT("BackgroundImageReadOnly"), BrushJson))
	{
		bApplied |= ApplySlateBrushFromJson(Style.BackgroundImageReadOnly, *BrushJson);
	}

	const TSharedPtr<FJsonObject>* MarginJson = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("Padding"), MarginJson))
	{
		Style.Padding = ParseMargin(*MarginJson);
		bApplied = true;
	}
	if (StyleJson->TryGetObjectField(TEXT("HScrollBarPadding"), MarginJson))
	{
		Style.HScrollBarPadding = ParseMargin(*MarginJson);
		bApplied = true;
	}
	if (StyleJson->TryGetObjectField(TEXT("VScrollBarPadding"), MarginJson))
	{
		Style.VScrollBarPadding = ParseMargin(*MarginJson);
		bApplied = true;
	}

	const TSharedPtr<FJsonObject>* ColorObj = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("ForegroundColor"), ColorObj))
	{
		Style.ForegroundColor = FSlateColor(ParseColor(*ColorObj));
		bApplied = true;
	}
	if (StyleJson->TryGetObjectField(TEXT("FocusedForegroundColor"), ColorObj))
	{
		Style.FocusedForegroundColor = FSlateColor(ParseColor(*ColorObj));
		bApplied = true;
	}
	if (StyleJson->TryGetObjectField(TEXT("ReadOnlyForegroundColor"), ColorObj))
	{
		Style.ReadOnlyForegroundColor = FSlateColor(ParseColor(*ColorObj));
		bApplied = true;
	}
	if (StyleJson->TryGetObjectField(TEXT("BackgroundColor"), ColorObj))
	{
		Style.BackgroundColor = FSlateColor(ParseColor(*ColorObj));
		bApplied = true;
	}

	const TSharedPtr<FJsonObject>* TextStyleJson = nullptr;
	if (StyleJson->TryGetObjectField(TEXT("TextStyle"), TextStyleJson))
	{
		bApplied |= ApplyTextBlockStyleFromJson(Style.TextStyle, *TextStyleJson);
	}

	return bApplied;
}

static bool BrushHasMeaningfulData(const FSlateBrush& Brush)
{
	const FLinearColor Tint = Brush.TintColor.GetSpecifiedColor();
	const FVector2D ImageSize = Brush.GetImageSize();
	return Brush.GetResourceObject() != nullptr
		|| !IsMarginNearlyZero(Brush.Margin)
		|| !ImageSize.IsZero()
		|| Tint != FLinearColor::White
		|| Brush.DrawAs != ESlateBrushDrawType::Image;
}

static TSharedPtr<FJsonObject> BrushToJson(const FSlateBrush& Brush)
{
	if (!BrushHasMeaningfulData(Brush))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	bool bHasData = false;

	if (UObject* ResourceObject = Brush.GetResourceObject())
	{
		Json->SetStringField(TEXT("Texture"), ResourceObject->GetPathName());
		bHasData = true;
	}

	const FLinearColor Tint = Brush.TintColor.GetSpecifiedColor();
	if (Tint != FLinearColor::White)
	{
		Json->SetObjectField(TEXT("TintColor"), MakeColorJson(Tint));
		bHasData = true;
	}

	if (!IsMarginNearlyZero(Brush.Margin))
	{
		Json->SetObjectField(TEXT("Margin"), MakeMarginJson(Brush.Margin));
		bHasData = true;
	}

	const FVector2D ImageSize = Brush.GetImageSize();
	if (!ImageSize.IsZero())
	{
		TSharedPtr<FJsonObject> SizeJson = MakeShared<FJsonObject>();
		SizeJson->SetNumberField(TEXT("Width"), ImageSize.X);
		SizeJson->SetNumberField(TEXT("Height"), ImageSize.Y);
		Json->SetObjectField(TEXT("ImageSize"), SizeJson);
		bHasData = true;
	}

	if (Brush.DrawAs != ESlateBrushDrawType::Image)
	{
		Json->SetStringField(TEXT("DrawAs"), BrushDrawTypeToString(Brush.DrawAs));
		bHasData = true;
	}

	return bHasData ? Json : nullptr;
}

static TSharedPtr<FJsonObject> ButtonStyleToJson(const FButtonStyle& Style)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	bool bHasData = false;

	auto AddBrushField = [&](const TCHAR* FieldName, const FSlateBrush& Brush)
	{
		if (TSharedPtr<FJsonObject> BrushJson = BrushToJson(Brush))
		{
			Json->SetObjectField(FieldName, BrushJson);
			bHasData = true;
		}
	};

	AddBrushField(TEXT("Normal"), Style.Normal);
	AddBrushField(TEXT("Hovered"), Style.Hovered);
	AddBrushField(TEXT("Pressed"), Style.Pressed);
	AddBrushField(TEXT("Disabled"), Style.Disabled);

	if (!IsMarginNearlyZero(Style.NormalPadding))
	{
		Json->SetObjectField(TEXT("NormalPadding"), MakeMarginJson(Style.NormalPadding));
		bHasData = true;
	}
	if (!IsMarginNearlyZero(Style.PressedPadding))
	{
		Json->SetObjectField(TEXT("PressedPadding"), MakeMarginJson(Style.PressedPadding));
		bHasData = true;
	}

	return bHasData ? Json : nullptr;
}

static TSharedPtr<FJsonObject> TextBlockStyleToJson(const FTextBlockStyle& Style)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	bool bHasData = false;

	if (Style.Font.Size != 24)
	{
		Json->SetNumberField(TEXT("FontSize"), Style.Font.Size);
		bHasData = true;
	}

	if (Style.Font.FontObject)
	{
		Json->SetStringField(TEXT("FontFamily"), Style.Font.FontObject->GetPathName());
		bHasData = true;
	}

	const FLinearColor Color = Style.ColorAndOpacity.GetSpecifiedColor();
	if (Color != FLinearColor::White)
	{
		Json->SetObjectField(TEXT("Color"), MakeColorJson(Color));
		bHasData = true;
	}

	return bHasData ? Json : nullptr;
}

static TSharedPtr<FJsonObject> EditableTextBoxStyleToJson(const FEditableTextBoxStyle& Style)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	bool bHasData = false;

	auto AddBrushField = [&](const TCHAR* FieldName, const FSlateBrush& Brush)
	{
		if (TSharedPtr<FJsonObject> BrushJson = BrushToJson(Brush))
		{
			Json->SetObjectField(FieldName, BrushJson);
			bHasData = true;
		}
	};

	AddBrushField(TEXT("BackgroundImageNormal"), Style.BackgroundImageNormal);
	AddBrushField(TEXT("BackgroundImageHovered"), Style.BackgroundImageHovered);
	AddBrushField(TEXT("BackgroundImageFocused"), Style.BackgroundImageFocused);
	AddBrushField(TEXT("BackgroundImageReadOnly"), Style.BackgroundImageReadOnly);

	if (!IsMarginNearlyZero(Style.Padding))
	{
		Json->SetObjectField(TEXT("Padding"), MakeMarginJson(Style.Padding));
		bHasData = true;
	}
	if (!IsMarginNearlyZero(Style.HScrollBarPadding))
	{
		Json->SetObjectField(TEXT("HScrollBarPadding"), MakeMarginJson(Style.HScrollBarPadding));
		bHasData = true;
	}
	if (!IsMarginNearlyZero(Style.VScrollBarPadding))
	{
		Json->SetObjectField(TEXT("VScrollBarPadding"), MakeMarginJson(Style.VScrollBarPadding));
		bHasData = true;
	}

	const FLinearColor ForegroundColor = Style.ForegroundColor.GetSpecifiedColor();
	if (ForegroundColor != FLinearColor::White)
	{
		Json->SetObjectField(TEXT("ForegroundColor"), MakeColorJson(ForegroundColor));
		bHasData = true;
	}

	const FLinearColor FocusedForegroundColor = Style.FocusedForegroundColor.GetSpecifiedColor();
	if (FocusedForegroundColor != FLinearColor::White)
	{
		Json->SetObjectField(TEXT("FocusedForegroundColor"), MakeColorJson(FocusedForegroundColor));
		bHasData = true;
	}

	const FLinearColor ReadOnlyForegroundColor = Style.ReadOnlyForegroundColor.GetSpecifiedColor();
	if (ReadOnlyForegroundColor != FLinearColor::White)
	{
		Json->SetObjectField(TEXT("ReadOnlyForegroundColor"), MakeColorJson(ReadOnlyForegroundColor));
		bHasData = true;
	}

	const FLinearColor BackgroundColor = Style.BackgroundColor.GetSpecifiedColor();
	if (BackgroundColor != FLinearColor::White)
	{
		Json->SetObjectField(TEXT("BackgroundColor"), MakeColorJson(BackgroundColor));
		bHasData = true;
	}

	if (TSharedPtr<FJsonObject> TextStyleJson = TextBlockStyleToJson(Style.TextStyle))
	{
		Json->SetObjectField(TEXT("TextStyle"), TextStyleJson);
		bHasData = true;
	}

	return bHasData ? Json : nullptr;
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
			UObject* FontObj = LoadAssetObject<UObject>(FontFamily);
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

		const TSharedPtr<FJsonObject>* BrushObj = nullptr;
		if (Props->TryGetObjectField(TEXT("Brush"), BrushObj))
		{
			FSlateBrush Brush = Img->GetBrush();
			if (ApplySlateBrushFromJson(Brush, *BrushObj))
			{
				Img->SetBrush(Brush);
			}
		}
		else
		{
			FString BrushPath;
			if (Props->TryGetStringField(TEXT("Brush"), BrushPath))
			{
				UTexture2D* Tex = LoadAssetObject<UTexture2D>(BrushPath);
				if (Tex) Img->SetBrushFromTexture(Tex, true);
			}
		}
	}

	// Button
	if (UButton* Btn = Cast<UButton>(Widget))
	{
		const TSharedPtr<FJsonObject>* StyleObj;
		if (Props->TryGetObjectField(TEXT("ButtonStyle"), StyleObj))
		{
			ApplyButtonStyleFromJson(Btn, *StyleObj);
		}

		const TSharedPtr<FJsonObject>* BgColorObj;
		if (Props->TryGetObjectField(TEXT("BackgroundColor"), BgColorObj))
		{
			Btn->SetBackgroundColor(ParseColor(*BgColorObj));
		}

		const TSharedPtr<FJsonObject>* ColorObj;
		if (Props->TryGetObjectField(TEXT("ColorAndOpacity"), ColorObj))
		{
			Btn->SetColorAndOpacity(ParseColor(*ColorObj));
		}
	}

	// EditableTextBox
	if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(Widget))
	{
		FString Text;
		if (Props->TryGetStringField(TEXT("Text"), Text))
		{
			EditableTextBox->SetText(FText::FromString(Text));
		}

		FString HintText;
		if (Props->TryGetStringField(TEXT("HintText"), HintText))
		{
			EditableTextBox->SetHintText(FText::FromString(HintText));
		}

		bool bIsPassword = false;
		if (Props->TryGetBoolField(TEXT("IsPassword"), bIsPassword))
		{
			EditableTextBox->SetIsPassword(bIsPassword);
		}

		bool bIsReadOnly = false;
		if (Props->TryGetBoolField(TEXT("IsReadOnly"), bIsReadOnly))
		{
			EditableTextBox->SetIsReadOnly(bIsReadOnly);
		}

		double MinDesiredWidth = 0.0;
		if (Props->TryGetNumberField(TEXT("MinimumDesiredWidth"), MinDesiredWidth))
		{
			EditableTextBox->SetMinDesiredWidth(MinDesiredWidth);
		}

		FString Justify;
		if (Props->TryGetStringField(TEXT("Justification"), Justify))
		{
			if (Justify == TEXT("Center")) EditableTextBox->SetJustification(ETextJustify::Center);
			else if (Justify == TEXT("Right")) EditableTextBox->SetJustification(ETextJustify::Right);
			else EditableTextBox->SetJustification(ETextJustify::Left);
		}

		FEditableTextBoxStyle Style = EditableTextBox->GetWidgetStyle();
		bool bStyleChanged = false;

		const TSharedPtr<FJsonObject>* StyleObj = nullptr;
		if (Props->TryGetObjectField(TEXT("WidgetStyle"), StyleObj))
		{
			bStyleChanged |= ApplyEditableTextBoxStyleFromJson(Style, *StyleObj);
		}

		int32 FontSize = 0;
		if (Props->TryGetNumberField(TEXT("FontSize"), FontSize))
		{
			Style.TextStyle.Font.Size = FontSize;
			bStyleChanged = true;
		}

		FString FontFamily;
		if (Props->TryGetStringField(TEXT("FontFamily"), FontFamily))
		{
			UObject* FontObj = LoadAssetObject<UObject>(FontFamily);
			if (FontObj)
			{
				Style.TextStyle.Font.FontObject = FontObj;
				bStyleChanged = true;
			}
		}

		const TSharedPtr<FJsonObject>* ColorObj = nullptr;
		if (Props->TryGetObjectField(TEXT("Color"), ColorObj)
			|| Props->TryGetObjectField(TEXT("ForegroundColor"), ColorObj))
		{
			const FLinearColor TextColor = ParseColor(*ColorObj);
			Style.TextStyle.ColorAndOpacity = FSlateColor(TextColor);
			Style.ForegroundColor = FSlateColor(TextColor);
			Style.FocusedForegroundColor = FSlateColor(TextColor);
			Style.ReadOnlyForegroundColor = FSlateColor(TextColor);
			bStyleChanged = true;
		}

		if (Props->TryGetObjectField(TEXT("BackgroundColor"), ColorObj))
		{
			Style.BackgroundColor = FSlateColor(ParseColor(*ColorObj));
			bStyleChanged = true;
		}

		if (bStyleChanged)
		{
			EditableTextBox->SetWidgetStyle(Style);
		}
	}

	// SpinBox
	if (USpinBox* SpinBox = Cast<USpinBox>(Widget))
	{
		double Value = 0.0;
		if (Props->TryGetNumberField(TEXT("Value"), Value))
		{
			SpinBox->SetValue(static_cast<float>(Value));
		}

		double NumericProp = 0.0;
		if (Props->TryGetNumberField(TEXT("MinValue"), NumericProp))
		{
			SpinBox->SetMinValue(static_cast<float>(NumericProp));
		}
		if (Props->TryGetNumberField(TEXT("MaxValue"), NumericProp))
		{
			SpinBox->SetMaxValue(static_cast<float>(NumericProp));
		}
		if (Props->TryGetNumberField(TEXT("MinSliderValue"), NumericProp))
		{
			SpinBox->SetMinSliderValue(static_cast<float>(NumericProp));
		}
		if (Props->TryGetNumberField(TEXT("MaxSliderValue"), NumericProp))
		{
			SpinBox->SetMaxSliderValue(static_cast<float>(NumericProp));
		}
		if (Props->TryGetNumberField(TEXT("Delta"), NumericProp))
		{
			SpinBox->SetDelta(static_cast<float>(NumericProp));
		}
		if (Props->TryGetNumberField(TEXT("MinimumDesiredWidth"), NumericProp))
		{
			SpinBox->SetMinDesiredWidth(static_cast<float>(NumericProp));
		}

		int32 IntProp = 0;
		if (Props->TryGetNumberField(TEXT("MinFractionalDigits"), IntProp))
		{
			SpinBox->SetMinFractionalDigits(IntProp);
		}
		if (Props->TryGetNumberField(TEXT("MaxFractionalDigits"), IntProp))
		{
			SpinBox->SetMaxFractionalDigits(IntProp);
		}

		FString Justify;
		if (Props->TryGetStringField(TEXT("Justification"), Justify))
		{
			if (Justify == TEXT("Center")) SpinBox->SetJustification(ETextJustify::Center);
			else if (Justify == TEXT("Right")) SpinBox->SetJustification(ETextJustify::Right);
			else SpinBox->SetJustification(ETextJustify::Left);
		}

		int32 FontSize = 0;
		if (Props->TryGetNumberField(TEXT("FontSize"), FontSize))
		{
			FSlateFontInfo Font = SpinBox->GetFont();
			Font.Size = FontSize;
			SpinBox->SetFont(Font);
		}

		FString FontFamily;
		if (Props->TryGetStringField(TEXT("FontFamily"), FontFamily))
		{
			FSlateFontInfo Font = SpinBox->GetFont();
			UObject* FontObj = LoadAssetObject<UObject>(FontFamily);
			if (FontObj)
			{
				Font.FontObject = FontObj;
				SpinBox->SetFont(Font);
			}
		}

		const TSharedPtr<FJsonObject>* ColorObj = nullptr;
		if (Props->TryGetObjectField(TEXT("Color"), ColorObj)
			|| Props->TryGetObjectField(TEXT("ForegroundColor"), ColorObj))
		{
			SpinBox->SetForegroundColor(FSlateColor(ParseColor(*ColorObj)));
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

	// ScaleBox
	if (UScaleBox* ScaleBox = Cast<UScaleBox>(Widget))
	{
		FString Stretch;
		if (Props->TryGetStringField(TEXT("Stretch"), Stretch))
			ScaleBox->SetStretch(ParseStretch(Stretch));

		FString StretchDirection;
		if (Props->TryGetStringField(TEXT("StretchDirection"), StretchDirection))
			ScaleBox->SetStretchDirection(ParseStretchDirection(StretchDirection));

		bool bIgnoreInheritedScale = false;
		if (Props->TryGetBoolField(TEXT("IgnoreInheritedScale"), bIgnoreInheritedScale))
			ScaleBox->SetIgnoreInheritedScale(bIgnoreInheritedScale);

		double UserSpecifiedScale = 1.0;
		if (Props->TryGetNumberField(TEXT("UserSpecifiedScale"), UserSpecifiedScale))
			ScaleBox->SetUserSpecifiedScale(UserSpecifiedScale);
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

static FString HAlignToString(EHorizontalAlignment Align)
{
	switch (Align)
	{
	case HAlign_Center: return TEXT("Center");
	case HAlign_Right: return TEXT("Right");
	case HAlign_Fill: return TEXT("Fill");
	default: return TEXT("Left");
	}
}

static FString VAlignToString(EVerticalAlignment Align)
{
	switch (Align)
	{
	case VAlign_Center: return TEXT("Center");
	case VAlign_Bottom: return TEXT("Bottom");
	case VAlign_Fill: return TEXT("Fill");
	default: return TEXT("Top");
	}
}

static FVector2D GetDefaultCanvasAlignmentForAnchorName(const FString& AnchName)
{
	if (AnchName == TEXT("Center"))       return FVector2D(0.5f, 0.5f);
	if (AnchName == TEXT("TopCenter"))    return FVector2D(0.5f, 0.0f);
	if (AnchName == TEXT("TopRight"))     return FVector2D(1.0f, 0.0f);
	if (AnchName == TEXT("BottomLeft"))   return FVector2D(0.0f, 1.0f);
	if (AnchName == TEXT("BottomCenter")) return FVector2D(0.5f, 1.0f);
	if (AnchName == TEXT("BottomRight"))  return FVector2D(1.0f, 1.0f);
	if (AnchName == TEXT("CenterLeft"))   return FVector2D(0.0f, 0.5f);
	if (AnchName == TEXT("CenterRight"))  return FVector2D(1.0f, 0.5f);
	return FVector2D::ZeroVector;
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
		SS->SetHorizontalAlignment(HAlign_Fill);
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) SS->SetHorizontalAlignment(ParseHAlign(HA));
		SS->SetPadding(FMargin(0.f));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) SS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// ButtonSlot
	if (UButtonSlot* BS = Cast<UButtonSlot>(Widget->Slot))
	{
		BS->SetHorizontalAlignment(HAlign_Center);
		BS->SetVerticalAlignment(VAlign_Center);
		BS->SetPadding(FMargin(4.f, 2.f));
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) BS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) BS->SetVerticalAlignment(ParseVAlign(VA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) BS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// BorderSlot
	if (UBorderSlot* BS = Cast<UBorderSlot>(Widget->Slot))
	{
		BS->SetHorizontalAlignment(HAlign_Fill);
		BS->SetVerticalAlignment(VAlign_Fill);
		BS->SetPadding(FMargin(4.f, 2.f));
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) BS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) BS->SetVerticalAlignment(ParseVAlign(VA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) BS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// SizeBoxSlot
	if (USizeBoxSlot* BS = Cast<USizeBoxSlot>(Widget->Slot))
	{
		BS->SetHorizontalAlignment(HAlign_Fill);
		BS->SetVerticalAlignment(VAlign_Fill);
		BS->SetPadding(FMargin(0.f));
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) BS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) BS->SetVerticalAlignment(ParseVAlign(VA));
		const TSharedPtr<FJsonObject>* Pad;
		if (Slot->TryGetObjectField(TEXT("Padding"), Pad)) BS->SetPadding(ParseMargin(*Pad));
		return;
	}

	// ScaleBoxSlot
	if (UScaleBoxSlot* BS = Cast<UScaleBoxSlot>(Widget->Slot))
	{
		BS->SetHorizontalAlignment(HAlign_Center);
		BS->SetVerticalAlignment(VAlign_Center);
		FString HA; if (Slot->TryGetStringField(TEXT("HAlign"), HA)) BS->SetHorizontalAlignment(ParseHAlign(HA));
		FString VA; if (Slot->TryGetStringField(TEXT("VAlign"), VA)) BS->SetVerticalAlignment(ParseVAlign(VA));
		return;
	}

	// OverlaySlot
	if (UOverlaySlot* OS = Cast<UOverlaySlot>(Widget->Slot))
	{
		OS->SetHorizontalAlignment(HAlign_Left);
		OS->SetVerticalAlignment(VAlign_Top);
		OS->SetPadding(FMargin(0.f));
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
	const TSharedPtr<FJsonObject>* PropsJson = nullptr;
	if (Json->TryGetObjectField(TEXT("Properties"), PropsJson))
		SetWidgetProperties(NewWidget, *PropsJson);

	// Add to parent or set as root
	if (Parent)
	{
		Parent->AddChild(NewWidget);
		const TSharedPtr<FJsonObject>* SlotJson = nullptr;
		if (Json->TryGetObjectField(TEXT("Slot"), SlotJson))
			SetSlotProperties(NewWidget, *SlotJson);
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

		ETextJustify::Type Justification = ETextJustify::Left;
		if (TryGetTextJustification(TB, Justification) && Justification != ETextJustify::Left)
		{
			Props->SetStringField(TEXT("Justification"), TextJustificationToString(Justification));
			bHasProps = true;
		}
	}

	// Image
	if (UImage* Img = Cast<UImage>(Widget))
	{
		FLinearColor Color = Img->GetColorAndOpacity();
		if (Color != FLinearColor::White) { Props->SetObjectField(TEXT("Color"), ColorToJson(Color)); bHasProps = true; }

		if (TSharedPtr<FJsonObject> BrushJson = BrushToJson(Img->GetBrush()))
		{
			Props->SetObjectField(TEXT("Brush"), BrushJson);
			bHasProps = true;
		}
	}

	// Button
	if (UButton* Btn = Cast<UButton>(Widget))
	{
		if (TSharedPtr<FJsonObject> ButtonStyleJson = ButtonStyleToJson(Btn->GetStyle()))
		{
			Props->SetObjectField(TEXT("ButtonStyle"), ButtonStyleJson);
			bHasProps = true;
		}

		FLinearColor BgColor = Btn->GetBackgroundColor();
		if (BgColor != FLinearColor::White)
		{
			Props->SetObjectField(TEXT("BackgroundColor"), ColorToJson(BgColor));
			bHasProps = true;
		}

		FLinearColor Color = Btn->GetColorAndOpacity();
		if (Color != FLinearColor::White)
		{
			Props->SetObjectField(TEXT("ColorAndOpacity"), ColorToJson(Color));
			bHasProps = true;
		}
	}

	// EditableTextBox
	if (UEditableTextBox* EditableTextBox = Cast<UEditableTextBox>(Widget))
	{
		const FString Text = EditableTextBox->GetText().ToString();
		if (!Text.IsEmpty()) { Props->SetStringField(TEXT("Text"), Text); bHasProps = true; }

		const FString HintText = EditableTextBox->GetHintText().ToString();
		if (!HintText.IsEmpty()) { Props->SetStringField(TEXT("HintText"), HintText); bHasProps = true; }

		if (EditableTextBox->GetIsPassword()) { Props->SetBoolField(TEXT("IsPassword"), true); bHasProps = true; }
		if (EditableTextBox->GetIsReadOnly()) { Props->SetBoolField(TEXT("IsReadOnly"), true); bHasProps = true; }

		const float MinimumDesiredWidth = EditableTextBox->GetMinimumDesiredWidth();
		if (!FMath::IsNearlyZero(MinimumDesiredWidth))
		{
			Props->SetNumberField(TEXT("MinimumDesiredWidth"), MinimumDesiredWidth);
			bHasProps = true;
		}

		if (EditableTextBox->GetJustification() != ETextJustify::Left)
		{
			Props->SetStringField(TEXT("Justification"), TextJustificationToString(EditableTextBox->GetJustification()));
			bHasProps = true;
		}

		if (TSharedPtr<FJsonObject> StyleJson = EditableTextBoxStyleToJson(EditableTextBox->GetWidgetStyle()))
		{
			Props->SetObjectField(TEXT("WidgetStyle"), StyleJson);
			bHasProps = true;
		}

		const FLinearColor TextColor = EditableTextBox->GetWidgetStyle().TextStyle.ColorAndOpacity.GetSpecifiedColor();
		if (TextColor != FLinearColor::White)
		{
			Props->SetObjectField(TEXT("Color"), ColorToJson(TextColor));
			bHasProps = true;
		}

		if (EditableTextBox->GetWidgetStyle().TextStyle.Font.Size != 24)
		{
			Props->SetNumberField(TEXT("FontSize"), EditableTextBox->GetWidgetStyle().TextStyle.Font.Size);
			bHasProps = true;
		}
	}

	// SpinBox
	if (USpinBox* SpinBox = Cast<USpinBox>(Widget))
	{
		Props->SetNumberField(TEXT("Value"), SpinBox->GetValue());
		Props->SetNumberField(TEXT("MinValue"), SpinBox->GetMinValue());
		Props->SetNumberField(TEXT("MaxValue"), SpinBox->GetMaxValue());
		Props->SetNumberField(TEXT("MinSliderValue"), SpinBox->GetMinSliderValue());
		Props->SetNumberField(TEXT("MaxSliderValue"), SpinBox->GetMaxSliderValue());
		Props->SetNumberField(TEXT("Delta"), SpinBox->GetDelta());
		Props->SetNumberField(TEXT("MinimumDesiredWidth"), SpinBox->GetMinDesiredWidth());
		Props->SetNumberField(TEXT("MinFractionalDigits"), SpinBox->GetMinFractionalDigits());
		Props->SetNumberField(TEXT("MaxFractionalDigits"), SpinBox->GetMaxFractionalDigits());
		if (SpinBox->GetJustification() != ETextJustify::Left)
		{
			Props->SetStringField(TEXT("Justification"), TextJustificationToString(SpinBox->GetJustification()));
		}
		if (SpinBox->GetFont().Size != 24)
		{
			Props->SetNumberField(TEXT("FontSize"), SpinBox->GetFont().Size);
		}
		const FLinearColor TextColor = SpinBox->GetForegroundColor().GetSpecifiedColor();
		if (TextColor != FLinearColor::White)
		{
			Props->SetObjectField(TEXT("ForegroundColor"), ColorToJson(TextColor));
		}
		bHasProps = true;
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
	if (USizeBox* SB = Cast<USizeBox>(Widget))
	{
		const float WidthOverride = SB->GetWidthOverride();
		if (!FMath::IsNearlyZero(WidthOverride))
		{
			Props->SetNumberField(TEXT("WidthOverride"), WidthOverride);
			bHasProps = true;
		}

		const float HeightOverride = SB->GetHeightOverride();
		if (!FMath::IsNearlyZero(HeightOverride))
		{
			Props->SetNumberField(TEXT("HeightOverride"), HeightOverride);
			bHasProps = true;
		}

		const float MinDesiredWidth = SB->GetMinDesiredWidth();
		if (!FMath::IsNearlyZero(MinDesiredWidth))
		{
			Props->SetNumberField(TEXT("MinDesiredWidth"), MinDesiredWidth);
			bHasProps = true;
		}

		const float MinDesiredHeight = SB->GetMinDesiredHeight();
		if (!FMath::IsNearlyZero(MinDesiredHeight))
		{
			Props->SetNumberField(TEXT("MinDesiredHeight"), MinDesiredHeight);
			bHasProps = true;
		}

		const float MaxDesiredWidth = SB->GetMaxDesiredWidth();
		if (!FMath::IsNearlyZero(MaxDesiredWidth))
		{
			Props->SetNumberField(TEXT("MaxDesiredWidth"), MaxDesiredWidth);
			bHasProps = true;
		}

		const float MaxDesiredHeight = SB->GetMaxDesiredHeight();
		if (!FMath::IsNearlyZero(MaxDesiredHeight))
		{
			Props->SetNumberField(TEXT("MaxDesiredHeight"), MaxDesiredHeight);
			bHasProps = true;
		}
	}

	if (UScaleBox* ScaleBox = Cast<UScaleBox>(Widget))
	{
		if (ScaleBox->GetStretch() != EStretch::ScaleToFit)
		{
			Props->SetStringField(TEXT("Stretch"), StretchToString(ScaleBox->GetStretch()));
			bHasProps = true;
		}

		if (ScaleBox->GetStretchDirection() != EStretchDirection::Both)
		{
			Props->SetStringField(TEXT("StretchDirection"), StretchDirectionToString(ScaleBox->GetStretchDirection()));
			bHasProps = true;
		}

		if (ScaleBox->IsIgnoreInheritedScale())
		{
			Props->SetBoolField(TEXT("IgnoreInheritedScale"), true);
			bHasProps = true;
		}

		if (!FMath::IsNearlyEqual(ScaleBox->GetUserSpecifiedScale(), 1.0f))
		{
			Props->SetNumberField(TEXT("UserSpecifiedScale"), ScaleBox->GetUserSpecifiedScale());
			bHasProps = true;
		}
	}

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
		// Only omit alignment when it matches the preset that generation will recreate.
		// A centered anchor with Alignment=(0,0) is *not* default for the "Center" preset.
		const FVector2D DefaultAlign = GetDefaultCanvasAlignmentForAnchorName(AnchName);
		const bool bShouldExportAlign =
			AnchName.IsEmpty()
				? !Align.IsNearlyZero()
				: !Align.Equals(DefaultAlign, 0.01f);

		if (bShouldExportAlign)
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

		SlotJson->SetBoolField(TEXT("AutoSize"), CS->GetAutoSize());

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

	if (UScrollBoxSlot* SS = Cast<UScrollBoxSlot>(Widget->Slot))
	{
		if (SS->GetHorizontalAlignment() != HAlign_Fill)
		{
			SlotJson->SetStringField(TEXT("HAlign"), HAlignToString(SS->GetHorizontalAlignment()));
		}

		FMargin Pad = SS->GetPadding();
		if (!IsMarginNearlyZero(Pad))
		{
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		}
		bHasSlot = true;
	}

	if (UButtonSlot* BS = Cast<UButtonSlot>(Widget->Slot))
	{
		if (BS->GetHorizontalAlignment() != HAlign_Center)
		{
			SlotJson->SetStringField(TEXT("HAlign"), HAlignToString(BS->GetHorizontalAlignment()));
		}
		if (BS->GetVerticalAlignment() != VAlign_Center)
		{
			SlotJson->SetStringField(TEXT("VAlign"), VAlignToString(BS->GetVerticalAlignment()));
		}

		const FMargin Pad = BS->GetPadding();
		const FMargin DefaultPad(4.f, 2.f);
		if (!(Pad == DefaultPad))
		{
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		}
		bHasSlot = true;
	}

	if (UBorderSlot* BS = Cast<UBorderSlot>(Widget->Slot))
	{
		if (BS->GetHorizontalAlignment() != HAlign_Fill)
		{
			SlotJson->SetStringField(TEXT("HAlign"), HAlignToString(BS->GetHorizontalAlignment()));
		}
		if (BS->GetVerticalAlignment() != VAlign_Fill)
		{
			SlotJson->SetStringField(TEXT("VAlign"), VAlignToString(BS->GetVerticalAlignment()));
		}

		const FMargin Pad = BS->GetPadding();
		const FMargin DefaultPad(4.f, 2.f);
		if (!(Pad == DefaultPad))
		{
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		}
		bHasSlot = true;
	}

	if (USizeBoxSlot* BS = Cast<USizeBoxSlot>(Widget->Slot))
	{
		if (BS->GetHorizontalAlignment() != HAlign_Fill)
		{
			SlotJson->SetStringField(TEXT("HAlign"), HAlignToString(BS->GetHorizontalAlignment()));
		}
		if (BS->GetVerticalAlignment() != VAlign_Fill)
		{
			SlotJson->SetStringField(TEXT("VAlign"), VAlignToString(BS->GetVerticalAlignment()));
		}

		const FMargin Pad = BS->GetPadding();
		if (!IsMarginNearlyZero(Pad))
		{
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		}
		bHasSlot = true;
	}

	if (UScaleBoxSlot* BS = Cast<UScaleBoxSlot>(Widget->Slot))
	{
		if (BS->GetHorizontalAlignment() != HAlign_Center)
		{
			SlotJson->SetStringField(TEXT("HAlign"), HAlignToString(BS->GetHorizontalAlignment()));
		}
		if (BS->GetVerticalAlignment() != VAlign_Center)
		{
			SlotJson->SetStringField(TEXT("VAlign"), VAlignToString(BS->GetVerticalAlignment()));
		}
		bHasSlot = true;
	}

	if (UOverlaySlot* OS = Cast<UOverlaySlot>(Widget->Slot))
	{
		if (OS->GetHorizontalAlignment() != HAlign_Left)
		{
			SlotJson->SetStringField(TEXT("HAlign"), HAlignToString(OS->GetHorizontalAlignment()));
		}
		if (OS->GetVerticalAlignment() != VAlign_Top)
		{
			SlotJson->SetStringField(TEXT("VAlign"), VAlignToString(OS->GetVerticalAlignment()));
		}

		const FMargin Pad = OS->GetPadding();
		if (!IsMarginNearlyZero(Pad))
		{
			SlotJson->SetObjectField(TEXT("Padding"), MarginToJson(Pad));
		}
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

	const FString OutputPath = ResolveExportJsonPath(
		OutputFileName,
		WidgetBP->GetName());
	if (OutputPath.IsEmpty())
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("导出目标路径为空: %s"), *WidgetPath);
		return false;
	}

	if (const TSharedPtr<FJsonObject> ExistingConfig = TryLoadExistingJsonObject(OutputPath))
	{
		PreserveIngredientPlaceholders(Config, ExistingConfig);
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
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
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

	FString JsonPath = ResolveWidgetTemplatePath(JsonFileName);
	TSharedPtr<FJsonObject> Config = LoadJsonConfig(JsonPath);
	if (!Config.IsValid()) return nullptr;

	FString WidgetName;
	if (!Config->TryGetStringField(TEXT("WidgetName"), WidgetName))
	{
		UE_LOG(LogWidgetFactory, Error, TEXT("模板缺少 'WidgetName' 字段"));
		return nullptr;
	}

	UClass* ParentClass = UUserWidget::StaticClass();
	FString ParentClassPath;
	if (Config->TryGetStringField(TEXT("ParentClass"), ParentClassPath) && !ParentClassPath.IsEmpty())
	{
		if (UClass* LoadedParentClass = LoadClass<UUserWidget>(nullptr, *ParentClassPath))
		{
			ParentClass = LoadedParentClass;
		}
		else
		{
			UE_LOG(LogWidgetFactory, Error, TEXT("无法加载 ParentClass: %s"), *ParentClassPath);
			return nullptr;
		}
	}

	UWidgetBlueprint* BP = CreateWidgetBlueprint(PackagePath, WidgetName, ParentClass);
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
