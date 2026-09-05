using UnrealBuildTool;
using System.Collections.Generic;

// Build target for the packaged GAME (what a player runs).
public class SeawallTarget : TargetRules
{
	public SeawallTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Seawall");
	}
}
