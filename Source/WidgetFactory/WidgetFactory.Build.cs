using UnrealBuildTool;
using System.IO;

public class WidgetFactory : ModuleRules
{
	public WidgetFactory(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"UMG",
			"UMGEditor",
			"UnrealEd",
			"AssetTools",
			"Json",
			"JsonUtilities",
			"Kismet",
			"BlueprintGraph",
			"ToolMenus",
			"InputCore",
			"Projects",
			"ContentBrowser"
		});

		// UnLua optional support: link only if the module exists
		bool bHasUnLua = false;
		// Check all plugin directories for UnLua
		string[] PluginSearchPaths = new string[] {
			Path.Combine(ModuleDirectory, "..", "..", "..", "..", "Plugins"),  // Project/Plugins
			Path.Combine(ModuleDirectory, "..", "..", "..", "..", "_deps"),    // Project/_deps submodules
		};
		foreach (string SearchPath in PluginSearchPaths)
		{
			string UnLuaPath = Path.Combine(SearchPath, "UnLua");
			if (Directory.Exists(UnLuaPath))
			{
				bHasUnLua = true;
				break;
			}
			// Also check nested: _deps/Tencent-UnLua/Plugins/UnLua
			string NestedPath = Path.Combine(SearchPath, "Tencent-UnLua", "Plugins", "UnLua");
			if (Directory.Exists(NestedPath))
			{
				bHasUnLua = true;
				break;
			}
		}

		if (bHasUnLua)
		{
			PrivateDependencyModuleNames.Add("UnLua");
			PublicDefinitions.Add("WITH_UNLUA=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_UNLUA=0");
		}
	}
}
