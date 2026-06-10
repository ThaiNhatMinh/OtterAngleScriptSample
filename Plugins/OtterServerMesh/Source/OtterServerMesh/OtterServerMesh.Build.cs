// Copyright OtterAngleScript. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OtterServerMesh : ModuleRules
{
	public OtterServerMesh(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"NetCore",
			"ReplicationGraph"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"Sockets",
			"Networking"
		});

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.Add("CQTest");
		}

		// Add AngelScript SDK includes for the script bindings
		string AngleScriptSdkPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../OtterAngleScript/Source/ThirdParty/sdk/angelscript"));
		if (System.IO.Directory.Exists(AngleScriptSdkPath))
		{
			PublicIncludePaths.Add(Path.Combine(AngleScriptSdkPath, "include"));
			PublicIncludePaths.Add(Path.Combine(AngleScriptSdkPath, "source"));
		}

		string AsbindSdkPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../../OtterAngleScript/Source/ThirdParty/asbind20/include"));
		if (System.IO.Directory.Exists(AsbindSdkPath))
		{
			PublicIncludePaths.Add(AsbindSdkPath);
		}
	}
}
