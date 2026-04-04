#pragma once

#include "CoreMinimal.h"
#include "DNGDeepSeekTypes.generated.h"

// Runtime configuration used to call the DeepSeek chat completion API.
USTRUCT(BlueprintType)
struct FDNGDeepSeekConfig
{
	GENERATED_BODY()

	// Raw API key used for the Authorization header.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ApiKey;

	// API base URL. DeepSeek's official default is https://api.deepseek.com.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString BaseUrl = TEXT("https://api.deepseek.com");

	// Chat model name. The local default prefers deepseek-chat for lower latency and more stable in-game HTTP behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Model = TEXT("deepseek-chat");
};

// Structured response returned by the DeepSeek agent.
USTRUCT(BlueprintType)
struct FDNGDeepSeekDrawingPlan
{
	GENERATED_BODY()

	// High-level agent decision: draw, revise, or noop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Action;

	// Short natural-language summary of the intended result.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Summary;

	// Full SVG representation of the intended final drawing state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Svg;

	// Raw JSON content returned by the model message before local parsing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RawModelContent;
};
