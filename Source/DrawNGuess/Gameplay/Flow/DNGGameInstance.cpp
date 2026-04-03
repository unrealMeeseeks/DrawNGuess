#include "DNGGameInstance.h"

#include "../../SaveSystem/DNGPromptSettingsSave.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

// Loads locally persisted prompt settings as soon as the game instance is created.
void UDNGGameInstance::Init()
{
	Super::Init();
	LoadPromptSettings();
}

// Returns the cached local prompt settings without touching disk.
const FDNGPromptSettings& UDNGGameInstance::GetPromptSettings() const
{
	return PromptSettings;
}

// Updates the in-memory prompt settings and persists them immediately.
void UDNGGameInstance::UpdatePromptSettings(const FString& InPositivePrefix, const FString& InNegativePrefix, const FString& InJoinAddress)
{
	PromptSettings.PositivePrefix = InPositivePrefix.IsEmpty() ? FDNGPromptSettings().PositivePrefix : InPositivePrefix;
	PromptSettings.NegativePrefix = InNegativePrefix.IsEmpty() ? FDNGPromptSettings().NegativePrefix : InNegativePrefix;
	PromptSettings.LastJoinAddress = InJoinAddress.IsEmpty() ? PromptSettings.LastJoinAddress : InJoinAddress;
	SavePromptSettings();
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

// Uses the current level name so hosting works from whichever gameplay map is loaded.
FString UDNGGameInstance::ResolveStartupMapName() const
{
	if (const UWorld* World = GetWorld())
	{
		return UGameplayStatics::GetCurrentLevelName(World, true);
	}

	return TEXT("OpenWorld");
}
