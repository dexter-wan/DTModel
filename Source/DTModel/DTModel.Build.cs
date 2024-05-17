// Copyright 2023 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

using UnrealBuildTool;

public class DTModel : ModuleRules
{
	public DTModel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "GeometryCore", "ProceduralMeshComponent", "MeshConversion", "MeshDescription", "StaticMeshDescription", "GeometryFramework", "GeometryScriptingCore" });
	}
}
