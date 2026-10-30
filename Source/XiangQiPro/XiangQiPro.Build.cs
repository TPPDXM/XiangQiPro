// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class XiangQiPro : ModuleRules
{
	public XiangQiPro(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "Android", "XQPFullScreen_UPL.xml"));
        }
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput",
            "UMG",
            "Sockets",
            "Networking",
            "Niagara",
            "MoviePlayer",
            "Json",           // 添加 JSON 支持
            "JsonUtilities"   // 添加 JSON 工具支持
             });


        PrivateDependencyModuleNames.AddRange(new string[] {
            "CinematicCamera",
            "Slate", 
            "SlateCore"
        });

        // 启用异常处理(try语句)
        bEnableExceptions = true;

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
