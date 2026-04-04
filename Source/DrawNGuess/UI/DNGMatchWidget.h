#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DNGMatchWidget.generated.h"

class UButton;
class UEditableTextBox;
class UMultiLineEditableTextBox;
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

	// Binds button callbacks for either Blueprint-owned or fallback-created widgets.
	void BindActions();

	// Pulls current state from the owning player controller and updates the HUD.
	void RefreshFromController();

	UFUNCTION()
	void HandlePencilClicked();

	UFUNCTION()
	void HandleEraserClicked();

	UFUNCTION()
	void HandleBlackClicked();

	UFUNCTION()
	void HandleRedClicked();

	UFUNCTION()
	void HandleBlueClicked();

	UFUNCTION()
	void HandleGreenClicked();

	UFUNCTION()
	void HandlePencilSmallClicked();

	UFUNCTION()
	void HandlePencilMediumClicked();

	UFUNCTION()
	void HandlePencilLargeClicked();

	UFUNCTION()
	void HandleEraserSmallClicked();

	UFUNCTION()
	void HandleEraserMediumClicked();

	UFUNCTION()
	void HandleEraserLargeClicked();

	UFUNCTION()
	void HandleFinishClicked();

	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleNextRoundClicked();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleAgentSendClicked();

	UFUNCTION()
	void HandleAgentResetClicked();

	// Text widgets used by the fallback HUD.
	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PhaseText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> RoleText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ScoreText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> PromptText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> ResultText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> InstructionText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> BrushText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> SaveStatusText = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UEditableTextBox> GuessInput = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UMultiLineEditableTextBox> AgentInstructionInput = nullptr;

	// Action buttons used by the fallback HUD.
	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PencilButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> EraserButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BlackButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> RedButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> BlueButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> GreenButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PencilSmallButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PencilMediumButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> PencilLargeButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> EraserSmallButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> EraserMediumButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> EraserLargeButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> FinishButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SubmitButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> NextRoundButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> SaveButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> AgentSendButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> AgentResetButton = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "MatchHUD", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> AgentStatusText = nullptr;
};
