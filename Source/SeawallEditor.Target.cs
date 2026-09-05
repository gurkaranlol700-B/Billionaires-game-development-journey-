using UnrealBuildTool;
using System.Collections.Generic;

// Build target for the EDITOR (what we work in). This is the one you build
// while developing; the Game target above is only needed when packaging.
public class SeawallEditorTarget : TargetRules
{
	public SeawallEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Seawall");
	}
}
