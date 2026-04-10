#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Templates/SubclassOf.h"
#include "../../AI/DeepSeek/DNGDeepSeekTypes.h"
#include "../../Core/DNGTypes.h"
#include "DNGPlayerController.generated.h"

class ADNGBoardActor;
class ADNGGameState;
class UDNGDeepSeekAgentService;
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

	// Sets the currently selected pencil color.
	UFUNCTION(BlueprintCallable)
	void RequestSetPencilColor(const FLinearColor& NewColor);

	// Sets the currently selected pencil thickness.
	UFUNCTION(BlueprintCallable)
	void RequestSetPencilThickness(float NewThickness);

	// Sets the currently selected eraser thickness.
	UFUNCTION(BlueprintCallable)
	void RequestSetEraserThickness(float NewThickness);

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

	// Requests an agent-generated plan and lets the model decide draw, revise, or noop.
	UFUNCTION(BlueprintCallable)
	void RequestAgentInstruction(const FString& Instruction);

	// Clears the locally remembered agent SVG state and any queued playback.
	UFUNCTION(BlueprintCallable)
	void ResetAgentSession();

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
	FString GetBrushDescription() const;

	UFUNCTION(BlueprintCallable)
	FString GetAgentStatusDescription() const { return AgentStatusMessage; }

	UFUNCTION(BlueprintCallable)
	FString GetSaveStatusDescription() const { return SaveStatusMessage; }

	UFUNCTION(BlueprintCallable)
	EDNGDrawTool GetActiveTool() const { return ActiveTool; }

	UFUNCTION(BlueprintCallable)
	FLinearColor GetActivePencilColor() const { return ActivePencilColor; }

	UFUNCTION(BlueprintCallable)
	float GetActivePencilThickness() const { return PencilThickness; }

	UFUNCTION(BlueprintCallable)
	float GetActiveEraserThickness() const { return EraserThickness; }

	UFUNCTION(BlueprintCallable)
	bool CanUseAgentDrawingControls() const;

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
	void ServerAddDrawSegment(const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool, float Thickness, const FLinearColor& Color);

	// Reliable RPC used to finish the drawing phase.
	UFUNCTION(Server, Reliable)
	void ServerFinishDrawing();

	// Reliable RPC used to submit guess text.
	UFUNCTION(Server, Reliable)
	void ServerSubmitGuess(const FString& GuessText);

	// Reliable RPC used to request the next round.
	UFUNCTION(Server, Reliable)
	void ServerNextRound();

	// Reliable RPC used by the painter to clear the authoritative board before SVG playback.
	UFUNCTION(Server, Reliable)
	void ServerClearBoardForAgent();

private:
	// Emits one logical segment locally and remotely.
	void EmitDrawSegment(const FVector2D& Start, const FVector2D& End);

	// Emits one prebuilt segment locally and remotely.
	void EmitResolvedSegment(const FDNGDrawSegment& Segment);

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

	// Ensures the DeepSeek service exists and is configured from the current GameInstance state.
	void EnsureDeepSeekAgentService();

	// Handles an asynchronous DeepSeek response and converts it into queued board segments.
	void HandleAgentPlanResult(bool bSuccess, const FDNGDeepSeekDrawingPlan& Plan, const FString& ErrorMessage);

	// Finalizes one accepted agent plan into SVG playback.
	void FinalizeAgentPlan(const FDNGDeepSeekDrawingPlan& Plan);

	// Builds a board-derived SVG snapshot used as revision context for the agent.
	bool BuildCurrentBoardSvgContext(FString& OutSvgContext) const;

	// Parses the authoritative SVG, computes the delta from the current board, and starts playback.
	void QueueAgentSvgPlayback(const FDNGDeepSeekDrawingPlan& Plan);

	// Converts the current board and target SVG into an erase-then-draw playback sequence.
	void BuildAgentDiffPlaybackSegments(const TArray<FDNGDrawSegment>& CurrentSegments, const TArray<FDNGDrawSegment>& TargetSegments, TArray<FDNGDrawSegment>& OutPlaybackSegments) const;

	// Replays one queued segment every timer tick.
	void PlayNextAgentSegment();

	// Resets local agent request/playback state when a new round starts.
	void RefreshAgentRoundState();

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

	// Currently selected pencil color.
	UPROPERTY()
	FLinearColor ActivePencilColor = FLinearColor::Black;

	// Currently selected pencil thickness.
	UPROPERTY()
	float PencilThickness = 7.0f;

	// Currently selected eraser thickness.
	UPROPERTY()
	float EraserThickness = 18.0f;

	// Latest local save status displayed in the HUD.
	UPROPERTY()
	FString SaveStatusMessage;

	// Latest agent status text shown in the HUD.
	UPROPERTY()
	FString AgentStatusMessage;

	// Latest final SVG returned by the agent and used as revision context.
	UPROPERTY()
	FString LastAgentSvg;

	// Latest raw JSON content returned by the model for debugging.
	UPROPERTY()
	FString LastAgentRawResponse;

	// Original user instruction for the current agent request chain.
	UPROPERTY()
	FString PendingAgentOriginalInstruction;

	// Local DeepSeek HTTP service used by this controller.
	UPROPERTY(Transient)
	TObjectPtr<UDNGDeepSeekAgentService> DeepSeekAgentService = nullptr;

	// Segments currently queued for timed playback.
	UPROPERTY(Transient)
	TArray<FDNGDrawSegment> QueuedAgentSegments;

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

	// Playback interval used when replaying agent-generated segments.
	UPROPERTY(EditDefaultsOnly, Category = "Agent")
	float AgentPlaybackInterval = 0.035f;

	// Tracks whether an asynchronous DeepSeek request is currently in flight.
	UPROPERTY(Transient)
	bool bAgentRequestInFlight = false;

	// Current segment playback index inside the queued agent segment list.
	UPROPERTY(Transient)
	int32 NextAgentSegmentIndex = 0;

	// Last round number seen by this controller so agent state can reset automatically.
	UPROPERTY(Transient)
	int32 LastObservedRoundNumber = INDEX_NONE;

	FTimerHandle AgentPlaybackTimerHandle;
};
