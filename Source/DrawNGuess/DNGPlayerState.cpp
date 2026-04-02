#include "DNGPlayerState.h"

#include "Net/UnrealNetwork.h"

ADNGPlayerState::ADNGPlayerState()
{
	bReplicates = true;
}

void ADNGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADNGPlayerState, bIsPainter);
	DOREPLIFETIME(ADNGPlayerState, RoundScore);
}

void ADNGPlayerState::SetPainter(bool bInPainter)
{
	bIsPainter = bInPainter;
}

void ADNGPlayerState::AddRoundScore(int32 Delta)
{
	RoundScore += Delta;
	SetScore(static_cast<float>(RoundScore));
}
