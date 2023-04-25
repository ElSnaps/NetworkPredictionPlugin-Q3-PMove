// Copyright Snaps 2022, All Rights Reserved.

using UnrealBuildTool;

public class MesaCore : ModuleRules
{
	public MesaCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// Auto include for Public / Private, since no split
        PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] {
			
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"EnhancedInput",
			"NetworkPrediction",
			"NetworkPredictionExtras",
			//"FMODStudio"
		});
	}
}
