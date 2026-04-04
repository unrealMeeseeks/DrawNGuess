#include "DNGDeepSeekAgentService.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	static FString RedactAuthorizationHeader(const FString& HeaderLine)
	{
		FString Left;
		FString Right;
		if (HeaderLine.Split(TEXT(":"), &Left, &Right) && Left.TrimStartAndEnd().Equals(TEXT("Authorization"), ESearchCase::IgnoreCase))
		{
			return TEXT("Authorization: Bearer [REDACTED]");
		}

		return HeaderLine;
	}

	static FString SummarizePayloadForLog(const FString& Payload)
	{
		constexpr int32 MaxLoggedChars = 4000;
		if (Payload.Len() <= MaxLoggedChars)
		{
			return Payload;
		}

		return Payload.Left(MaxLoggedChars) + TEXT(" ...(truncated)");
	}
}

void UDNGDeepSeekAgentService::Configure(const FDNGDeepSeekConfig& InConfig)
{
	Config = InConfig;

	if (Config.BaseUrl.TrimStartAndEnd().IsEmpty())
	{
		Config.BaseUrl = TEXT("https://api.deepseek.com");
	}

	if (Config.Model.TrimStartAndEnd().IsEmpty())
	{
		Config.Model = TEXT("deepseek-chat");
	}
}

void UDNGDeepSeekAgentService::RequestDrawingPlan(const FString& UserInstruction, const FString& CurrentSvg, FOnDeepSeekPlanCompleted Completion)
{
	if (!HasApiKey())
	{
		Completion.ExecuteIfBound(false, FDNGDeepSeekDrawingPlan(), TEXT("DeepSeek API key is missing."));
		return;
	}

	TWeakObjectPtr<UDNGDeepSeekAgentService> WeakThis(this);
	const FString RequestedModel = Config.Model;

	SendChatCompletionRequest(
		TEXT("artist"),
		RequestedModel,
		BuildArtistSystemPrompt(),
		BuildArtistUserPrompt(UserInstruction, CurrentSvg),
		ShouldEnableThinkingForModel(RequestedModel),
		true,
		[WeakThis, Completion](bool bSuccess, const FString& ResponseBody, const FString& ErrorMessage)
		{
			if (!WeakThis.IsValid())
			{
				Completion.ExecuteIfBound(false, FDNGDeepSeekDrawingPlan(), TEXT("DeepSeek agent service was destroyed before the artist stage completed."));
				return;
			}

			if (!bSuccess)
			{
				Completion.ExecuteIfBound(false, FDNGDeepSeekDrawingPlan(), ErrorMessage);
				return;
			}

			FString ContentJson;
			FString ExtractError;
			if (!WeakThis->ExtractMessageContent(ResponseBody, ContentJson, ExtractError))
			{
				Completion.ExecuteIfBound(false, FDNGDeepSeekDrawingPlan(), ExtractError);
				return;
			}

			FDNGDeepSeekDrawingPlan FinalPlan;
			FString ParseError;
			if (!WeakThis->ParseArtistContent(ContentJson, FinalPlan, ParseError))
			{
				Completion.ExecuteIfBound(false, FDNGDeepSeekDrawingPlan(), ParseError);
				return;
			}

			Completion.ExecuteIfBound(true, FinalPlan, FString());
		});
}

void UDNGDeepSeekAgentService::SendChatCompletionRequest(
	const FString& StageName,
	const FString& ModelName,
	const FString& SystemPrompt,
	const FString& UserPrompt,
	bool bEnableThinking,
	bool bAllowFallback,
	TFunction<void(bool, const FString&, const FString&)> Completion)
{
	const FString RequestBody = BuildRequestBody(StageName, ModelName, SystemPrompt, UserPrompt, bEnableThinking);

	TWeakObjectPtr<UDNGDeepSeekAgentService> WeakThis(this);
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(BuildEndpointUrl());
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Config.ApiKey));
	Request->SetContentAsString(RequestBody);
	Request->SetTimeout(180.0f);
	Request->SetActivityTimeout(180.0f);

	TArray<FString> RedactedHeaders;
	for (const FString& HeaderLine : Request->GetAllHeaders())
	{
		RedactedHeaders.Add(RedactAuthorizationHeader(HeaderLine));
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("DeepSeek request starting. Stage=%s URL=%s Model=%s Thinking=%s Timeout=%.1fs ActivityTimeout=%.1fs ContentLength=%llu Headers=[%s] Body=%s"),
		*StageName,
		*Request->GetURL(),
		*ModelName,
		bEnableThinking ? TEXT("true") : TEXT("false"),
		180.0f,
		180.0f,
		Request->GetContentLength(),
		*FString::Join(RedactedHeaders, TEXT(" | ")),
		*SummarizePayloadForLog(RequestBody));

	Request->OnStatusCodeReceived().BindLambda(
		[StageName, ModelName](FHttpRequestPtr HttpRequest, int32 StatusCode)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("DeepSeek status code received. Stage=%s URL=%s Model=%s StatusCode=%d"),
				*StageName,
				HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("(unknown)"),
				*ModelName,
				StatusCode);
		});

	Request->OnHeaderReceived().BindLambda(
		[StageName, ModelName](FHttpRequestPtr HttpRequest, const FString& HeaderName, const FString& HeaderValue)
		{
			UE_LOG(
				LogTemp,
				Log,
				TEXT("DeepSeek response header. Stage=%s URL=%s Model=%s %s=%s"),
				*StageName,
				HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("(unknown)"),
				*ModelName,
				*HeaderName,
				*HeaderValue);
		});

	TSharedRef<bool, ESPMode::ThreadSafe> bLoggedFirstProgress = MakeShared<bool, ESPMode::ThreadSafe>(false);
	Request->OnRequestProgress64().BindLambda(
		[StageName, ModelName, bLoggedFirstProgress](FHttpRequestPtr HttpRequest, uint64 BytesSent, uint64 BytesReceived)
		{
			if (!*bLoggedFirstProgress || BytesReceived > 0)
			{
				*bLoggedFirstProgress = true;
				UE_LOG(
					LogTemp,
					Log,
					TEXT("DeepSeek request progress. Stage=%s URL=%s Model=%s BytesSent=%llu BytesReceived=%llu"),
					*StageName,
					HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("(unknown)"),
					*ModelName,
					BytesSent,
					BytesReceived);
			}
		});

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, StageName, ModelName, SystemPrompt, UserPrompt, bEnableThinking, bAllowFallback, Completion = MoveTemp(Completion)](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded) mutable
		{
			if (!WeakThis.IsValid())
			{
				Completion(false, FString(), TEXT("DeepSeek agent service was destroyed before the request completed."));
				return;
			}

			if (!bSucceeded || !HttpResponse.IsValid())
			{
				const int32 HttpStatus = HttpResponse.IsValid() ? HttpResponse->GetResponseCode() : 0;
				const int32 RequestStatus = HttpRequest.IsValid() ? static_cast<int32>(HttpRequest->GetStatus()) : INDEX_NONE;
				const EHttpFailureReason FailureReason = HttpRequest.IsValid() ? HttpRequest->GetFailureReason() : EHttpFailureReason::Other;
				const float ElapsedSeconds = HttpRequest.IsValid() ? HttpRequest->GetElapsedTime() : 0.0f;
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("DeepSeek request failed before a valid HTTP response. Stage=%s URL=%s Model=%s RequestStatus=%d(%s) HttpStatus=%d FailureReason=%s Elapsed=%.2fs"),
					*StageName,
					HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("(unknown)"),
					*ModelName,
					RequestStatus,
					HttpRequest.IsValid() ? EHttpRequestStatus::ToString(HttpRequest->GetStatus()) : TEXT("Unknown"),
					HttpStatus,
					LexToString(FailureReason),
					ElapsedSeconds);

				if (bAllowFallback && bEnableThinking)
				{
					UE_LOG(LogTemp, Warning, TEXT("Retrying DeepSeek request without thinking mode. Stage=%s Model=%s"), *StageName, *ModelName);
					WeakThis->SendChatCompletionRequest(StageName, ModelName, SystemPrompt, UserPrompt, false, false, MoveTemp(Completion));
					return;
				}

				const FString FallbackModel = WeakThis->GetFallbackModelName(ModelName);
				if (bAllowFallback && !FallbackModel.IsEmpty())
				{
					UE_LOG(LogTemp, Warning, TEXT("Retrying DeepSeek request with fallback model %s. Stage=%s"), *FallbackModel, *StageName);
					WeakThis->SendChatCompletionRequest(StageName, FallbackModel, SystemPrompt, UserPrompt, WeakThis->ShouldEnableThinkingForModel(FallbackModel), false, MoveTemp(Completion));
					return;
				}

				Completion(false, FString(), TEXT("DeepSeek request failed before a valid HTTP response was received."));
				return;
			}

			if (!EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("DeepSeek HTTP error response. Stage=%s URL=%s Model=%s Status=%d Headers=[%s] Body=%s"),
					*StageName,
					HttpRequest.IsValid() ? *HttpRequest->GetURL() : TEXT("(unknown)"),
					*ModelName,
					HttpResponse->GetResponseCode(),
					*FString::Join(HttpResponse->GetAllHeaders(), TEXT(" | ")),
					*SummarizePayloadForLog(HttpResponse->GetContentAsString()));
				Completion(false, FString(), FString::Printf(TEXT("DeepSeek request failed with HTTP %d: %s"), HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString()));
				return;
			}

			Completion(true, HttpResponse->GetContentAsString(), FString());
		});

	Request->ProcessRequest();
}

FString UDNGDeepSeekAgentService::BuildRequestBody(const FString& StageName, const FString& ModelName, const FString& SystemPrompt, const FString& UserPrompt, bool bEnableThinking) const
{
	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	const FString NormalizedModelName = ModelName.TrimStartAndEnd().ToLower();
	RootObject->SetStringField(TEXT("model"), ModelName);
	RootObject->SetBoolField(TEXT("stream"), false);
	RootObject->SetNumberField(TEXT("max_tokens"), 4096);

	if (bEnableThinking)
	{
		const TSharedRef<FJsonObject> ThinkingObject = MakeShared<FJsonObject>();
		ThinkingObject->SetStringField(TEXT("type"), TEXT("enabled"));
		RootObject->SetObjectField(TEXT("thinking"), ThinkingObject);
	}
	else if (NormalizedModelName == TEXT("deepseek-reasoner"))
	{
		// Keep reasoner calls close to the plain API usage that already produced good SVGs outside UE.
	}
	else
	{
		if (StageName == TEXT("artist"))
		{
			// The artist stage should stay literal and coherent instead of drifting into abstract geometry.
			RootObject->SetNumberField(TEXT("temperature"), 0.35);
		}
		else
		{
			RootObject->SetNumberField(TEXT("temperature"), 0.6);
			RootObject->SetNumberField(TEXT("presence_penalty"), 0.1);
		}
	}

	const TSharedRef<FJsonObject> ResponseFormatObject = MakeShared<FJsonObject>();
	ResponseFormatObject->SetStringField(TEXT("type"), TEXT("json_object"));
	RootObject->SetObjectField(TEXT("response_format"), ResponseFormatObject);

	TArray<TSharedPtr<FJsonValue>> Messages;
	{
		const TSharedRef<FJsonObject> SystemMessage = MakeShared<FJsonObject>();
		SystemMessage->SetStringField(TEXT("role"), TEXT("system"));
		SystemMessage->SetStringField(TEXT("content"), SystemPrompt);
		Messages.Add(MakeShared<FJsonValueObject>(SystemMessage));
	}

	{
		const TSharedRef<FJsonObject> UserMessage = MakeShared<FJsonObject>();
		UserMessage->SetStringField(TEXT("role"), TEXT("user"));
		UserMessage->SetStringField(TEXT("content"), UserPrompt);
		Messages.Add(MakeShared<FJsonValueObject>(UserMessage));
	}

	RootObject->SetArrayField(TEXT("messages"), Messages);

	FString RequestBody;
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&RequestBody);
		FJsonSerializer::Serialize(RootObject, Writer);
	}

	return RequestBody;
}

FString UDNGDeepSeekAgentService::BuildArtistSystemPrompt() const
{
	return TEXT(
		"You are an SVG artist. "
		"You must always answer in json. "
		"Return a valid JSON object with this exact shape: "
		"{"
		"\"action\":\"draw|revise|noop\","
		"\"summary\":\"short description\","
		"\"svg\":\"full standalone svg string\""
		"} "
		"Rules: "
		"1. For drawing or revision requests, create a complete final-state standalone SVG for the whole image. "
		"2. The result must depict the requested subject literally and recognizably, not symbolically or abstractly, unless the user explicitly asks for abstraction. "
		"3. Favor the same quality bar an SVG illustrator would aim for: strong silhouette, expressive curves, interior detail, and a composition that fills the canvas well. "
		"4. For animals, people, and objects, include the parts humans use to recognize them immediately. A cat should visibly read as a cat, with features such as ears, eyes, nose, whiskers, body, paws, and tail. "
		"5. Avoid lazy substitutes such as concentric circles, nested ovals, generic blobs, target-like layouts, or unrelated geometry standing in for the subject. "
		"6. Use clean, standard SVG. Prefer path, circle, ellipse, line, polyline, polygon, and rect. Avoid scripts, filters, masks, clip-paths, text, images, and transforms. "
		"7. Use a clean square canvas such as <svg viewBox=\"0 0 512 512\" xmlns=\"http://www.w3.org/2000/svg\">. White background, black outlines, and tasteful accent colors are good defaults. "
		"8. If the request is a revision, preserve the strong parts of the current SVG and modify only what the user asked for. "
		"9. If the instruction is unrelated to drawing or revision, set action to noop, set svg to an empty string, and explain why in summary. "
		"10. Do not output markdown fences or any text outside the json object.");
}

FString UDNGDeepSeekAgentService::BuildArtistUserPrompt(const FString& UserInstruction, const FString& CurrentSvg) const
{
	const FString EffectiveSvg = CurrentSvg.TrimStartAndEnd().IsEmpty() ? TEXT("(none)") : CurrentSvg;
	return FString::Printf(
		TEXT("SVG artist request.\nCurrent final svg:\n%s\n\nUser instruction:\n%s\n\nInfer whether this is a fresh drawing request, a revision request, or a non-drawing request. For draw or revise, return a complete final standalone SVG that directly satisfies the instruction. Prioritize an appealing, recognizable result similar to what a strong SVG illustrator would make. Return only the required json object."),
		*EffectiveSvg,
		*UserInstruction);
}

FString UDNGDeepSeekAgentService::BuildEndpointUrl() const
{
	FString Base = Config.BaseUrl.TrimStartAndEnd();
	while (Base.EndsWith(TEXT("/")))
	{
		Base.LeftChopInline(1, EAllowShrinking::No);
	}

	return Base + TEXT("/chat/completions");
}

bool UDNGDeepSeekAgentService::ExtractMessageContent(const FString& ResponseBody, FString& OutContentJson, FString& OutError) const
{
	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = TEXT("DeepSeek response was not valid JSON.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("choices"), Choices) || !Choices || Choices->Num() == 0)
	{
		OutError = TEXT("DeepSeek response did not contain any choices.");
		return false;
	}

	const TSharedPtr<FJsonObject> ChoiceObject = (*Choices)[0].IsValid() ? (*Choices)[0]->AsObject() : nullptr;
	if (!ChoiceObject.IsValid())
	{
		OutError = TEXT("DeepSeek response choice payload was malformed.");
		return false;
	}

	const TSharedPtr<FJsonObject>* MessageObject = nullptr;
	if (!ChoiceObject->TryGetObjectField(TEXT("message"), MessageObject) || !MessageObject || !MessageObject->IsValid())
	{
		OutError = TEXT("DeepSeek response did not contain a valid message object.");
		return false;
	}

	if (!(*MessageObject)->TryGetStringField(TEXT("content"), OutContentJson) || OutContentJson.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("DeepSeek response message content was empty.");
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("DeepSeek model content: %s"), *OutContentJson);

	FString ReasoningContent;
	if ((*MessageObject)->TryGetStringField(TEXT("reasoning_content"), ReasoningContent) && !ReasoningContent.TrimStartAndEnd().IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("DeepSeek reasoning content: %s"), *ReasoningContent);
	}

	return true;
}

bool UDNGDeepSeekAgentService::ParseArtistContent(const FString& ContentJson, FDNGDeepSeekDrawingPlan& OutPlan, FString& OutError) const
{
	TSharedPtr<FJsonObject> PlanObject;
	const TSharedRef<TJsonReader<>> PlanReader = TJsonReaderFactory<>::Create(ContentJson);
	if (!FJsonSerializer::Deserialize(PlanReader, PlanObject) || !PlanObject.IsValid())
	{
		OutError = FString::Printf(TEXT("DeepSeek returned non-artist JSON content: %s"), *ContentJson);
		return false;
	}

	OutPlan.RawModelContent = ContentJson;
	PlanObject->TryGetStringField(TEXT("action"), OutPlan.Action);
	PlanObject->TryGetStringField(TEXT("summary"), OutPlan.Summary);
	PlanObject->TryGetStringField(TEXT("svg"), OutPlan.Svg);

	if (OutPlan.Action.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("DeepSeek artist stage did not contain a valid action field.");
		return false;
	}

	const FString NormalizedAction = OutPlan.Action.TrimStartAndEnd().ToLower();
	if (NormalizedAction != TEXT("noop") && OutPlan.Svg.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("DeepSeek artist stage did not contain a usable svg payload.");
		return false;
	}

	return true;
}

bool UDNGDeepSeekAgentService::ShouldEnableThinkingForModel(const FString& ModelName) const
{
	const FString NormalizedModel = ModelName.TrimStartAndEnd().ToLower();
	return NormalizedModel == TEXT("deepseek-chat");
}

FString UDNGDeepSeekAgentService::GetFallbackModelName(const FString& ModelName) const
{
	const FString NormalizedModel = ModelName.TrimStartAndEnd().ToLower();
	if (NormalizedModel == TEXT("deepseek-reasoner"))
	{
		return TEXT("deepseek-chat");
	}

	return FString();
}
