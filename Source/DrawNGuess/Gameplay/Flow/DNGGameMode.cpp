#include "DNGGameMode.h"

#include "../../AI/CLIP/DNGClipScorer.h"
#include "../Board/DNGBoardActor.h"
#include "DNGGameInstance.h"
#include "DNGGameState.h"
#include "../Player/DNGPlayerController.h"
#include "../Player/DNGPlayerState.h"
#include "EngineUtils.h"

// Sets the authoritative gameplay classes used by the prototype.
ADNGGameMode::ADNGGameMode()
{
	GameStateClass = ADNGGameState::StaticClass();
	PlayerControllerClass = ADNGPlayerController::StaticClass();
	PlayerStateClass = ADNGPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	BoardActorClass = ADNGBoardActor::StaticClass();
}

// Initializes the board, scoring backend, replicated settings, and initial round state.
void ADNGGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureBoardActor();
	InitializeClipScorer();
	ApplyPromptSettings();
	EnterLobby();
	RefreshBoardViewTargets();
	TryStartRound();
}

// Re-checks round startup conditions whenever a player joins.
void ADNGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	RefreshBoardViewTargets();
	TryStartRound();
}

// Returns the match to a clean lobby state whenever a player leaves.
void ADNGGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (SpawnedBoardActor)
	{
		SpawnedBoardActor->ClearBoard();
	}

	EnterLobby();
	TryStartRound();
}

// Validates painter-only drawing input and appends it to the shared board.
void ADNGGameMode::HandleDrawSegment(ADNGPlayerController* RequestingController, const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool)
{
	ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	if (!DNGGameState || DNGGameState->GetMatchPhase() != EDNGMatchPhase::Drawing || !SpawnedBoardActor)
	{
		return;
	}

	ADNGPlayerState* RequestingPlayerState = RequestingController ? RequestingController->GetPlayerState<ADNGPlayerState>() : nullptr;
	if (!RequestingPlayerState || !DNGGameState->IsPainter(RequestingPlayerState))
	{
		return;
	}

	FDNGDrawSegment Segment;
	Segment.Start = Start;
	Segment.End = End;
	Segment.Tool = Tool;
	Segment.Thickness = Tool == EDNGDrawTool::Eraser ? 18.0f : 7.0f;
	SpawnedBoardActor->AddSegment(Segment);
}

// Transitions from drawing to guessing when the active painter finishes.
void ADNGGameMode::HandleFinishDrawing(ADNGPlayerController* RequestingController)
{
	ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	if (!DNGGameState || DNGGameState->GetMatchPhase() != EDNGMatchPhase::Drawing)
	{
		return;
	}

	ADNGPlayerState* RequestingPlayerState = RequestingController ? RequestingController->GetPlayerState<ADNGPlayerState>() : nullptr;
	if (!RequestingPlayerState || !DNGGameState->IsPainter(RequestingPlayerState))
	{
		return;
	}

	DNGGameState->SetMatchPhase(EDNGMatchPhase::Guessing);
}

// Scores the submitted guess and publishes the round result.
void ADNGGameMode::HandleSubmitGuess(ADNGPlayerController* RequestingController, const FString& GuessText)
{
	ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	if (!DNGGameState || DNGGameState->GetMatchPhase() != EDNGMatchPhase::Guessing)
	{
		return;
	}

	ADNGPlayerState* RequestingPlayerState = RequestingController ? RequestingController->GetPlayerState<ADNGPlayerState>() : nullptr;
	if (!RequestingPlayerState || DNGGameState->IsPainter(RequestingPlayerState))
	{
		return;
	}

	const FDNGPromptSettings PromptSettings = DNGGameState->GetPromptSettings();
	const FString SanitizedGuess = GuessText.TrimStartAndEnd();
	FDNGRoundResult Result = BuildFallbackRoundResult(SanitizedGuess, PromptSettings);
	if (bPreferClipScoring)
	{
		TryPopulateClipRoundResult(Result);
	}

	if (Result.bGuessAccepted)
	{
		RequestingPlayerState->AddRoundScore(1);
	}

	DNGGameState->SetLastRoundResult(Result);
	DNGGameState->SetMatchPhase(EDNGMatchPhase::Results);
}

// Starts a new round only when the request is valid for the current result phase.
void ADNGGameMode::HandleNextRound(ADNGPlayerController* RequestingController)
{
	if (!RequestingController || !RequestingController->HasAuthority())
	{
		return;
	}

	ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	if (!DNGGameState || DNGGameState->GetMatchPhase() != EDNGMatchPhase::Results)
	{
		return;
	}

	TryStartRound();
}

// Reuses a placed board actor when available, otherwise spawns a fallback board.
void ADNGGameMode::EnsureBoardActor()
{
	if (SpawnedBoardActor)
	{
		return;
	}

	for (TActorIterator<ADNGBoardActor> It(GetWorld()); It; ++It)
	{
		if (ADNGBoardActor* ExistingBoardActor = *It)
		{
			SpawnedBoardActor = ExistingBoardActor;
			break;
		}
	}

	if (!SpawnedBoardActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* ResolvedBoardActorClass = BoardActorClass ? BoardActorClass.Get() : ADNGBoardActor::StaticClass();
		SpawnedBoardActor = GetWorld()->SpawnActor<ADNGBoardActor>(ResolvedBoardActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	}

	if (ADNGGameState* DNGGameState = GetGameState<ADNGGameState>())
	{
		DNGGameState->SetBoardActor(SpawnedBoardActor);
	}
}

// Builds and initializes the CLIP scorer only when the feature is enabled.
void ADNGGameMode::InitializeClipScorer()
{
	if (ClipScorer)
	{
		return;
	}

	ClipScorer = NewObject<UDNGClipScorer>(this);
	if (ClipScorer)
	{
		ClipScorer->Configure(ClipRuntimeName, ClipImageEncoderModel, ClipTextEncoderModel, ClipTokenizerDirectory);
	}
}

// Copies host-owned prompt settings into the replicated GameState payload.
void ADNGGameMode::ApplyPromptSettings()
{
	ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	UDNGGameInstance* DNGGameInstance = Cast<UDNGGameInstance>(GetGameInstance());
	if (!DNGGameState || !DNGGameInstance)
	{
		return;
	}

	DNGGameState->SetPromptSettings(DNGGameInstance->GetPromptSettings());
}

// Starts a round once enough players are connected and the lobby is ready.
void ADNGGameMode::TryStartRound()
{
	if (const ADNGGameState* DNGGameState = GetGameState<ADNGGameState>())
	{
		const EDNGMatchPhase CurrentPhase = DNGGameState->GetMatchPhase();
		if (CurrentPhase != EDNGMatchPhase::Lobby && CurrentPhase != EDNGMatchPhase::Results)
		{
			return;
		}
	}

	if (GetOrderedPlayers().Num() < 2)
	{
		EnterLobby();
		return;
	}

	StartRound();
}

// Performs the full per-round reset, painter assignment, and state replication.
void ADNGGameMode::StartRound()
{
	ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	if (!DNGGameState)
	{
		return;
	}

	const TArray<ADNGPlayerState*> Players = GetOrderedPlayers();
	if (Players.Num() < 2)
	{
		EnterLobby();
		return;
	}

	const int32 NextRound = DNGGameState->GetRoundNumber() + 1;
	DNGGameState->SetRoundNumber(NextRound);
	DNGGameState->SetLastRoundResult(FDNGRoundResult());

	if (SpawnedBoardActor)
	{
		SpawnedBoardActor->ClearBoard();
	}

	ADNGPlayerState* PainterPlayerState = ResolvePainterForRound(Players);
	for (ADNGPlayerState* PlayerState : Players)
	{
		if (PlayerState)
		{
			PlayerState->SetPainter(PlayerState == PainterPlayerState);
		}
	}

	DNGGameState->SetPainterPlayerState(PainterPlayerState);
	DNGGameState->SetMatchPhase(EDNGMatchPhase::Drawing);
}

// Resets the shared match state to its lobby defaults.
void ADNGGameMode::EnterLobby()
{
	if (ADNGGameState* DNGGameState = GetGameState<ADNGGameState>())
	{
		DNGGameState->SetMatchPhase(EDNGMatchPhase::Lobby);
		DNGGameState->SetPainterPlayerState(nullptr);
		DNGGameState->SetLastRoundResult(FDNGRoundResult());
	}

	for (ADNGPlayerState* PlayerState : GetOrderedPlayers())
	{
		if (PlayerState)
		{
			PlayerState->SetPainter(false);
		}
	}
}

// Re-locks every connected player controller to the board camera.
void ADNGGameMode::RefreshBoardViewTargets() const
{
	if (!SpawnedBoardActor)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PlayerController = It->Get())
		{
			PlayerController->SetViewTarget(SpawnedBoardActor);
			PlayerController->bShowMouseCursor = true;
		}
	}
}

// Produces a deterministic player order so painter rotation stays stable.
TArray<ADNGPlayerState*> ADNGGameMode::GetOrderedPlayers() const
{
	TArray<ADNGPlayerState*> Result;
	if (const ADNGGameState* DNGGameState = GetGameState<ADNGGameState>())
	{
		for (APlayerState* PlayerState : DNGGameState->PlayerArray)
		{
			if (ADNGPlayerState* DNGPlayerState = Cast<ADNGPlayerState>(PlayerState))
			{
				Result.Add(DNGPlayerState);
			}
		}
	}

	Result.Sort([](const ADNGPlayerState& Left, const ADNGPlayerState& Right)
	{
		return Left.GetPlayerId() < Right.GetPlayerId();
	});

	return Result;
}

// Chooses the painter by rotating through the ordered player list.
ADNGPlayerState* ADNGGameMode::ResolvePainterForRound(const TArray<ADNGPlayerState*>& Players) const
{
	if (Players.Num() == 0)
	{
		return nullptr;
	}

	const ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	const int32 RoundIndex = DNGGameState ? FMath::Max(0, DNGGameState->GetRoundNumber() - 1) : 0;
	return Players[RoundIndex % Players.Num()];
}

// Produces a stable mock result so gameplay remains testable without CLIP.
FDNGRoundResult ADNGGameMode::BuildFallbackRoundResult(const FString& GuessText, const FDNGPromptSettings& PromptSettings) const
{
	// The fallback path is seeded deterministically so repeated tests stay stable.
	const ADNGGameState* DNGGameState = GetGameState<ADNGGameState>();
	const int32 Seed = GetTypeHash(GuessText) ^ ((DNGGameState ? DNGGameState->GetRoundNumber() : 0) * 7919);
	FRandomStream Stream(Seed);

	FDNGRoundResult Result;
	Result.GuessText = GuessText;
	Result.PositivePrompt = PromptSettings.PositivePrefix.Replace(TEXT("{}"), *GuessText);
	Result.NegativePrompt = PromptSettings.NegativePrefix.Replace(TEXT("{}"), *GuessText);
	Result.PositiveScore = GuessText.IsEmpty() ? 0.0f : Stream.FRandRange(0.45f, 0.98f);
	Result.NegativeScore = Stream.FRandRange(0.05f, 0.85f);
	Result.bGuessAccepted = !GuessText.IsEmpty() && Result.PositiveScore > Result.NegativeScore;
	ApplySoftmaxProbabilities(Result, 1.0f);
	Result.ScoringBackend = TEXT("Mock");
	Result.DiagnosticMessage = TEXT("Using placeholder scores until CLIP scoring succeeds.");
	return Result;
}

// Converts raw cosine scores into a two-class softmax probability pair.
void ADNGGameMode::ApplySoftmaxProbabilities(FDNGRoundResult& InOutResult, float LogitScale) const
{
	const double PositiveLogit = static_cast<double>(InOutResult.PositiveScore) * static_cast<double>(LogitScale);
	const double NegativeLogit = static_cast<double>(InOutResult.NegativeScore) * static_cast<double>(LogitScale);
	const double MaxLogit = FMath::Max(PositiveLogit, NegativeLogit);

	const double PositiveExp = FMath::Exp(PositiveLogit - MaxLogit);
	const double NegativeExp = FMath::Exp(NegativeLogit - MaxLogit);
	const double SumExp = PositiveExp + NegativeExp;

	if (SumExp <= UE_DOUBLE_SMALL_NUMBER)
	{
		InOutResult.PositiveProbability = 0.5f;
		InOutResult.NegativeProbability = 0.5f;
		return;
	}

	InOutResult.PositiveProbability = static_cast<float>(PositiveExp / SumExp);
	InOutResult.NegativeProbability = static_cast<float>(NegativeExp / SumExp);
}

// Exports the authoritative board image and asks CLIP to score both prompts.
bool ADNGGameMode::TryPopulateClipRoundResult(FDNGRoundResult& InOutResult)
{
	if (!ClipScorer || !SpawnedBoardActor)
	{
		return false;
	}

	FString SavedBoardPath;
	if (!SpawnedBoardActor->SaveBoardImage(SavedBoardPath))
	{
		InOutResult.DiagnosticMessage = TEXT("CLIP probe skipped: failed to export the board image on the server.");
		return false;
	}

	InOutResult.SavedBoardPath = SavedBoardPath;

	FString ErrorMessage;
	if (!ClipScorer->Initialize(ErrorMessage))
	{
		InOutResult.DiagnosticMessage = FString::Printf(TEXT("CLIP initialization failed: %s"), *ErrorMessage);
		return false;
	}

	TArray<float> ImageEmbedding;
	if (!ClipScorer->EncodeImageFile(SavedBoardPath, ImageEmbedding, ErrorMessage))
	{
		InOutResult.DiagnosticMessage = FString::Printf(TEXT("CLIP image encoder failed: %s"), *ErrorMessage);
		return false;
	}

	float PositiveScore = 0.0f;
	float NegativeScore = 0.0f;
	FString PositiveError;
	FString NegativeError;
	const bool bPositiveScored = ClipScorer->ScoreImageAgainstText(SavedBoardPath, InOutResult.PositivePrompt, PositiveScore, PositiveError);
	const bool bNegativeScored = ClipScorer->ScoreImageAgainstText(SavedBoardPath, InOutResult.NegativePrompt, NegativeScore, NegativeError);

	if (!bPositiveScored || !bNegativeScored)
	{
		InOutResult.ScoringBackend = TEXT("Mock + CLIP image probe");
		InOutResult.DiagnosticMessage = FString::Printf(
			TEXT("CLIP image encoder is working (%d dims), but text scoring is not ready. Positive: %s Negative: %s"),
			ImageEmbedding.Num(),
			bPositiveScored ? TEXT("ok") : *PositiveError,
			bNegativeScored ? TEXT("ok") : *NegativeError);
		return false;
	}

	InOutResult.PositiveScore = PositiveScore;
	InOutResult.NegativeScore = NegativeScore;
	ApplySoftmaxProbabilities(InOutResult, ClipLogitScale);
	InOutResult.bGuessAccepted = !InOutResult.GuessText.IsEmpty() && PositiveScore > NegativeScore;
	InOutResult.ScoringBackend = TEXT("CLIP");
	InOutResult.DiagnosticMessage = FString::Printf(TEXT("CLIP image/text encoders produced %d-d embeddings. Softmax scale=%.2f"), ImageEmbedding.Num(), ClipLogitScale);
	return true;
}
