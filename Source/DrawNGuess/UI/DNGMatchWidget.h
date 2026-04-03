#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DNGMatchWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

// Fallback in-match HUD implemented entirely in C++.
UCLASS()
class DRAWNGUESS_API UDNGMatchWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	// Builds the fallback widget tree when no Blueprint layout exists.
	void BuildWidgetTree();

	// Pulls current state from the owning player controller and updates the HUD.
	void RefreshFromController();

	UFUNCTION()
	void HandlePencilClicked();

	UFUNCTION()
	void HandleEraserClicked();

	UFUNCTION()
	void HandleFinishClicked();

	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleNextRoundClicked();

	UFUNCTION()
	void HandleSaveClicked();

	// Text widgets used by the fallback HUD.
	UPROPERTY()
	TObjectPtr<UTextBlock> PhaseText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> RoleText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> ScoreText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> PromptText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> ResultText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> InstructionText = nullptr;

	UPROPERTY()
	TObjectPtr<UTextBlock> SaveStatusText = nullptr;

	UPROPERTY()
	TObjectPtr<UEditableTextBox> GuessInput = nullptr;

	// Action buttons used by the fallback HUD.
	UPROPERTY()
	TObjectPtr<UButton> PencilButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> EraserButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> FinishButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> SubmitButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> NextRoundButton = nullptr;

	UPROPERTY()
	TObjectPtr<UButton> SaveButton = nullptr;
};
