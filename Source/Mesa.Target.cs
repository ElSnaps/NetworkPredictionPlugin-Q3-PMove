// Copyright Snaps 2022, All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class MesaTarget : TargetRules
{
	public MesaTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.AddRange( new string[] { "MesaCore" } );
	}
}
