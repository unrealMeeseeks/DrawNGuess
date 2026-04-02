#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "DNGTypes.h"
#include "DNGGameInstance.generated.h"

UCLASS()
class DRAWNGUESS_API UDNGGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	const FDNGPromptSettings& GetPromptSettings() const;

	UFUNCTION(BlueprintCallable)
	void UpdatePromptSettings(const FString& InPositivePrefix, const FString& InNegativePrefix, const FString& InJoinAddress);

	UFUNCTION(BlueprintCallable)
	void HostGame();

	UFUNCTION(BlueprintCallable)
	void JoinGame(const FString& ServerAddress);

	void LoadPromptSettings();
	void SavePromptSettings() const;

private:
	static constexpr const TCHAR* SaveSlotName = TEXT("PromptSettings");

	UPROPERTY()
	FDNGPromptSettings PromptSettings;

	FString ResolveStartupMapName() const;
};
