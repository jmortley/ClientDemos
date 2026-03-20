namespace UnrealBuildTool.Rules
{
	public class ClientDemos : ModuleRules
	{
		public ClientDemos(TargetInfo Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			PrivateIncludePaths.Add("ClientDemos/Private");
			PublicIncludePaths.Add("ClientDemos/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
				}
			);
		}
	}
}
