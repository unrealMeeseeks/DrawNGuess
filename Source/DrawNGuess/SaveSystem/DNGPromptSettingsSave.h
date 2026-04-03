#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "../Core/DNGTypes.h"
#include "DNGPromptSettingsSave.generated.h"

// SaveGame object used to persist the menu prompt settings.
UCLASS()
class DRAWNGUESS_API UDNGPromptSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	// Serialized prompt settings payload.
	UPROPERTY()
	FDNGPromptSettings PromptSettings;
};
