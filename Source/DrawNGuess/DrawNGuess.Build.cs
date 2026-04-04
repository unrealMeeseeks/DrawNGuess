// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

// Central build configuration for the DrawNGuess runtime module.
public class DrawNGuess : ModuleRules
{
	public DrawNGuess(ReadOnlyTargetRules Target) : base(Target)
	{
		// Use Unreal's standard shared or explicit PCH handling for this gameplay module.
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		// Public modules needed by gameplay code, UI, JSON parsing, HTTP calls, XML parsing, and CLIP/NNE integration.
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "ImageCore", "NNE", "Json", "JsonUtilities", "HTTP", "XmlParser" });

		// Private UI dependencies used by the fallback C++ widgets.
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
	}
}
