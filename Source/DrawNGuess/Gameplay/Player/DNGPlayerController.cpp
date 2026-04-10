#include "DNGPlayerController.h"

#include "../../AI/DeepSeek/DNGDeepSeekAgentService.h"
#include "../../AI/DeepSeek/DNGSvgParser.h"
#include "../Flow/DNGGameInstance.h"
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
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 AgentDiffRasterWidth = 128;
	constexpr int32 AgentDiffRasterHeight = 128;
	constexpr float AgentDiffEraserThickness = 18.0f;

	static uint8 ResolveRasterColorIndex(const FDNGDrawSegment& Segment)
	{
		if (Segment.Tool == EDNGDrawTool::Eraser)
		{
			return 0;
		}

		const TArray<FLinearColor> Palette =
		{
			FLinearColor::Black,
			FLinearColor(0.85f, 0.15f, 0.15f, 1.0f),
			FLinearColor(0.15f, 0.35f, 0.95f, 1.0f),
			FLinearColor(0.15f, 0.7f, 0.25f, 1.0f)
		};

		double BestDistance = TNumericLimits<double>::Max();
		uint8 BestIndex = 1;
		for (int32 Index = 0; Index < Palette.Num(); ++Index)
		{
			const FVector Delta(Segment.Color.R - Palette[Index].R, Segment.Color.G - Palette[Index].G, Segment.Color.B - Palette[Index].B);
			const double Distance = Delta.SizeSquared();
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestIndex = static_cast<uint8>(Index + 1);
			}
		}

		return BestIndex;
	}

	static FIntPoint UVToRasterPoint(const FVector2D& UV)
	{
		return FIntPoint(
			FMath::Clamp(FMath::RoundToInt(UV.X * static_cast<float>(AgentDiffRasterWidth - 1)), 0, AgentDiffRasterWidth - 1),
			FMath::Clamp(FMath::RoundToInt(UV.Y * static_cast<float>(AgentDiffRasterHeight - 1)), 0, AgentDiffRasterHeight - 1));
	}

	static FVector2D RasterPointToUV(float X, float Y)
	{
		return FVector2D(
			FMath::Clamp(X / static_cast<float>(AgentDiffRasterWidth - 1), 0.0f, 1.0f),
			FMath::Clamp(Y / static_cast<float>(AgentDiffRasterHeight - 1), 0.0f, 1.0f));
	}

	static void StampRasterDisc(TArray<uint8>& Raster, int32 CenterX, int32 CenterY, int32 Radius, uint8 Value)
	{
		const int32 ClampedRadius = FMath::Max(1, Radius);
		const int32 RadiusSquared = ClampedRadius * ClampedRadius;
		for (int32 Y = CenterY - ClampedRadius; Y <= CenterY + ClampedRadius; ++Y)
		{
			if (Y < 0 || Y >= AgentDiffRasterHeight)
			{
				continue;
			}

			for (int32 X = CenterX - ClampedRadius; X <= CenterX + ClampedRadius; ++X)
			{
				if (X < 0 || X >= AgentDiffRasterWidth)
				{
					continue;
				}

				const int32 DeltaX = X - CenterX;
				const int32 DeltaY = Y - CenterY;
				if ((DeltaX * DeltaX) + (DeltaY * DeltaY) <= RadiusSquared)
				{
					Raster[(Y * AgentDiffRasterWidth) + X] = Value;
				}
			}
		}
	}

	static void RasterizeSegments(const TArray<FDNGDrawSegment>& Segments, TArray<uint8>& OutRaster)
	{
		OutRaster.Init(0, AgentDiffRasterWidth * AgentDiffRasterHeight);

		for (const FDNGDrawSegment& Segment : Segments)
		{
			const FIntPoint Start = UVToRasterPoint(Segment.Start);
			const FIntPoint End = UVToRasterPoint(Segment.End);
			const float Distance = FVector2D::Distance(FVector2D(Start), FVector2D(End));
			const int32 SampleCount = Distance <= KINDA_SMALL_NUMBER ? 1 : FMath::Max(1, FMath::CeilToInt(Distance * 2.0f));
			const int32 Radius = FMath::Max(1, FMath::RoundToInt((Segment.Thickness / 2048.0f) * static_cast<float>(AgentDiffRasterWidth) * 0.9f));
			const uint8 Value = ResolveRasterColorIndex(Segment);

			for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
			{
				const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
				const int32 X = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Start.X), static_cast<float>(End.X), Alpha));
				const int32 Y = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Start.Y), static_cast<float>(End.Y), Alpha));
				StampRasterDisc(OutRaster, X, Y, Radius, Value);
			}
		}
	}

	static bool SegmentTouchesChangeMask(const FDNGDrawSegment& Segment, const TArray<uint8>& ChangeMask)
	{
		const FIntPoint Start = UVToRasterPoint(Segment.Start);
		const FIntPoint End = UVToRasterPoint(Segment.End);
		const float Distance = FVector2D::Distance(FVector2D(Start), FVector2D(End));
		const int32 SampleCount = Distance <= KINDA_SMALL_NUMBER ? 1 : FMath::Max(1, FMath::CeilToInt(Distance * 2.0f));
		const int32 Radius = FMath::Max(1, FMath::RoundToInt((Segment.Thickness / 2048.0f) * static_cast<float>(AgentDiffRasterWidth) * 0.75f));
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			const int32 CenterX = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Start.X), static_cast<float>(End.X), Alpha));
			const int32 CenterY = FMath::RoundToInt(FMath::Lerp(static_cast<float>(Start.Y), static_cast<float>(End.Y), Alpha));
			for (int32 Y = CenterY - Radius; Y <= CenterY + Radius; ++Y)
			{
				if (Y < 0 || Y >= AgentDiffRasterHeight)
				{
					continue;
				}
				for (int32 X = CenterX - Radius; X <= CenterX + Radius; ++X)
				{
					if (X < 0 || X >= AgentDiffRasterWidth)
					{
						continue;
					}
					if (ChangeMask[(Y * AgentDiffRasterWidth) + X] != 0)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	static void BuildEraserSegmentsFromMask(const TArray<uint8>& RemoveMask, TArray<FDNGDrawSegment>& OutSegments)
	{
		const int32 Radius = FMath::Max(1, FMath::RoundToInt((AgentDiffEraserThickness / 2048.0f) * static_cast<float>(AgentDiffRasterWidth) * 0.9f));
		const int32 RowStep = FMath::Max(1, Radius);
		for (int32 Y = 0; Y < AgentDiffRasterHeight; Y += RowStep)
		{
			int32 X = 0;
			while (X < AgentDiffRasterWidth)
			{
				while (X < AgentDiffRasterWidth && RemoveMask[(Y * AgentDiffRasterWidth) + X] == 0)
				{
					++X;
				}

				if (X >= AgentDiffRasterWidth)
				{
					break;
				}

				const int32 StartX = X;
				while (X < AgentDiffRasterWidth && RemoveMask[(Y * AgentDiffRasterWidth) + X] != 0)
				{
					++X;
				}

				const int32 EndX = X - 1;
				FDNGDrawSegment Segment;
				Segment.Tool = EDNGDrawTool::Eraser;
				Segment.Color = FLinearColor::White;
				Segment.Thickness = AgentDiffEraserThickness;
				Segment.Start = RasterPointToUV(static_cast<float>(StartX), static_cast<float>(Y));
				Segment.End = RasterPointToUV(static_cast<float>(EndX), static_cast<float>(Y));
				OutSegments.Add(Segment);
			}
		}
	}
}

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

	EnsureDeepSeekAgentService();
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

	RefreshAgentRoundState();

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

// Updates the local pencil color preset.
void ADNGPlayerController::RequestSetPencilColor(const FLinearColor& NewColor)
{
	ActivePencilColor = NewColor;
}

// Updates the local pencil thickness preset.
void ADNGPlayerController::RequestSetPencilThickness(float NewThickness)
{
	PencilThickness = FMath::Clamp(NewThickness, 1.0f, 64.0f);
}

// Updates the local eraser thickness preset.
void ADNGPlayerController::RequestSetEraserThickness(float NewThickness)
{
	EraserThickness = FMath::Clamp(NewThickness, 1.0f, 64.0f);
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

// Starts an agent request and lets the model decide whether to draw, revise, or noop.
void ADNGPlayerController::RequestAgentInstruction(const FString& Instruction)
{
	if (!CanUseAgentDrawingControls())
	{
		AgentStatusMessage = TEXT("Agent drawing is only available for the active painter during the drawing phase.");
		return;
	}

	const FString TrimmedInstruction = Instruction.TrimStartAndEnd();
	if (TrimmedInstruction.IsEmpty())
	{
		AgentStatusMessage = TEXT("Agent instruction is empty.");
		return;
	}

	EnsureDeepSeekAgentService();
	if (!DeepSeekAgentService || !DeepSeekAgentService->HasApiKey())
	{
		AgentStatusMessage = TEXT("DeepSeek API key is missing. Add Config/DeepSeekAgent.json or enter and save it in the menu.");
		return;
	}

	bAgentRequestInFlight = true;
	PendingAgentOriginalInstruction = TrimmedInstruction;
	AgentStatusMessage = TEXT("Requesting DeepSeek SVG...");

	FString CurrentSvgContext;
	BuildCurrentBoardSvgContext(CurrentSvgContext);

	TWeakObjectPtr<ADNGPlayerController> WeakThis(this);
	DeepSeekAgentService->RequestDrawingPlan(
		TrimmedInstruction,
		CurrentSvgContext,
		UDNGDeepSeekAgentService::FOnDeepSeekPlanCompleted::CreateLambda(
			[WeakThis](bool bSuccess, const FDNGDeepSeekDrawingPlan& Plan, const FString& ErrorMessage)
			{
				if (WeakThis.IsValid())
				{
					WeakThis->HandleAgentPlanResult(bSuccess, Plan, ErrorMessage);
				}
			}));
}

// Clears any remembered SVG/replay state so the next agent request starts cleanly.
void ADNGPlayerController::ResetAgentSession()
{
	bAgentRequestInFlight = false;
	LastAgentSvg.Reset();
	LastAgentRawResponse.Reset();
	PendingAgentOriginalInstruction.Reset();
	QueuedAgentSegments.Reset();
	NextAgentSegmentIndex = 0;
	AgentStatusMessage = TEXT("Agent session reset.");

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AgentPlaybackTimerHandle);
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
			// TEXT("Guess: %s\nPositive: %s\nNegative: %s\nPositive score: %.6f\nNegative score: %.6f\nScore delta: %.6f\nPositive probability: %.6f\nNegative probability: %.6f\nDecision: %s\nBackend: %s\nSaved board: %s\nDiagnostic: %s"),
			TEXT("Guess: %s\nPositive: %s\nNegative: %s\nPositive score: %.6f\nNegative score: %.6f\nScore delta: %.6f\nPositive probability: %.6f\nNegative probability: %.6f\nDecision: %s"),
			*Result.GuessText,
			*Result.PositivePrompt,
			*Result.NegativePrompt,
			Result.PositiveScore,
			Result.NegativeScore,
			Result.PositiveScore - Result.NegativeScore,
			Result.PositiveProbability,
			Result.NegativeProbability,
			Result.bGuessAccepted ? TEXT("Accepted") : TEXT("Rejected"));
			// Result.ScoringBackend.IsEmpty() ? TEXT("Unknown") : *Result.ScoringBackend,
			// Result.SavedBoardPath.IsEmpty() ? TEXT("(none)") : *Result.SavedBoardPath,
			// Result.DiagnosticMessage.IsEmpty() ? TEXT("(none)") : *Result.DiagnosticMessage);
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

// Summarizes the currently selected tool, color, and thickness presets.
FString ADNGPlayerController::GetBrushDescription() const
{
	const FString ToolName = ActiveTool == EDNGDrawTool::Eraser ? TEXT("Eraser") : TEXT("Pencil");
	const FString ColorDescription = FString::Printf(TEXT("R%.0f G%.0f B%.0f"), ActivePencilColor.R * 255.0f, ActivePencilColor.G * 255.0f, ActivePencilColor.B * 255.0f);
	return FString::Printf(
		TEXT("Tool: %s | Pencil color: %s | Pencil size: %.0f | Eraser size: %.0f"),
		*ToolName,
		*ColorDescription,
		PencilThickness,
		EraserThickness);
}

// Agent requests are limited to the active painter and blocked while another request is pending.
bool ADNGPlayerController::CanUseAgentDrawingControls() const
{
	const bool bPlaybackActive = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(AgentPlaybackTimerHandle);
	return CanUseDrawingControls() && !bAgentRequestInFlight && !bPlaybackActive;
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
void ADNGPlayerController::ServerAddDrawSegment_Implementation(const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool, float Thickness, const FLinearColor& Color)
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleDrawSegment(this, Start, End, Tool, Thickness, Color);
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

// Clears the authoritative board so agent SVG playback can redraw the final state from scratch.
void ADNGPlayerController::ServerClearBoardForAgent_Implementation()
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleClearBoardForAgent(this);
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
		Segment.Color = ActivePencilColor;
		Segment.Thickness = ActiveTool == EDNGDrawTool::Eraser ? EraserThickness : PencilThickness;

		EmitResolvedSegment(Segment);
	}
}

// Emits a fully resolved segment without reapplying local tool-size selection logic.
void ADNGPlayerController::EmitResolvedSegment(const FDNGDrawSegment& Segment)
{
	if (ADNGBoardActor* BoardActor = GetBoardActor())
	{
		BoardActor->AddPredictedSegment(Segment);
	}

	ServerAddDrawSegment(Segment.Start, Segment.End, Segment.Tool, Segment.Thickness, Segment.Color);
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

// Ensures the local DeepSeek service exists and mirrors the latest local config from GameInstance.
void ADNGPlayerController::EnsureDeepSeekAgentService()
{
	if (!DeepSeekAgentService)
	{
		DeepSeekAgentService = NewObject<UDNGDeepSeekAgentService>(this);
	}

	if (DeepSeekAgentService)
	{
		if (const UDNGGameInstance* DNGGameInstance = Cast<UDNGGameInstance>(GetGameInstance()))
		{
			DeepSeekAgentService->Configure(DNGGameInstance->GetDeepSeekConfig());
		}
	}
}

// Applies an asynchronous DeepSeek result and schedules playback if valid.
void ADNGPlayerController::HandleAgentPlanResult(bool bSuccess, const FDNGDeepSeekDrawingPlan& Plan, const FString& ErrorMessage)
{
	bAgentRequestInFlight = false;

	if (!bSuccess)
	{
		AgentStatusMessage = ErrorMessage;
		return;
	}

	const FString NormalizedAction = Plan.Action.TrimStartAndEnd().ToLower();
	if (NormalizedAction == TEXT("noop"))
	{
		LastAgentRawResponse = Plan.RawModelContent;
		AgentStatusMessage = Plan.Summary.IsEmpty() ? TEXT("Agent ignored the instruction because it was not a drawing request.") : Plan.Summary;
		return;
	}

	FinalizeAgentPlan(Plan);
}

// Stores the accepted final SVG and schedules its playback.
void ADNGPlayerController::FinalizeAgentPlan(const FDNGDeepSeekDrawingPlan& Plan)
{
	LastAgentSvg = Plan.Svg;
	LastAgentRawResponse = Plan.RawModelContent;
	UE_LOG(LogTemp, Log, TEXT("DeepSeek parsed action=%s summary=%s svg_length=%d"), *Plan.Action, *Plan.Summary, Plan.Svg.Len());
	QueueAgentSvgPlayback(Plan);
}

// Builds a board-derived SVG snapshot so agent revisions operate on the actual visible board.
bool ADNGPlayerController::BuildCurrentBoardSvgContext(FString& OutSvgContext) const
{
	OutSvgContext.Reset();

	if (const ADNGBoardActor* BoardActor = GetBoardActor())
	{
		return BoardActor->BuildBoardSvgSnapshot(OutSvgContext);
	}

	return false;
}

// Builds an erase-then-draw playback list that preserves identical board regions.
void ADNGPlayerController::BuildAgentDiffPlaybackSegments(const TArray<FDNGDrawSegment>& CurrentSegments, const TArray<FDNGDrawSegment>& TargetSegments, TArray<FDNGDrawSegment>& OutPlaybackSegments) const
{
	OutPlaybackSegments.Reset();

	if (CurrentSegments.Num() == 0)
	{
		OutPlaybackSegments = TargetSegments;
		return;
	}

	TArray<uint8> CurrentRaster;
	TArray<uint8> TargetRaster;
	RasterizeSegments(CurrentSegments, CurrentRaster);
	RasterizeSegments(TargetSegments, TargetRaster);

	TArray<uint8> RemoveMask;
	TArray<uint8> AddMask;
	RemoveMask.Init(0, CurrentRaster.Num());
	AddMask.Init(0, CurrentRaster.Num());

	for (int32 Index = 0; Index < CurrentRaster.Num(); ++Index)
	{
		const uint8 CurrentValue = CurrentRaster[Index];
		const uint8 TargetValue = TargetRaster[Index];
		if (CurrentValue != 0 && CurrentValue != TargetValue)
		{
			RemoveMask[Index] = 1;
		}

		if (TargetValue != 0 && TargetValue != CurrentValue)
		{
			AddMask[Index] = 1;
		}
	}

	BuildEraserSegmentsFromMask(RemoveMask, OutPlaybackSegments);

	for (const FDNGDrawSegment& Segment : TargetSegments)
	{
		if (SegmentTouchesChangeMask(Segment, AddMask))
		{
			OutPlaybackSegments.Add(Segment);
		}
	}
}

// Parses the final SVG into sampled board segments, computes the board delta, and starts timed playback.
void ADNGPlayerController::QueueAgentSvgPlayback(const FDNGDeepSeekDrawingPlan& Plan)
{
	QueuedAgentSegments.Reset();
	NextAgentSegmentIndex = 0;

	FDNGSvgParseOptions ParseOptions;
	ParseOptions.SmallThickness = 4.0f;
	ParseOptions.MediumThickness = 7.0f;
	ParseOptions.LargeThickness = 12.0f;
	ParseOptions.MaxSegmentLength = MaxSegmentLength;

	FString ParseError;
	if (!FDNGSvgParser::ParseSvgToSegments(Plan.Svg, ParseOptions, QueuedAgentSegments, ParseError))
	{
		AgentStatusMessage = FString::Printf(TEXT("Agent SVG parse failed: %s"), *ParseError);
		return;
	}

	if (QueuedAgentSegments.Num() == 0)
	{
		AgentStatusMessage = TEXT("DeepSeek returned SVG, but it did not produce any usable stroke segments.");
		return;
	}

	TArray<FDNGDrawSegment> CurrentSegments;
	if (const ADNGBoardActor* BoardActor = GetBoardActor())
	{
		BoardActor->GetVisibleSegments(CurrentSegments);
	}

	TArray<FDNGDrawSegment> PlaybackSegments;
	BuildAgentDiffPlaybackSegments(CurrentSegments, QueuedAgentSegments, PlaybackSegments);
	QueuedAgentSegments = MoveTemp(PlaybackSegments);

	if (QueuedAgentSegments.Num() == 0)
	{
		AgentStatusMessage = TEXT("Agent revision produced no visible board changes.");
		return;
	}

	AgentStatusMessage = FString::Printf(
		TEXT("Agent SVG ready: %s (%d delta segments queued)"),
		Plan.Summary.IsEmpty() ? TEXT("drawing") : *Plan.Summary,
		QueuedAgentSegments.Num());

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AgentPlaybackTimerHandle);
		World->GetTimerManager().SetTimer(AgentPlaybackTimerHandle, this, &ADNGPlayerController::PlayNextAgentSegment, AgentPlaybackInterval, true);
	}
}

// Plays one queued agent-generated segment per timer tick.
void ADNGPlayerController::PlayNextAgentSegment()
{
	if (NextAgentSegmentIndex >= QueuedAgentSegments.Num())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AgentPlaybackTimerHandle);
		}

		QueuedAgentSegments.Reset();
		NextAgentSegmentIndex = 0;
		AgentStatusMessage = TEXT("Agent drawing playback finished.");
		return;
	}

	EmitResolvedSegment(QueuedAgentSegments[NextAgentSegmentIndex]);
	++NextAgentSegmentIndex;
}

// Resets local agent state whenever the authoritative round number changes.
void ADNGPlayerController::RefreshAgentRoundState()
{
	const ADNGGameState* DNGGameState = GetDNGGameState();
	if (!DNGGameState)
	{
		return;
	}

	const int32 CurrentRoundNumber = DNGGameState->GetRoundNumber();
	if (LastObservedRoundNumber == INDEX_NONE)
	{
		LastObservedRoundNumber = CurrentRoundNumber;
		return;
	}

	if (CurrentRoundNumber != LastObservedRoundNumber)
	{
		LastObservedRoundNumber = CurrentRoundNumber;
		ResetAgentSession();
		AgentStatusMessage = TEXT("Agent session reset for the new round.");
	}
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
