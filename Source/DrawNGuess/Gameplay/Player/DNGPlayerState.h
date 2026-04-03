#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DNGPlayerState.generated.h"

// Minimal replicated player payload used by the round flow.
UCLASS()
class DRAWNGUESS_API ADNGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ADNGPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Returns whether this player is currently the painter.
	UFUNCTION(BlueprintCallable)
	bool IsPainter() const { return bIsPainter; }

	// Returns the replicated round score tracked for this player.
	UFUNCTION(BlueprintCallable)
	int32 GetRoundScore() const { return RoundScore; }

	// Setters used by the GameMode when roles or scores change.
	void SetPainter(bool bInPainter);
	void AddRoundScore(int32 Delta);

private:
	// Replicated painter flag.
	UPROPERTY(Replicated)
	bool bIsPainter = false;

	// Replicated per-round score.
	UPROPERTY(Replicated)
	int32 RoundScore = 0;
};
