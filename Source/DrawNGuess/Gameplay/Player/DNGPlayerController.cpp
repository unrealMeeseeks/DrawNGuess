#include "DNGPlayerController.h"

#include "../Board/DNGBoardActor.h"
#include "../Flow/DNGGameMode.h"
#include "../Flow/DNGGameState.h"
#include "DNGPlayerState.h"
#include "../../UI/DNGMainMenuWidget.h"
#include "../../UI/DNGMatchWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Enables mouse-driven interaction and sets default fallback widget classes.
ADNGPlayerController::ADNGPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	PrimaryActorTick.bCanEverTick = true;
	bAutoManageActiveCameraTarget = false;
	MainMenuWidgetClass = UDNGMainMenuWidget::StaticClass();
	MatchWidgetClass = UDNGMatchWidget::StaticClass();
}

// Creates the local widgets, applies the input mode, and locks the view to the board.
void ADNGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	RefreshWidgets();
	ApplyInputMode();
	SetIgnoreLookInput(true);
	SetIgnoreMoveInput(true);
	EnsureBoardViewTarget();
}

// Polls drawing input while the pointer is held and the local player can draw.
void ADNGPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController() || !bPointerHeld || !CanUseDrawingControls())
	{
		EnsureBoardViewTarget();
		return;
	}

	EnsureBoardViewTarget();

	FVector2D BoardPoint;
	if (!TryGetBoardPoint(BoardPoint))
	{
		bHasLastBoardPoint = false;
		return;
	}

	if (!bHasLastBoardPoint)
	{
		LastBoardPoint = BoardPoint;
		bHasLastBoardPoint = true;
		EmitDrawSegment(BoardPoint, BoardPoint);
		return;
	}

	if (FVector2D::Distance(LastBoardPoint, BoardPoint) < MinimumSegmentLength)
	{
		return;
	}

	EmitDrawSegment(LastBoardPoint, BoardPoint);
	LastBoardPoint = BoardPoint;
}

// Registers the draw input action used by the prototype HUD.
void ADNGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	check(InputComponent);
	InputComponent->BindAction(TEXT("DrawPointer"), IE_Pressed, this, &ADNGPlayerController::HandleDrawPressed);
	InputComponent->BindAction(TEXT("DrawPointer"), IE_Released, this, &ADNGPlayerController::HandleDrawReleased);
}

// Updates the locally selected tool.
void ADNGPlayerController::RequestSetTool(EDNGDrawTool NewTool)
{
	ActiveTool = NewTool;
}

// Sends a finish request only when painter controls are currently valid.
void ADNGPlayerController::RequestFinishDrawing()
{
	if (CanUseDrawingControls())
	{
		ServerFinishDrawing();
	}
}

// Sends a guess request only when the local player may currently guess.
void ADNGPlayerController::RequestSubmitGuess(const FString& GuessText)
{
	if (CanSubmitGuess())
	{
		ServerSubmitGuess(GuessText);
	}
}

// Sends a next-round request only when the local player is allowed to advance.
void ADNGPlayerController::RequestNextRound()
{
	if (CanAdvanceRound())
	{
		ServerNextRound();
	}
}

// Saves the current board image locally and updates the HUD status string.
void ADNGPlayerController::RequestSaveBoard()
{
	ADNGBoardActor* BoardActor = GetBoardActor();
	if (!BoardActor)
	{
		SaveStatusMessage = TEXT("Save failed: board not found.");
		return;
	}

	FString SavedPath;
	if (BoardActor->SaveBoardImage(SavedPath))
	{
		SaveStatusMessage = FString::Printf(TEXT("Saved board to %s"), *SavedPath);
	}
	else
	{
		SaveStatusMessage = TEXT("Save failed.");
	}
}

// Checks the replicated GameState to see whether this local player is the painter.
bool ADNGPlayerController::IsPainterLocal() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		return DNGGameState->IsPainter(PlayerState);
	}

	return false;
}

// Returns the current replicated phase, or Lobby before GameState is ready.
EDNGMatchPhase ADNGPlayerController::GetCurrentPhase() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		return DNGGameState->GetMatchPhase();
	}

	return EDNGMatchPhase::Lobby;
}

// Returns a short UI label for the current phase.
FString ADNGPlayerController::GetPhaseDescription() const
{
	switch (GetCurrentPhase())
	{
	case EDNGMatchPhase::Lobby:
		return TEXT("Waiting for player 2");
	case EDNGMatchPhase::Drawing:
		return TEXT("Drawing");
	case EDNGMatchPhase::Guessing:
		return TEXT("Guessing");
	case EDNGMatchPhase::Results:
		return TEXT("Results");
	default:
		return TEXT("Unknown");
	}
}

// Returns a short UI label for the local player's role.
FString ADNGPlayerController::GetRoleDescription() const
{
	return IsPainterLocal() ? TEXT("Role: Painter") : TEXT("Role: Guesser");
}

// Builds the score summary shown in the fallback HUD.
FString ADNGPlayerController::GetScoreDescription() const
{
	FString Description = TEXT("Score: ");
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		bool bFirst = true;
		for (APlayerState* RawPlayerState : DNGGameState->PlayerArray)
		{
			if (const ADNGPlayerState* DNGPlayerState = Cast<ADNGPlayerState>(RawPlayerState))
			{
				if (!bFirst)
				{
					Description += TEXT(" | ");
				}

				Description += FString::Printf(TEXT("P%d=%d"), DNGPlayerState->GetPlayerId(), DNGPlayerState->GetRoundScore());
				bFirst = false;
			}
		}
	}

	return Description;
}

// Returns the currently replicated prompt settings as readable text.
FString ADNGPlayerController::GetPromptDescription() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		const FDNGPromptSettings& PromptSettings = DNGGameState->GetPromptSettings();
		return FString::Printf(TEXT("Positive prefix: %s\nNegative prefix: %s"), *PromptSettings.PositivePrefix, *PromptSettings.NegativePrefix);
	}

	return TEXT("Prompt settings not ready");
}

// Builds the detailed result summary shown after scoring.
FString ADNGPlayerController::GetResultDescription() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		const FDNGRoundResult& Result = DNGGameState->GetLastRoundResult();
		if (Result.GuessText.IsEmpty())
		{
			return TEXT("Waiting for round result");
		}

		return FString::Printf(
			TEXT("Guess: %s\nPositive: %s\nNegative: %s\nPositive score: %.6f\nNegative score: %.6f\nScore delta: %.6f\nPositive probability: %.6f\nNegative probability: %.6f\nDecision: %s\nBackend: %s\nSaved board: %s\nDiagnostic: %s"),
			*Result.GuessText,
			*Result.PositivePrompt,
			*Result.NegativePrompt,
			Result.PositiveScore,
			Result.NegativeScore,
			Result.PositiveScore - Result.NegativeScore,
			Result.PositiveProbability,
			Result.NegativeProbability,
			Result.bGuessAccepted ? TEXT("Accepted") : TEXT("Rejected"),
			Result.ScoringBackend.IsEmpty() ? TEXT("Unknown") : *Result.ScoringBackend,
			Result.SavedBoardPath.IsEmpty() ? TEXT("(none)") : *Result.SavedBoardPath,
			Result.DiagnosticMessage.IsEmpty() ? TEXT("(none)") : *Result.DiagnosticMessage);
	}

	return TEXT("No result yet");
}

// Returns context-sensitive instructions for the local player.
FString ADNGPlayerController::GetInstructionDescription() const
{
	switch (GetCurrentPhase())
	{
	case EDNGMatchPhase::Lobby:
		return TEXT("The round starts automatically when two players are connected.");
	case EDNGMatchPhase::Drawing:
		return IsPainterLocal()
			? TEXT("Use left mouse on the board. Switch tool in the UI, then click Finish Drawing.")
			: TEXT("Watch the painter until the drawing phase ends.");
	case EDNGMatchPhase::Guessing:
		return IsPainterLocal()
			? TEXT("Wait for the guesser to submit text.")
			: TEXT("Enter your guess on the right and submit it.");
	case EDNGMatchPhase::Results:
		return HasAuthority() ? TEXT("Review the result and click Next Round.") : TEXT("Wait for the host to start the next round.");
	default:
		return TEXT("");
	}
}

// Drawing is only allowed for the local painter during the drawing phase.
bool ADNGPlayerController::CanUseDrawingControls() const
{
	return IsPainterLocal() && GetCurrentPhase() == EDNGMatchPhase::Drawing;
}

// Guessing is only allowed during the guessing phase for non-painters.
bool ADNGPlayerController::CanSubmitGuess() const
{
	return !IsPainterLocal() && GetCurrentPhase() == EDNGMatchPhase::Guessing;
}

// Advancing is currently restricted to the listen-server host on the result screen.
bool ADNGPlayerController::CanAdvanceRound() const
{
	return HasAuthority() && GetCurrentPhase() == EDNGMatchPhase::Results;
}

// Forwards one locally generated segment to the authoritative GameMode.
void ADNGPlayerController::ServerAddDrawSegment_Implementation(const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool)
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleDrawSegment(this, Start, End, Tool);
	}
}

// Forwards the finish-drawing request to the authoritative GameMode.
void ADNGPlayerController::ServerFinishDrawing_Implementation()
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleFinishDrawing(this);
	}
}

// Forwards a guess submission to the authoritative GameMode.
void ADNGPlayerController::ServerSubmitGuess_Implementation(const FString& GuessText)
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleSubmitGuess(this, GuessText);
	}
}

// Forwards the next-round request to the authoritative GameMode.
void ADNGPlayerController::ServerNextRound_Implementation()
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleNextRound(this);
	}
}

// Ensures the correct fallback widget exists for the current phase.
void ADNGPlayerController::RefreshWidgets()
{
	if (!IsLocalController())
	{
		return;
	}

	const bool bStandalone = GetNetMode() == NM_Standalone;
	if (bStandalone)
	{
		if (!MainMenuWidget)
		{
			UClass* ResolvedMainMenuClass = MainMenuWidgetClass ? MainMenuWidgetClass.Get() : UDNGMainMenuWidget::StaticClass();
			MainMenuWidget = CreateWidget<UDNGMainMenuWidget>(this, ResolvedMainMenuClass);
		}

		if (MainMenuWidget && !MainMenuWidget->IsInViewport())
		{
			MainMenuWidget->AddToViewport(20);
		}

		if (MatchWidget)
		{
			MatchWidget->RemoveFromParent();
		}

		return;
	}

	if (MainMenuWidget)
	{
		MainMenuWidget->RemoveFromParent();
	}

	if (!MatchWidget)
	{
		UClass* ResolvedMatchClass = MatchWidgetClass ? MatchWidgetClass.Get() : UDNGMatchWidget::StaticClass();
		MatchWidget = CreateWidget<UDNGMatchWidget>(this, ResolvedMatchClass);
	}

	if (MatchWidget && !MatchWidget->IsInViewport())
	{
		MatchWidget->AddToViewport(10);
	}
}

// Switches between menu-focused and gameplay-focused mouse input modes.
void ADNGPlayerController::ApplyInputMode()
{
	if (!IsLocalController())
	{
		return;
	}

	if (GetNetMode() == NM_Standalone && MainMenuWidget)
	{
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, MainMenuWidget, EMouseLockMode::DoNotLock, false);
	}
	else
	{
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(this, MatchWidget, EMouseLockMode::DoNotLock, false, false);
	}

	bShowMouseCursor = true;
}

// Draws locally for prediction and sends subdivided segments to the server.
void ADNGPlayerController::EmitDrawSegment(const FVector2D& Start, const FVector2D& End)
{
	const float Distance = FVector2D::Distance(Start, End);
	const int32 SegmentCount = Distance <= KINDA_SMALL_NUMBER ? 1 : FMath::Max(1, FMath::CeilToInt(Distance / MaxSegmentLength));

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float Alpha0 = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const float Alpha1 = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SegmentCount);

		FDNGDrawSegment Segment;
		Segment.Start = FMath::Lerp(Start, End, Alpha0);
		Segment.End = FMath::Lerp(Start, End, Alpha1);
		Segment.Tool = ActiveTool;
		Segment.Thickness = ActiveTool == EDNGDrawTool::Eraser ? 18.0f : 7.0f;

		if (ADNGBoardActor* BoardActor = GetBoardActor())
		{
			BoardActor->AddPredictedSegment(Segment);
		}

		ServerAddDrawSegment(Segment.Start, Segment.End, ActiveTool);
	}
}

// Keeps the controller camera locked to the board actor's orthographic camera.
void ADNGPlayerController::EnsureBoardViewTarget()
{
	ADNGBoardActor* BoardActor = GetBoardActor();
	if (!BoardActor)
	{
		return;
	}

	if (GetViewTarget() != BoardActor)
	{
		SetViewTarget(BoardActor);
		BoardActor->RefreshBoardVisuals();
	}
}

// Starts pointer capture for drawing.
void ADNGPlayerController::HandleDrawPressed()
{
	bPointerHeld = true;
	bHasLastBoardPoint = false;
}

// Stops pointer capture and resets stroke continuity.
void ADNGPlayerController::HandleDrawReleased()
{
	bPointerHeld = false;
	bHasLastBoardPoint = false;
}

// Converts the cursor position into normalized board UV coordinates using hit UV lookup.
bool ADNGPlayerController::TryGetBoardPoint(FVector2D& OutBoardPoint) const
{
	ADNGBoardActor* BoardActor = GetBoardActor();
	if (!BoardActor || !BoardActor->GetBoardMesh())
	{
		return false;
	}

	FVector WorldOrigin;
	FVector WorldDirection;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	const FVector TraceEnd = WorldOrigin + (WorldDirection * 100000.0f);
	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DrawBoardTrace), true);
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnFaceIndex = true;
	QueryParams.AddIgnoredActor(GetPawn());

	const bool bHitBoard = BoardActor->GetBoardMesh()->LineTraceComponent(HitResult, WorldOrigin, TraceEnd, QueryParams);
	if (!bHitBoard || HitResult.GetComponent() != BoardActor->GetBoardMesh())
	{
		return false;
	}

	if (UGameplayStatics::FindCollisionUV(HitResult, 0, OutBoardPoint))
	{
		return true;
	}

	// Fallback for cases where collision UV support is unavailable or the hit data is incomplete.
	FVector2D BoardPoint;
	if (!BoardActor->ProjectWorldToBoard(HitResult.ImpactPoint, BoardPoint))
	{
		return false;
	}

	OutBoardPoint = BoardActor->BoardPointToUV(BoardPoint);
	return true;
}

// Typed helper for accessing the replicated GameState.
ADNGGameState* ADNGPlayerController::GetDNGGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ADNGGameState>() : nullptr;
}

// Typed helper for accessing the shared board actor.
ADNGBoardActor* ADNGPlayerController::GetBoardActor() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		return DNGGameState->GetBoardActor();
	}

	return nullptr;
}
