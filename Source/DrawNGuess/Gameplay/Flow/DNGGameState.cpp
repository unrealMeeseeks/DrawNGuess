#include "DNGGameState.h"

#include "../Board/DNGBoardActor.h"
#include "../Player/DNGPlayerState.h"
#include "Net/UnrealNetwork.h"

// GameState exists on server and clients, so it only stores replicated state.
ADNGGameState::ADNGGameState()
{
	bReplicates = true;
}

// Registers all fields consumed by player controllers and widgets.
void ADNGGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADNGGameState, MatchPhase);
	DOREPLIFETIME(ADNGGameState, RoundNumber);
	DOREPLIFETIME(ADNGGameState, PainterPlayerState);
	DOREPLIFETIME(ADNGGameState, BoardActor);
	DOREPLIFETIME(ADNGGameState, PromptSettings);
	DOREPLIFETIME(ADNGGameState, LastRoundResult);
}

// Returns whether the supplied player is the currently assigned painter.
bool ADNGGameState::IsPainter(const APlayerState* PlayerState) const
{
	return PlayerState != nullptr && PlayerState == PainterPlayerState;
}

// The setters below intentionally remain simple because GameMode is authoritative.
void ADNGGameState::SetMatchPhase(EDNGMatchPhase InPhase)
{
	MatchPhase = InPhase;
}

void ADNGGameState::SetPainterPlayerState(ADNGPlayerState* InPainterPlayerState)
{
	PainterPlayerState = InPainterPlayerState;
}

void ADNGGameState::SetBoardActor(ADNGBoardActor* InBoardActor)
{
	BoardActor = InBoardActor;
}

void ADNGGameState::SetPromptSettings(const FDNGPromptSettings& InPromptSettings)
{
	PromptSettings = InPromptSettings;
}

void ADNGGameState::SetLastRoundResult(const FDNGRoundResult& InRoundResult)
{
	LastRoundResult = InRoundResult;
}

void ADNGGameState::SetRoundNumber(int32 InRoundNumber)
{
	RoundNumber = InRoundNumber;
}
