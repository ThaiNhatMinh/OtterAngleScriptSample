// Copyright OtterAngleScript. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class AngelScriptSampleServerTarget : TargetRules
{
	public AngelScriptSampleServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		bUseLoggingInShipping = true;

		ExtraModuleNames.AddRange( new string[] { "AngelScriptSample" } );
	}
}
