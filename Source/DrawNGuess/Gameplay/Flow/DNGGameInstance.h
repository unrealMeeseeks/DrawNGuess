#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "../../Core/DNGTypes.h"
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

	// Updates the prompt settings and persists them locally.
	UFUNCTION(BlueprintCallable)
	void UpdatePromptSettings(const FString& InPositivePrefix, const FString& InNegativePrefix, const FString& InJoinAddress);

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

private:
	// Fixed SaveGame slot name used for prompt settings.
	static constexpr const TCHAR* SaveSlotName = TEXT("PromptSettings");

	// Local-only settings copied into the UI when the game starts.
	UPROPERTY()
	FDNGPromptSettings PromptSettings;

	// Resolves the map opened by HostGame().
	FString ResolveStartupMapName() const;
};
