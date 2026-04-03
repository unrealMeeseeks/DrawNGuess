#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DNGMainMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

// Fallback lobby widget implemented entirely in C++.
UCLASS()
class DRAWNGUESS_API UDNGMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;

private:
	// Builds the default widget tree when no Blueprint hierarchy is provided.
	void BuildWidgetTree();

	// Loads saved prompt values and join address into the inputs.
	void LoadSavedValues();

	// Saves the current input values back into the GameInstance.
	void PersistInputs() const;

	UFUNCTION()
	void HandleHostClicked();

	UFUNCTION()
	void HandleJoinClicked();

	UFUNCTION()
	void HandleSaveClicked();

	// Prompt prefix input fields.
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

	// Status line shown under the menu controls.
	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText = nullptr;
};
