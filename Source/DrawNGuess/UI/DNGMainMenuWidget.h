#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DNGMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class DRAWNGUESS_API UDNGMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

private:
	void BuildWidgetTree();
	void LoadSavedValues();
	void PersistInputs() const;

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UPROPERTY()
	TObjectPtr<UEditableTextBox> PositivePrefixInput = nullptr;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> NegativePrefixInput = nullptr;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> JoinAddressInput = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> HostButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> JoinButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> SaveButton = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText = nullptr;
};
