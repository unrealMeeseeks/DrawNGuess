#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DNGTypes.h"
#include "DNGPromptSettingsSave.generated.h"

UCLASS()
class DRAWNGUESS_API UDNGPromptSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FDNGPromptSettings PromptSettings;
};
