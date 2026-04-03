#include "DNGPlayerState.h"

#include "Net/UnrealNetwork.h"

// PlayerState only carries the minimal replicated data needed by this prototype.
ADNGPlayerState::ADNGPlayerState()
{
	bReplicates = true;
}

// Registers the painter flag and per-round score for replication.
void ADNGPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADNGPlayerState, bIsPainter);
	DOREPLIFETIME(ADNGPlayerState, RoundScore);
}

// Sets whether this player is the active painter.
void ADNGPlayerState::SetPainter(bool bInPainter)
{
	bIsPainter = bInPainter;
}

// Adds round score and mirrors it into the base APlayerState score field.
void ADNGPlayerState::AddRoundScore(int32 Delta)
{
	RoundScore += Delta;
	SetScore(static_cast<float>(RoundScore));
}
