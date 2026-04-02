#include "DNGGameState.h"

#include "DNGBoardActor.h"
#include "DNGPlayerState.h"
#include "Net/UnrealNetwork.h"

ADNGGameState::ADNGGameState()
{
	bReplicates = true;
}

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

bool ADNGGameState::IsPainter(const APlayerState* PlayerState) const
{
	return PlayerState != nullptr && PlayerState == PainterPlayerState;
}

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
