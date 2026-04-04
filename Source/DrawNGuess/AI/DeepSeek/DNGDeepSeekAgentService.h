#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DNGDeepSeekTypes.h"
#include "DNGDeepSeekAgentService.generated.h"

// Thin HTTP wrapper around DeepSeek's OpenAI-compatible chat completion API.
UCLASS()
class DRAWNGUESS_API UDNGDeepSeekAgentService : public UObject
{
	GENERATED_BODY()

public:
	// Callback used when an asynchronous DeepSeek drawing request completes.
	DECLARE_DELEGATE_ThreeParams(FOnDeepSeekPlanCompleted, bool, const FDNGDeepSeekDrawingPlan&, const FString&);

	// Applies the latest local runtime configuration before a request is sent.
	void Configure(const FDNGDeepSeekConfig& InConfig);

	// Returns whether a non-empty API key is currently configured.
	bool HasApiKey() const { return !Config.ApiKey.TrimStartAndEnd().IsEmpty(); }

	// Requests an SVG-first drawing response from DeepSeek using the current SVG as context.
	void RequestDrawingPlan(const FString& UserInstruction, const FString& CurrentSvg, FOnDeepSeekPlanCompleted Completion);

private:
	// Sends one raw chat-completions request and returns the raw response body or an error string.
	void SendChatCompletionRequest(
		const FString& StageName,
		const FString& ModelName,
		const FString& SystemPrompt,
		const FString& UserPrompt,
		bool bEnableThinking,
		bool bAllowFallback,
		TFunction<void(bool, const FString&, const FString&)> Completion);

	// Builds the OpenAI-compatible request payload for one model attempt.
	FString BuildRequestBody(const FString& StageName, const FString& ModelName, const FString& SystemPrompt, const FString& UserPrompt, bool bEnableThinking) const;

	// Returns the system prompt used for the single-stage SVG artist request.
	FString BuildArtistSystemPrompt() const;

	// Returns the user prompt for the single-stage SVG artist request.
	FString BuildArtistUserPrompt(const FString& UserInstruction, const FString& CurrentSvg) const;

	// Builds the final /chat/completions endpoint from the configured base URL.
	FString BuildEndpointUrl() const;

	// Extracts the message content JSON string from an OpenAI-compatible response.
	bool ExtractMessageContent(const FString& ResponseBody, FString& OutContentJson, FString& OutError) const;

	// Parses the single-stage SVG artist JSON object.
	bool ParseArtistContent(const FString& ContentJson, FDNGDeepSeekDrawingPlan& OutPlan, FString& OutError) const;

	// Returns whether the given model should run in DeepSeek thinking mode.
	bool ShouldEnableThinkingForModel(const FString& ModelName) const;

	// Returns the fallback model name used when the preferred model fails before a valid response arrives.
	FString GetFallbackModelName(const FString& ModelName) const;

	// Current runtime configuration.
	FDNGDeepSeekConfig Config;
};
