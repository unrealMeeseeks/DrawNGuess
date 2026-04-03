#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "../../Core/DNGTypes.h"
#include "DNGPlayerController.generated.h"

class ADNGBoardActor;
class ADNGGameState;
class UDNGMainMenuWidget;
class UDNGMatchWidget;

// Client-side controller that owns fallback UI, captures drawing input,
// and forwards gameplay actions to the authoritative server.
UCLASS()
class DRAWNGUESS_API ADNGPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ADNGPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

	// Switches the currently selected local drawing tool.
	UFUNCTION(BlueprintCallable)
	void RequestSetTool(EDNGDrawTool NewTool);

	// Requests that the server finish the drawing phase.
	UFUNCTION(BlueprintCallable)
	void RequestFinishDrawing();

	// Requests that the server score the supplied guess text.
	UFUNCTION(BlueprintCallable)
	void RequestSubmitGuess(const FString& GuessText);

	// Requests that the server start the next round.
	UFUNCTION(BlueprintCallable)
	void RequestNextRound();

	// Saves the current board image locally.
	UFUNCTION(BlueprintCallable)
	void RequestSaveBoard();

	// Returns whether this local player is the active painter.
	UFUNCTION(BlueprintCallable)
	bool IsPainterLocal() const;

	// Returns whether this controller belongs to the listen server host.
	UFUNCTION(BlueprintCallable)
	bool IsHostLocal() const { return HasAuthority(); }

	// Returns the current replicated phase.
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
	FString GetSaveStatusDescription() const { return SaveStatusMessage; }

	UFUNCTION(BlueprintCallable)
	EDNGDrawTool GetActiveTool() const { return ActiveTool; }

	// Returns whether drawing controls are currently active.
	UFUNCTION(BlueprintCallable)
	bool CanUseDrawingControls() const;

	// Returns whether the local player may submit a guess now.
	UFUNCTION(BlueprintCallable)
	bool CanSubmitGuess() const;

	// Returns whether the local player may advance to the next round now.
	UFUNCTION(BlueprintCallable)
	bool CanAdvanceRound() const;

protected:
	// Reliable RPC used to send one segment from the local painter to the server.
	UFUNCTION(Server, Reliable)
	void ServerAddDrawSegment(const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool);

	// Reliable RPC used to finish the drawing phase.
	UFUNCTION(Server, Reliable)
	void ServerFinishDrawing();

	// Reliable RPC used to submit guess text.
	UFUNCTION(Server, Reliable)
	void ServerSubmitGuess(const FString& GuessText);

	// Reliable RPC used to request the next round.
	UFUNCTION(Server, Reliable)
	void ServerNextRound();

private:
	// Emits one logical segment locally and remotely.
	void EmitDrawSegment(const FVector2D& Start, const FVector2D& End);

	// Keeps the view target locked to the board camera.
	void EnsureBoardViewTarget();

	// Creates or removes widgets according to the replicated match phase.
	void RefreshWidgets();

	// Applies the proper mouse/UI input mode for the current phase.
	void ApplyInputMode();

	// Pointer handlers used to start and stop drawing.
	void HandleDrawPressed();
	void HandleDrawReleased();

	// Converts the cursor position into normalized board UV coordinates.
	bool TryGetBoardPoint(FVector2D& OutBoardPoint) const;

	// Convenience accessors for common replicated state objects.
	ADNGGameState* GetDNGGameState() const;
	ADNGBoardActor* GetBoardActor() const;

	// Menu widget shown in the lobby.
	UPROPERTY()
	TObjectPtr<UDNGMainMenuWidget> MainMenuWidget = nullptr;

	// In-match widget shown during drawing, guessing, and results.
	UPROPERTY()
	TObjectPtr<UDNGMatchWidget> MatchWidget = nullptr;

	// Tracks whether the pointer is currently held down for drawing.
	UPROPERTY()
	bool bPointerHeld = false;

	// Tracks whether there is a previous board point for stroke continuation.
	UPROPERTY()
	bool bHasLastBoardPoint = false;

	// Previously accepted board point in normalized UV space.
	UPROPERTY()
	FVector2D LastBoardPoint = FVector2D::ZeroVector;

	// Locally selected drawing tool.
	UPROPERTY()
	EDNGDrawTool ActiveTool = EDNGDrawTool::Pencil;

	// Latest local save status displayed in the HUD.
	UPROPERTY()
	FString SaveStatusMessage;

	// Optional Blueprint override for the menu widget.
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDNGMainMenuWidget> MainMenuWidgetClass;

	// Optional Blueprint override for the in-match widget.
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UDNGMatchWidget> MatchWidgetClass;

	// Minimum UV-space movement before a new segment is emitted.
	UPROPERTY(EditDefaultsOnly)
	float MinimumSegmentLength = 0.0015f;

	// Maximum UV-space step allowed before long drags are subdivided.
	UPROPERTY(EditDefaultsOnly)
	float MaxSegmentLength = 0.01f;
};
