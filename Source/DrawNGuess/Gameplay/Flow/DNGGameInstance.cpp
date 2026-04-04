#include "DNGGameInstance.h"

#include "../../SaveSystem/DNGPromptSettingsSave.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

// Loads locally persisted prompt settings as soon as the game instance is created.
void UDNGGameInstance::Init()
{
	Super::Init();
	LoadPromptSettings();
	LoadDeepSeekConfig();
}

// Returns the cached local prompt settings without touching disk.
const FDNGPromptSettings& UDNGGameInstance::GetPromptSettings() const
{
	return PromptSettings;
}

// Returns the currently cached DeepSeek configuration.
const FDNGDeepSeekConfig& UDNGGameInstance::GetDeepSeekConfig() const
{
	return DeepSeekConfig;
}

// Updates the in-memory prompt settings and persists them immediately.
void UDNGGameInstance::UpdatePromptSettings(const FString& InPositivePrefix, const FString& InNegativePrefix, const FString& InJoinAddress)
{
	PromptSettings.PositivePrefix = InPositivePrefix.IsEmpty() ? FDNGPromptSettings().PositivePrefix : InPositivePrefix;
	PromptSettings.NegativePrefix = InNegativePrefix.IsEmpty() ? FDNGPromptSettings().NegativePrefix : InNegativePrefix;
	PromptSettings.LastJoinAddress = InJoinAddress.IsEmpty() ? PromptSettings.LastJoinAddress : InJoinAddress;
	SavePromptSettings();
}

// Updates the in-memory DeepSeek settings and optionally persists them to JSON.
void UDNGGameInstance::UpdateDeepSeekConfig(const FString& InApiKey, const FString& InBaseUrl, const FString& InModel, bool bSaveToFile)
{
	DeepSeekConfig.ApiKey = InApiKey;
	DeepSeekConfig.BaseUrl = InBaseUrl.IsEmpty() ? TEXT("https://api.deepseek.com") : InBaseUrl;
	DeepSeekConfig.Model = InModel.IsEmpty() ? TEXT("deepseek-chat") : InModel;

	if (bSaveToFile)
	{
		SaveDeepSeekConfig();
	}
}

// Returns whether a usable API key is currently available.
bool UDNGGameInstance::HasDeepSeekApiKey() const
{
	return !DeepSeekConfig.ApiKey.TrimStartAndEnd().IsEmpty();
}

// Returns the writable DeepSeek config file path under Saved/Config.
FString UDNGGameInstance::GetDeepSeekConfigPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("DeepSeekAgent.json"));
}

// Starts a listen server on the currently active map.
void UDNGGameInstance::HostGame()
{
	SavePromptSettings();

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*ResolveStartupMapName()), true, TEXT("listen"));
	}
}

// Connects the local player to a remote host and remembers the entered address.
void UDNGGameInstance::JoinGame(const FString& ServerAddress)
{
	const FString TargetAddress = ServerAddress.IsEmpty() ? PromptSettings.LastJoinAddress : ServerAddress;
	PromptSettings.LastJoinAddress = TargetAddress;
	SavePromptSettings();

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			PlayerController->ClientTravel(TargetAddress, TRAVEL_Absolute);
		}
	}
}

// Restores prompt settings from the dedicated SaveGame slot if it exists.
void UDNGGameInstance::LoadPromptSettings()
{
	if (UGameplayStatics::DoesSaveGameExist(SaveSlotName, 0))
	{
		if (UDNGPromptSettingsSave* Save = Cast<UDNGPromptSettingsSave>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0)))
		{
			PromptSettings = Save->PromptSettings;
			return;
		}
	}

	PromptSettings = FDNGPromptSettings();
}

// Writes the current prompt settings to the SaveGame slot.
void UDNGGameInstance::SavePromptSettings() const
{
	UDNGPromptSettingsSave* Save = Cast<UDNGPromptSettingsSave>(UGameplayStatics::CreateSaveGameObject(UDNGPromptSettingsSave::StaticClass()));
	if (!Save)
	{
		return;
	}

	Save->PromptSettings = PromptSettings;
	UGameplayStatics::SaveGameToSlot(Save, SaveSlotName, 0);
}

// Loads DeepSeek settings from Saved/Config first, then falls back to Config if needed.
void UDNGGameInstance::LoadDeepSeekConfig()
{
	DeepSeekConfig = FDNGDeepSeekConfig();

	TArray<FString> CandidatePaths;
	CandidatePaths.Add(GetDeepSeekConfigPath());
	CandidatePaths.Add(FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DeepSeekAgent.json")));

	for (const FString& CandidatePath : CandidatePaths)
	{
		if (!FPaths::FileExists(CandidatePath))
		{
			continue;
		}

		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *CandidatePath))
		{
			continue;
		}

		TSharedPtr<FJsonObject> RootObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			continue;
		}

		RootObject->TryGetStringField(TEXT("api_key"), DeepSeekConfig.ApiKey);
		RootObject->TryGetStringField(TEXT("base_url"), DeepSeekConfig.BaseUrl);
		RootObject->TryGetStringField(TEXT("model"), DeepSeekConfig.Model);

		if (DeepSeekConfig.BaseUrl.TrimStartAndEnd().IsEmpty())
		{
			DeepSeekConfig.BaseUrl = TEXT("https://api.deepseek.com");
		}

		if (DeepSeekConfig.Model.TrimStartAndEnd().IsEmpty())
		{
			DeepSeekConfig.Model = TEXT("deepseek-chat");
		}

		return;
	}
}

// Persists DeepSeek settings to the writable Saved/Config location without validating them.
void UDNGGameInstance::SaveDeepSeekConfig() const
{
	const FString ConfigPath = GetDeepSeekConfigPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ConfigPath), true);

	const TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();
	RootObject->SetStringField(TEXT("api_key"), DeepSeekConfig.ApiKey);
	RootObject->SetStringField(TEXT("base_url"), DeepSeekConfig.BaseUrl.IsEmpty() ? TEXT("https://api.deepseek.com") : DeepSeekConfig.BaseUrl);
	RootObject->SetStringField(TEXT("model"), DeepSeekConfig.Model.IsEmpty() ? TEXT("deepseek-chat") : DeepSeekConfig.Model);

	FString JsonString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(RootObject, Writer);
	FFileHelper::SaveStringToFile(JsonString, *ConfigPath);
}

// Uses the current level name so hosting works from whichever gameplay map is loaded.
FString UDNGGameInstance::ResolveStartupMapName() const
{
	if (const UWorld* World = GetWorld())
	{
		return UGameplayStatics::GetCurrentLevelName(World, true);
	}

	return TEXT("OpenWorld");
}
