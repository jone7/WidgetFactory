// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "WidgetFactoryGenerator.h"

#ifdef WIDGETFACTORY_WidgetFactoryGenerator_generated_h
#error "WidgetFactoryGenerator.generated.h already included, missing '#pragma once' in WidgetFactoryGenerator.h"
#endif
#define WIDGETFACTORY_WidgetFactoryGenerator_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class UWidgetBlueprint;

// ********** Begin Class UWidgetFactoryGenerator **************************************************
#define FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execGenerateAllWidgets); \
	DECLARE_FUNCTION(execGenerateFromJson);


struct Z_Construct_UClass_UWidgetFactoryGenerator_Statics;
WIDGETFACTORY_API UClass* Z_Construct_UClass_UWidgetFactoryGenerator_NoRegister();

#define FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUWidgetFactoryGenerator(); \
	friend struct ::Z_Construct_UClass_UWidgetFactoryGenerator_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend WIDGETFACTORY_API UClass* ::Z_Construct_UClass_UWidgetFactoryGenerator_NoRegister(); \
public: \
	DECLARE_CLASS2(UWidgetFactoryGenerator, UObject, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/WidgetFactory"), Z_Construct_UClass_UWidgetFactoryGenerator_NoRegister) \
	DECLARE_SERIALIZER(UWidgetFactoryGenerator)


#define FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_ENHANCED_CONSTRUCTORS \
	/** Standard constructor, called after all reflected properties have been initialized */ \
	NO_API UWidgetFactoryGenerator(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \
	/** Deleted move- and copy-constructors, should never be used */ \
	UWidgetFactoryGenerator(UWidgetFactoryGenerator&&) = delete; \
	UWidgetFactoryGenerator(const UWidgetFactoryGenerator&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UWidgetFactoryGenerator); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UWidgetFactoryGenerator); \
	DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(UWidgetFactoryGenerator) \
	NO_API virtual ~UWidgetFactoryGenerator();


#define FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_28_PROLOG
#define FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_INCLASS_NO_PURE_DECLS \
	FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h_31_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UWidgetFactoryGenerator;

// ********** End Class UWidgetFactoryGenerator ****************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
