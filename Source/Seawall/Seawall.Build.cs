using UnrealBuildTool;

// Declares what engine modules our C++ can #include and link against.
// Adding a module here is what makes its headers available.
public class Seawall : ModuleRules
{
	public Seawall(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",

			// Modern input system -- action/context based rather than raw key binds.
			"EnhancedInput",

			// Gameplay Ability System. Pulled in from day one because it is
			// replication-native, which is what keeps co-op cheap to add later.
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",

			// AI: StateTree is what Epic is moving to over Behavior Trees.
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",

			// Navigation for creature pathing.
			"NavigationSystem",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
		});
	}
}
