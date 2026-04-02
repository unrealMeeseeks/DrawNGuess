#include "DNGGameMode.h"

#include "DNGBoardActor.h"
#include "DNGGameInstance.h"
#include "DNGGameState.h"
#include "DNGPlayerController.h"
#include "DNGPlayerState.h"
#include "EngineUtils.h"

ADNGGameMode::ADNGGameMode()
{
	GameStateClass = ADNGGameState::StaticClass();
	PlayerControllerClass = ADNGPlayerController::StaticClass();
	PlayerStateClass = ADNGPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	BoardActorClass = ADNGBoardActor::StaticClass();
}

void ADNGGameMode::BeginPlay()
{
	Super::BeginPlay();

	EnsureBoardActor();
	ApplyPromptSettings();
	EnterLobby();
	RefreshBoardViewTargets();
	TryStartRound();
}

void ADNGGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	RefreshBoardViewTargets();
	TryStartRound();
}

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
	const int32 Seed = GetTypeHash(SanitizedGuess) ^ (DNGGameState->GetRoundNumber() * 7919);
	FRandomStream Stream(Seed);

	FDNGRoundResult Result;
	Result.GuessText = SanitizedGuess;
	Result.PositivePrompt = PromptSettings.PositivePrefix.Replace(TEXT("{}"), *SanitizedGuess);
	Result.NegativePrompt = PromptSettings.NegativePrefix.Replace(TEXT("{}"), *SanitizedGuess);
	Result.PositiveScore = SanitizedGuess.IsEmpty() ? 0.0f : Stream.FRandRange(0.45f, 0.98f);
	Result.NegativeScore = Stream.FRandRange(0.05f, 0.85f);
	Result.bGuessAccepted = !SanitizedGuess.IsEmpty() && Result.PositiveScore > Result.NegativeScore;

	if (Result.bGuessAccepted)
	{
		RequestingPlayerState->AddRoundScore(1);
	}

	DNGGameState->SetLastRoundResult(Result);
	DNGGameState->SetMatchPhase(EDNGMatchPhase::Results);
}

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
