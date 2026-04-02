#include "DNGPlayerController.h"

#include "DNGBoardActor.h"
#include "DNGGameMode.h"
#include "DNGGameState.h"
#include "DNGPlayerState.h"
#include "UI/DNGMainMenuWidget.h"
#include "UI/DNGMatchWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ADNGPlayerController::ADNGPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	PrimaryActorTick.bCanEverTick = true;
	MainMenuWidgetClass = UDNGMainMenuWidget::StaticClass();
	MatchWidgetClass = UDNGMatchWidget::StaticClass();
}

void ADNGPlayerController::BeginPlay()
{
	Super::BeginPlay();

	RefreshWidgets();
	ApplyInputMode();
}

void ADNGPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController() || !bPointerHeld || !CanUseDrawingControls())
	{
		return;
	}

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

void ADNGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	check(InputComponent);
	InputComponent->BindAction(TEXT("DrawPointer"), IE_Pressed, this, &ADNGPlayerController::HandleDrawPressed);
	InputComponent->BindAction(TEXT("DrawPointer"), IE_Released, this, &ADNGPlayerController::HandleDrawReleased);
}

void ADNGPlayerController::RequestSetTool(EDNGDrawTool NewTool)
{
	ActiveTool = NewTool;
}

void ADNGPlayerController::RequestFinishDrawing()
{
	if (CanUseDrawingControls())
	{
		ServerFinishDrawing();
	}
}

void ADNGPlayerController::RequestSubmitGuess(const FString& GuessText)
{
	if (CanSubmitGuess())
	{
		ServerSubmitGuess(GuessText);
	}
}

void ADNGPlayerController::RequestNextRound()
{
	if (CanAdvanceRound())
	{
		ServerNextRound();
	}
}

bool ADNGPlayerController::IsPainterLocal() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		return DNGGameState->IsPainter(PlayerState);
	}

	return false;
}

EDNGMatchPhase ADNGPlayerController::GetCurrentPhase() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		return DNGGameState->GetMatchPhase();
	}

	return EDNGMatchPhase::Lobby;
}

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

FString ADNGPlayerController::GetRoleDescription() const
{
	return IsPainterLocal() ? TEXT("Role: Painter") : TEXT("Role: Guesser");
}

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

FString ADNGPlayerController::GetPromptDescription() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		const FDNGPromptSettings& PromptSettings = DNGGameState->GetPromptSettings();
		return FString::Printf(TEXT("Positive prefix: %s\nNegative prefix: %s"), *PromptSettings.PositivePrefix, *PromptSettings.NegativePrefix);
	}

	return TEXT("Prompt settings not ready");
}

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
			TEXT("Guess: %s\nPositive: %s\nNegative: %s\nPositive score: %.3f\nNegative score: %.3f\nDecision: %s"),
			*Result.GuessText,
			*Result.PositivePrompt,
			*Result.NegativePrompt,
			Result.PositiveScore,
			Result.NegativeScore,
			Result.bGuessAccepted ? TEXT("Accepted") : TEXT("Rejected"));
	}

	return TEXT("No result yet");
}

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

bool ADNGPlayerController::CanUseDrawingControls() const
{
	return IsPainterLocal() && GetCurrentPhase() == EDNGMatchPhase::Drawing;
}

bool ADNGPlayerController::CanSubmitGuess() const
{
	return !IsPainterLocal() && GetCurrentPhase() == EDNGMatchPhase::Guessing;
}

bool ADNGPlayerController::CanAdvanceRound() const
{
	return HasAuthority() && GetCurrentPhase() == EDNGMatchPhase::Results;
}

void ADNGPlayerController::ServerAddDrawSegment_Implementation(const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool)
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleDrawSegment(this, Start, End, Tool);
	}
}

void ADNGPlayerController::ServerFinishDrawing_Implementation()
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleFinishDrawing(this);
	}
}

void ADNGPlayerController::ServerSubmitGuess_Implementation(const FString& GuessText)
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleSubmitGuess(this, GuessText);
	}
}

void ADNGPlayerController::ServerNextRound_Implementation()
{
	if (ADNGGameMode* DNGGameMode = Cast<ADNGGameMode>(GetWorld()->GetAuthGameMode()))
	{
		DNGGameMode->HandleNextRound(this);
	}
}

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

void ADNGPlayerController::HandleDrawPressed()
{
	bPointerHeld = true;
	bHasLastBoardPoint = false;
}

void ADNGPlayerController::HandleDrawReleased()
{
	bPointerHeld = false;
	bHasLastBoardPoint = false;
}

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

ADNGGameState* ADNGPlayerController::GetDNGGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ADNGGameState>() : nullptr;
}

ADNGBoardActor* ADNGPlayerController::GetBoardActor() const
{
	if (const ADNGGameState* DNGGameState = GetDNGGameState())
	{
		return DNGGameState->GetBoardActor();
	}

	return nullptr;
}
