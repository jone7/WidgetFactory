// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "WidgetFactory/WidgetFactoryGenerator.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");
void EmptyLinkFunctionForGeneratedCodeWidgetFactoryGenerator() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UObject();
UMGEDITOR_API UClass* Z_Construct_UClass_UWidgetBlueprint_NoRegister();
UPackage* Z_Construct_UPackage__Script_WidgetFactory();
WIDGETFACTORY_API UClass* Z_Construct_UClass_UWidgetFactoryGenerator();
WIDGETFACTORY_API UClass* Z_Construct_UClass_UWidgetFactoryGenerator_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UWidgetFactoryGenerator Function GenerateAllWidgets **********************
struct Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics
{
	struct WidgetFactoryGenerator_eventGenerateAllWidgets_Parms
	{
		FString PackagePath;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "WidgetFactory" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/** \xe6\x89\xb9\xe9\x87\x8f\xe7\x94\x9f\xe6\x88\x90\xe6\xa8\xa1\xe6\x9d\xbf\xe7\x9b\xae\xe5\xbd\x95\xe4\xb8\x8b\xe6\x89\x80\xe6\x9c\x89\xe6\xa8\xa1\xe6\x9d\xbf */" },
#endif
		{ "CPP_Default_PackagePath", "/Game/UI" },
		{ "ModuleRelativePath", "WidgetFactoryGenerator.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe6\x89\xb9\xe9\x87\x8f\xe7\x94\x9f\xe6\x88\x90\xe6\xa8\xa1\xe6\x9d\xbf\xe7\x9b\xae\xe5\xbd\x95\xe4\xb8\x8b\xe6\x89\x80\xe6\x9c\x89\xe6\xa8\xa1\xe6\x9d\xbf" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PackagePath_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GenerateAllWidgets constinit property declarations ********************
	static const UECodeGen_Private::FStrPropertyParams NewProp_PackagePath;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GenerateAllWidgets constinit property declarations **********************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GenerateAllWidgets Property Definitions *******************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::NewProp_PackagePath = { "PackagePath", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(WidgetFactoryGenerator_eventGenerateAllWidgets_Parms, PackagePath), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PackagePath_MetaData), NewProp_PackagePath_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::NewProp_PackagePath,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::PropPointers) < 2048);
// ********** End Function GenerateAllWidgets Property Definitions *********************************
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UWidgetFactoryGenerator, nullptr, "GenerateAllWidgets", 	Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::PropPointers, 
	UE_ARRAY_COUNT(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::PropPointers), 
sizeof(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::WidgetFactoryGenerator_eventGenerateAllWidgets_Parms),
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::Function_MetaDataParams), Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::WidgetFactoryGenerator_eventGenerateAllWidgets_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UWidgetFactoryGenerator::execGenerateAllWidgets)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_PackagePath);
	P_FINISH;
	P_NATIVE_BEGIN;
	UWidgetFactoryGenerator::GenerateAllWidgets(Z_Param_PackagePath);
	P_NATIVE_END;
}
// ********** End Class UWidgetFactoryGenerator Function GenerateAllWidgets ************************

// ********** Begin Class UWidgetFactoryGenerator Function GenerateFromJson ************************
struct Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics
{
	struct WidgetFactoryGenerator_eventGenerateFromJson_Parms
	{
		FString JsonFileName;
		FString PackagePath;
		UWidgetBlueprint* ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "WidgetFactory" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n\x09 * \xe4\xbb\x8e JSON \xe6\xa8\xa1\xe6\x9d\xbf\xe7\x94\x9f\xe6\x88\x90 Widget Blueprint\n\x09 * @param JsonFileName  \xe6\xa8\xa1\xe6\x9d\xbf\xe5\x90\x8d\xef\xbc\x88\xe4\xb8\x8d\xe5\x90\xab\xe8\xb7\xaf\xe5\xbe\x84\xe5\x92\x8c\xe6\x89\xa9\xe5\xb1\x95\xe5\x90\x8d\xef\xbc\x89\n\x09 * @param PackagePath   \xe8\xbe\x93\xe5\x87\xba\xe8\xb7\xaf\xe5\xbe\x84\xef\xbc\x8c\xe9\xbb\x98\xe8\xae\xa4 /Game/UI\n\x09 * @return \xe7\x94\x9f\xe6\x88\x90\xe7\x9a\x84 Widget Blueprint\xef\xbc\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xe8\xbf\x94\xe5\x9b\x9e nullptr\n\x09 */" },
#endif
		{ "CPP_Default_PackagePath", "/Game/UI" },
		{ "ModuleRelativePath", "WidgetFactoryGenerator.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xbb\x8e JSON \xe6\xa8\xa1\xe6\x9d\xbf\xe7\x94\x9f\xe6\x88\x90 Widget Blueprint\n@param JsonFileName  \xe6\xa8\xa1\xe6\x9d\xbf\xe5\x90\x8d\xef\xbc\x88\xe4\xb8\x8d\xe5\x90\xab\xe8\xb7\xaf\xe5\xbe\x84\xe5\x92\x8c\xe6\x89\xa9\xe5\xb1\x95\xe5\x90\x8d\xef\xbc\x89\n@param PackagePath   \xe8\xbe\x93\xe5\x87\xba\xe8\xb7\xaf\xe5\xbe\x84\xef\xbc\x8c\xe9\xbb\x98\xe8\xae\xa4 /Game/UI\n@return \xe7\x94\x9f\xe6\x88\x90\xe7\x9a\x84 Widget Blueprint\xef\xbc\x8c\xe5\xa4\xb1\xe8\xb4\xa5\xe8\xbf\x94\xe5\x9b\x9e nullptr" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_JsonFileName_MetaData[] = {
		{ "NativeConst", "" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_PackagePath_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA

// ********** Begin Function GenerateFromJson constinit property declarations **********************
	static const UECodeGen_Private::FStrPropertyParams NewProp_JsonFileName;
	static const UECodeGen_Private::FStrPropertyParams NewProp_PackagePath;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
// ********** End Function GenerateFromJson constinit property declarations ************************
	static const UECodeGen_Private::FFunctionParams FuncParams;
};

// ********** Begin Function GenerateFromJson Property Definitions *********************************
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::NewProp_JsonFileName = { "JsonFileName", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(WidgetFactoryGenerator_eventGenerateFromJson_Parms, JsonFileName), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_JsonFileName_MetaData), NewProp_JsonFileName_MetaData) };
const UECodeGen_Private::FStrPropertyParams Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::NewProp_PackagePath = { "PackagePath", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Str, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(WidgetFactoryGenerator_eventGenerateFromJson_Parms, PackagePath), METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_PackagePath_MetaData), NewProp_PackagePath_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(WidgetFactoryGenerator_eventGenerateFromJson_Parms, ReturnValue), Z_Construct_UClass_UWidgetBlueprint_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::NewProp_JsonFileName,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::NewProp_PackagePath,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::PropPointers) < 2048);
// ********** End Function GenerateFromJson Property Definitions ***********************************
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UWidgetFactoryGenerator, nullptr, "GenerateFromJson", 	Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::PropPointers, 
	UE_ARRAY_COUNT(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::PropPointers), 
sizeof(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::WidgetFactoryGenerator_eventGenerateFromJson_Parms),
RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04022401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::Function_MetaDataParams), Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::WidgetFactoryGenerator_eventGenerateFromJson_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UWidgetFactoryGenerator::execGenerateFromJson)
{
	P_GET_PROPERTY(FStrProperty,Z_Param_JsonFileName);
	P_GET_PROPERTY(FStrProperty,Z_Param_PackagePath);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(UWidgetBlueprint**)Z_Param__Result=UWidgetFactoryGenerator::GenerateFromJson(Z_Param_JsonFileName,Z_Param_PackagePath);
	P_NATIVE_END;
}
// ********** End Class UWidgetFactoryGenerator Function GenerateFromJson **************************

// ********** Begin Class UWidgetFactoryGenerator **************************************************
FClassRegistrationInfo Z_Registration_Info_UClass_UWidgetFactoryGenerator;
UClass* UWidgetFactoryGenerator::GetPrivateStaticClass()
{
	using TClass = UWidgetFactoryGenerator;
	if (!Z_Registration_Info_UClass_UWidgetFactoryGenerator.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			TClass::StaticPackage(),
			TEXT("WidgetFactoryGenerator"),
			Z_Registration_Info_UClass_UWidgetFactoryGenerator.InnerSingleton,
			StaticRegisterNativesUWidgetFactoryGenerator,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_UWidgetFactoryGenerator.InnerSingleton;
}
UClass* Z_Construct_UClass_UWidgetFactoryGenerator_NoRegister()
{
	return UWidgetFactoryGenerator::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UWidgetFactoryGenerator_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \xe4\xbb\x8e JSON \xe6\xa8\xa1\xe6\x9d\xbf\xe8\x87\xaa\xe5\x8a\xa8\xe7\x94\x9f\xe6\x88\x90 UMG Widget Blueprint\xe3\x80\x82\n *\n * \xe6\x94\xaf\xe6\x8c\x81\xe5\x8a\x9f\xe8\x83\xbd\xef\xbc\x9a\n *   - \xe5\xae\x8c\xe6\x95\xb4\xe6\x8e\xa7\xe4\xbb\xb6\xe6\xa0\x91\xe6\x9e\x84\xe5\xbb\xba\xef\xbc\x88\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x81\xe5\xb1\x82\xe7\xba\xa7\xe3\x80\x81\xe5\xb1\x9e\xe6\x80\xa7\xe3\x80\x81Slot\xe3\x80\x81\xe5\x8f\x98\xe9\x87\x8f\xe7\xbb\x91\xe5\xae\x9a\xef\xbc\x89\n *   - \xe5\x8f\xaf\xe9\x80\x89 UnLua \xe8\x84\x9a\xe6\x9c\xac\xe7\xbb\x91\xe5\xae\x9a\xef\xbc\x88\xe8\x87\xaa\xe5\x8a\xa8\xe6\xa3\x80\xe6\xb5\x8b UnLua \xe6\x8f\x92\xe4\xbb\xb6\xe6\x98\xaf\xe5\x90\xa6\xe5\xad\x98\xe5\x9c\xa8\xef\xbc\x89\n *   - \xe5\x8f\xaf\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe6\xa8\xa1\xe6\x9d\xbf\xe7\x9b\xae\xe5\xbd\x95\n *\n * \xe4\xbd\xbf\xe7\x94\xa8\xe6\x96\xb9\xe5\xbc\x8f\xef\xbc\x9a\n *   - \xe6\x8e\xa7\xe5\x88\xb6\xe5\x8f\xb0: WidgetFactory.Generate <\xe6\xa8\xa1\xe6\x9d\xbf\xe5\x90\x8d>\n *   - \xe6\x8e\xa7\xe5\x88\xb6\xe5\x8f\xb0: WidgetFactory.GenerateAll\n *   - \xe8\x8f\x9c\xe5\x8d\x95:   \xe5\xb7\xa5\xe5\x85\xb7 > Widget \xe5\xb7\xa5\xe5\x8e\x82\n *   - Python:  unreal.WidgetFactoryGenerator.generate_from_json(\"\xe6\xa8\xa1\xe6\x9d\xbf\xe5\x90\x8d\")\n */" },
#endif
		{ "IncludePath", "WidgetFactoryGenerator.h" },
		{ "ModuleRelativePath", "WidgetFactoryGenerator.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "\xe4\xbb\x8e JSON \xe6\xa8\xa1\xe6\x9d\xbf\xe8\x87\xaa\xe5\x8a\xa8\xe7\x94\x9f\xe6\x88\x90 UMG Widget Blueprint\xe3\x80\x82\n\n\xe6\x94\xaf\xe6\x8c\x81\xe5\x8a\x9f\xe8\x83\xbd\xef\xbc\x9a\n  - \xe5\xae\x8c\xe6\x95\xb4\xe6\x8e\xa7\xe4\xbb\xb6\xe6\xa0\x91\xe6\x9e\x84\xe5\xbb\xba\xef\xbc\x88\xe7\xb1\xbb\xe5\x9e\x8b\xe3\x80\x81\xe5\xb1\x82\xe7\xba\xa7\xe3\x80\x81\xe5\xb1\x9e\xe6\x80\xa7\xe3\x80\x81Slot\xe3\x80\x81\xe5\x8f\x98\xe9\x87\x8f\xe7\xbb\x91\xe5\xae\x9a\xef\xbc\x89\n  - \xe5\x8f\xaf\xe9\x80\x89 UnLua \xe8\x84\x9a\xe6\x9c\xac\xe7\xbb\x91\xe5\xae\x9a\xef\xbc\x88\xe8\x87\xaa\xe5\x8a\xa8\xe6\xa3\x80\xe6\xb5\x8b UnLua \xe6\x8f\x92\xe4\xbb\xb6\xe6\x98\xaf\xe5\x90\xa6\xe5\xad\x98\xe5\x9c\xa8\xef\xbc\x89\n  - \xe5\x8f\xaf\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89\xe6\xa8\xa1\xe6\x9d\xbf\xe7\x9b\xae\xe5\xbd\x95\n\n\xe4\xbd\xbf\xe7\x94\xa8\xe6\x96\xb9\xe5\xbc\x8f\xef\xbc\x9a\n  - \xe6\x8e\xa7\xe5\x88\xb6\xe5\x8f\xb0: WidgetFactory.Generate <\xe6\xa8\xa1\xe6\x9d\xbf\xe5\x90\x8d>\n  - \xe6\x8e\xa7\xe5\x88\xb6\xe5\x8f\xb0: WidgetFactory.GenerateAll\n  - \xe8\x8f\x9c\xe5\x8d\x95:   \xe5\xb7\xa5\xe5\x85\xb7 > Widget \xe5\xb7\xa5\xe5\x8e\x82\n  - Python:  unreal.WidgetFactoryGenerator.generate_from_json(\"\xe6\xa8\xa1\xe6\x9d\xbf\xe5\x90\x8d\")" },
#endif
	};
#endif // WITH_METADATA

// ********** Begin Class UWidgetFactoryGenerator constinit property declarations ******************
// ********** End Class UWidgetFactoryGenerator constinit property declarations ********************
	static constexpr UE::CodeGen::FClassNativeFunction Funcs[] = {
		{ .NameUTF8 = UTF8TEXT("GenerateAllWidgets"), .Pointer = &UWidgetFactoryGenerator::execGenerateAllWidgets },
		{ .NameUTF8 = UTF8TEXT("GenerateFromJson"), .Pointer = &UWidgetFactoryGenerator::execGenerateFromJson },
	};
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateAllWidgets, "GenerateAllWidgets" }, // 1449002878
		{ &Z_Construct_UFunction_UWidgetFactoryGenerator_GenerateFromJson, "GenerateFromJson" }, // 3760824422
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UWidgetFactoryGenerator>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
}; // struct Z_Construct_UClass_UWidgetFactoryGenerator_Statics
UObject* (*const Z_Construct_UClass_UWidgetFactoryGenerator_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UObject,
	(UObject* (*)())Z_Construct_UPackage__Script_WidgetFactory,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UWidgetFactoryGenerator_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UWidgetFactoryGenerator_Statics::ClassParams = {
	&UWidgetFactoryGenerator::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	nullptr,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	0,
	0,
	0x001000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UWidgetFactoryGenerator_Statics::Class_MetaDataParams), Z_Construct_UClass_UWidgetFactoryGenerator_Statics::Class_MetaDataParams)
};
void UWidgetFactoryGenerator::StaticRegisterNativesUWidgetFactoryGenerator()
{
	UClass* Class = UWidgetFactoryGenerator::StaticClass();
	FNativeFunctionRegistrar::RegisterFunctions(Class, MakeConstArrayView(Z_Construct_UClass_UWidgetFactoryGenerator_Statics::Funcs));
}
UClass* Z_Construct_UClass_UWidgetFactoryGenerator()
{
	if (!Z_Registration_Info_UClass_UWidgetFactoryGenerator.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UWidgetFactoryGenerator.OuterSingleton, Z_Construct_UClass_UWidgetFactoryGenerator_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UWidgetFactoryGenerator.OuterSingleton;
}
UWidgetFactoryGenerator::UWidgetFactoryGenerator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {}
DEFINE_VTABLE_PTR_HELPER_CTOR_NS(, UWidgetFactoryGenerator);
UWidgetFactoryGenerator::~UWidgetFactoryGenerator() {}
// ********** End Class UWidgetFactoryGenerator ****************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h__Script_WidgetFactory_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UWidgetFactoryGenerator, UWidgetFactoryGenerator::StaticClass, TEXT("UWidgetFactoryGenerator"), &Z_Registration_Info_UClass_UWidgetFactoryGenerator, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UWidgetFactoryGenerator), 3256601498U) },
	};
}; // Z_CompiledInDeferFile_FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h__Script_WidgetFactory_Statics 
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h__Script_WidgetFactory_4271794180{
	TEXT("/Script/WidgetFactory"),
	Z_CompiledInDeferFile_FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h__Script_WidgetFactory_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_MS_cooker_Plugins_WidgetFactory_Source_WidgetFactory_WidgetFactoryGenerator_h__Script_WidgetFactory_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0,
};
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
