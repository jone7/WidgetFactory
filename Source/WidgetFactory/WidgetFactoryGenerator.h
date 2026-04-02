// WidgetFactoryGenerator.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Dom/JsonObject.h"
#include "WidgetFactoryGenerator.generated.h"

class UWidgetBlueprint;
class UWidget;
class UPanelWidget;
class UWidgetTree;

/**
 * 从 JSON 模板自动生成 UMG Widget Blueprint。
 *
 * 支持功能：
 *   - 完整控件树构建（类型、层级、属性、Slot、变量绑定）
 *   - 可选 UnLua 脚本绑定（自动检测 UnLua 插件是否存在）
 *   - 可自定义模板目录
 *
 * 使用方式：
 *   - 控制台: WidgetFactory.Generate <模板名>
 *   - 控制台: WidgetFactory.GenerateAll
 *   - 菜单:   工具 > Widget 工厂
 *   - Python:  unreal.WidgetFactoryGenerator.generate_from_json("模板名")
 */
UCLASS()
class WIDGETFACTORY_API UWidgetFactoryGenerator : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 从 JSON 模板生成 Widget Blueprint
	 * @param JsonFileName  模板名（不含路径和扩展名），或显式 JSON 路径
	 * @param PackagePath   输出路径，默认 /Game/UI
	 * @return 生成的 Widget Blueprint，失败返回 nullptr
	 */
	UFUNCTION(BlueprintCallable, Category = "WidgetFactory")
	static UWidgetBlueprint* GenerateFromJson(
		const FString& JsonFileName,
		const FString& PackagePath = TEXT("/Game/UI"));

	/** 批量生成模板目录下所有模板 */
	UFUNCTION(BlueprintCallable, Category = "WidgetFactory")
	static void GenerateAllWidgets(const FString& PackagePath = TEXT("/Game/UI"));

	/**
	 * 从已有 Widget Blueprint 导出 JSON 模板（反向工程）
	 * @param WidgetPath  资源路径，如 /Game/UI/WBP_GameHUD
	 * @param OutputFileName  输出文件名（不含路径和扩展名），为空则使用资源名
	 * @return 是否导出成功
	 */
	UFUNCTION(BlueprintCallable, Category = "WidgetFactory")
	static bool ExportToJson(
		const FString& WidgetPath,
		const FString& OutputFileName = TEXT(""));

	/** 获取当前模板目录（绝对路径） */
	static FString GetTemplateDirectory();

	/** 设置自定义模板目录（相对于项目根目录，如 "Config/WidgetTemplates"） */
	static void SetTemplateDirectory(const FString& RelativePath);

	/** 检测 UnLua 插件是否可用 */
	static bool IsUnLuaAvailable();

private:
	/** 自定义模板目录（相对路径），为空时使用默认值 */
	static FString CustomTemplateDir;

	static TSharedPtr<FJsonObject> LoadJsonConfig(const FString& JsonPath);
	static UWidgetBlueprint* CreateWidgetBlueprint(const FString& PackagePath, const FString& WidgetName);
	static UWidget* CreateWidgetFromJson(UWidgetTree* WidgetTree, const TSharedPtr<FJsonObject>& WidgetJson, UPanelWidget* Parent);
	static void SetWidgetProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& PropertiesJson);
	static void SetSlotProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotJson);
	static void SetCanvasSlotProperties(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotJson);
	static void SetWidgetAsVariable(UWidget* Widget, const FString& VariableName);
	static UClass* GetWidgetClass(const FString& TypeName);

	// UnLua 绑定（仅在 WITH_UNLUA=1 时有实际实现）
	static void SetupUnLuaBinding(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& UnLuaConfig);
	static void AddEventTickNode(UWidgetBlueprint* WidgetBP);

	// 导出相关
	static TSharedPtr<FJsonObject> ExportWidgetToJson(UWidget* Widget);
	static TSharedPtr<FJsonObject> ExportSlotToJson(UWidget* Widget);
	static TSharedPtr<FJsonObject> ExportPropertiesToJson(UWidget* Widget);
	static FString GetWidgetTypeName(UWidget* Widget);
	static TSharedPtr<FJsonObject> ColorToJson(const FLinearColor& Color);
	static TSharedPtr<FJsonObject> MarginToJson(const FMargin& Margin);
	static FString GetUnLuaModuleName(UWidgetBlueprint* WidgetBP);
};
