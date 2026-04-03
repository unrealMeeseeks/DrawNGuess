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

	// Binds button callbacks for either Blueprint-owned or fallback-created widgets.
	void BindActions();

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
	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> PositivePrefixInput = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> NegativePrefixInput = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> JoinAddressInput = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> HostButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> JoinButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SaveButton = nullptr;

	// Status line shown under the menu controls.
	UPROPERTY(BlueprintReadOnly, Category = "MainMenu", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> StatusText = nullptr;
};
