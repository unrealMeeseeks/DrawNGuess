#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "../../Core/DNGTypes.h"
#include "../../AI/DeepSeek/DNGDeepSeekTypes.h"
#include "DNGGameInstance.generated.h"

// Stores local menu settings and exposes the listen-server host/join flow.
UCLASS()
class DRAWNGUESS_API UDNGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	// Returns the settings currently cached in memory.
	const FDNGPromptSettings& GetPromptSettings() const;

	// Returns the currently resolved DeepSeek configuration.
	const FDNGDeepSeekConfig& GetDeepSeekConfig() const;

	// Updates the prompt settings and persists them locally.
	UFUNCTION(BlueprintCallable)
	void UpdatePromptSettings(const FString& InPositivePrefix, const FString& InNegativePrefix, const FString& InJoinAddress);

	// Updates the DeepSeek configuration and optionally persists it to the local JSON file.
	UFUNCTION(BlueprintCallable)
	void UpdateDeepSeekConfig(const FString& InApiKey, const FString& InBaseUrl, const FString& InModel, bool bSaveToFile = true);

	// Returns whether a non-empty DeepSeek API key is currently available.
	UFUNCTION(BlueprintCallable)
	bool HasDeepSeekApiKey() const;

	// Returns the resolved JSON config path used for save/load.
	UFUNCTION(BlueprintCallable)
	FString GetDeepSeekConfigPath() const;

	// Opens the startup map as a listen server.
	UFUNCTION(BlueprintCallable)
	void HostGame();

	// Connects to a remote listen server using a direct IP address.
	UFUNCTION(BlueprintCallable)
	void JoinGame(const FString& ServerAddress);

	// Loads settings from the SaveGame slot.
	void LoadPromptSettings();

	// Writes settings back to the SaveGame slot.
	void SavePromptSettings() const;

	// Loads DeepSeek configuration from JSON file if one exists.
	void LoadDeepSeekConfig();

	// Saves DeepSeek configuration to the writable JSON file.
	void SaveDeepSeekConfig() const;

private:
	// Fixed SaveGame slot name used for prompt settings.
	static constexpr const TCHAR* SaveSlotName = TEXT("PromptSettings");

	// Local-only settings copied into the UI when the game starts.
	UPROPERTY()
	FDNGPromptSettings PromptSettings;

	// Local-only DeepSeek runtime settings loaded from JSON or manual UI entry.
	UPROPERTY()
	FDNGDeepSeekConfig DeepSeekConfig;

	// Resolves the map opened by HostGame().
	FString ResolveStartupMapName() const;
};
