#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "DNGTypes.h"
#include "DNGPlayerController.generated.h"

class ADNGBoardActor;
class ADNGGameState;
class UDNGMainMenuWidget;
class UDNGMatchWidget;

UCLASS()
class DRAWNGUESS_API ADNGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADNGPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintCallable)
	void RequestSetTool(EDNGDrawTool NewTool);

	UFUNCTION(BlueprintCallable)
	void RequestFinishDrawing();

	UFUNCTION(BlueprintCallable)
	void RequestSubmitGuess(const FString& GuessText);

	UFUNCTION(BlueprintCallable)
	void RequestNextRound();

	UFUNCTION(BlueprintCallable)
	bool IsPainterLocal() const;

	UFUNCTION(BlueprintCallable)
	bool IsHostLocal() const { return HasAuthority(); }

	UFUNCTION(BlueprintCallable)
	EDNGMatchPhase GetCurrentPhase() const;

	UFUNCTION(BlueprintCallable)
	FString GetPhaseDescription() const;

	UFUNCTION(BlueprintCallable)
	FString GetRoleDescription() const;

	UFUNCTION(BlueprintCallable)
	FString GetScoreDescription() const;

	UFUNCTION(BlueprintCallable)
	FString GetPromptDescription() const;

	UFUNCTION(BlueprintCallable)
	FString GetResultDescription() const;

	UFUNCTION(BlueprintCallable)
	FString GetInstructionDescription() const;

	UFUNCTION(BlueprintCallable)
	EDNGDrawTool GetActiveTool() const { return ActiveTool; }

	UFUNCTION(BlueprintCallable)
	bool CanUseDrawingControls() const;

	UFUNCTION(BlueprintCallable)
	bool CanSubmitGuess() const;

	UFUNCTION(BlueprintCallable)
	bool CanAdvanceRound() const;

protected:
	UFUNCTION(Server, Reliable)
	void ServerAddDrawSegment(const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool);

	UFUNCTION(Server, Reliable)
	void ServerFinishDrawing();

	UFUNCTION(Server, Reliable)
	void ServerSubmitGuess(const FString& GuessText);

	UFUNCTION(Server, Reliable)
	void ServerNextRound();

private:
	void EmitDrawSegment(const FVector2D& Start, const FVector2D& End);
	void RefreshWidgets();
	void ApplyInputMode();
	void HandleDrawPressed();
	void HandleDrawReleased();
	bool TryGetBoardPoint(FVector2D& OutBoardPoint) const;
	ADNGGameState* GetDNGGameState() const;
	ADNGBoardActor* GetBoardActor() const;

	UPROPERTY()
	TObjectPtr<UDNGMainMenuWidget> MainMenuWidget = nullptr;

	UPROPERTY()
	TObjectPtr<UDNGMatchWidget> MatchWidget = nullptr;

	UPROPERTY()
	bool bPointerHeld = false;

	UPROPERTY()
	bool bHasLastBoardPoint = false;

	UPROPERTY()
	FVector2D LastBoardPoint = FVector2D::ZeroVector;

	UPROPERTY()
	EDNGDrawTool ActiveTool = EDNGDrawTool::Pencil;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDNGMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDNGMatchWidget> MatchWidgetClass;

	UPROPERTY(EditDefaultsOnly)
	float MinimumSegmentLength = 0.0015f;

	UPROPERTY(EditDefaultsOnly)
	float MaxSegmentLength = 0.01f;
};
