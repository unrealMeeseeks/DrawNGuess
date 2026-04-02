#include "DNGGameInstance.h"

#include "DNGPromptSettingsSave.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"

void UDNGGameInstance::Init()
{
	Super::Init();
	LoadPromptSettings();
}

const FDNGPromptSettings& UDNGGameInstance::GetPromptSettings() const
{
	return PromptSettings;
}

void UDNGGameInstance::UpdatePromptSettings(const FString& InPositivePrefix, const FString& InNegativePrefix, const FString& InJoinAddress)
{
	PromptSettings.PositivePrefix = InPositivePrefix.IsEmpty() ? FDNGPromptSettings().PositivePrefix : InPositivePrefix;
	PromptSettings.NegativePrefix = InNegativePrefix.IsEmpty() ? FDNGPromptSettings().NegativePrefix : InNegativePrefix;
	PromptSettings.LastJoinAddress = InJoinAddress.IsEmpty() ? PromptSettings.LastJoinAddress : InJoinAddress;
	SavePromptSettings();
}

void UDNGGameInstance::HostGame()
{
	SavePromptSettings();

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*ResolveStartupMapName()), true, TEXT("listen"));
	}
}

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

FString UDNGGameInstance::ResolveStartupMapName() const
{
	if (const UWorld* World = GetWorld())
	{
		return UGameplayStatics::GetCurrentLevelName(World, true);
	}

	return TEXT("OpenWorld");
}
