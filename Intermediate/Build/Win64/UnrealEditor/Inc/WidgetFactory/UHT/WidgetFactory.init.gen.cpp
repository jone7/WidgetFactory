// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
void EmptyLinkFunctionForGeneratedCodeWidgetFactory_init() {}
static_assert(!UE_WITH_CONSTINIT_UOBJECT, "This generated code can only be compiled with !UE_WITH_CONSTINIT_OBJECT");	static FPackageRegistrationInfo Z_Registration_Info_UPackage__Script_WidgetFactory;
	FORCENOINLINE UPackage* Z_Construct_UPackage__Script_WidgetFactory()
	{
		if (!Z_Registration_Info_UPackage__Script_WidgetFactory.OuterSingleton)
		{
		static const UECodeGen_Private::FPackageParams PackageParams = {
			"/Script/WidgetFactory",
			nullptr,
			0,
			PKG_CompiledIn | 0x00000040,
			0x7288F2CB,
			0x42A6888F,
			METADATA_PARAMS(0, nullptr)
		};
		UECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage__Script_WidgetFactory.OuterSingleton, PackageParams);
	}
	return Z_Registration_Info_UPackage__Script_WidgetFactory.OuterSingleton;
}
static FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage__Script_WidgetFactory(Z_Construct_UPackage__Script_WidgetFactory, TEXT("/Script/WidgetFactory"), Z_Registration_Info_UPackage__Script_WidgetFactory, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x7288F2CB, 0x42A6888F));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
