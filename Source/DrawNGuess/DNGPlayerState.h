#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DNGPlayerState.generated.h"

UCLASS()
class DRAWNGUESS_API ADNGPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ADNGPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable)
	bool IsPainter() const { return bIsPainter; }

	UFUNCTION(BlueprintCallable)
	int32 GetRoundScore() const { return RoundScore; }

	void SetPainter(bool bInPainter);
	void AddRoundScore(int32 Delta);

private:
	UPROPERTY(Replicated)
	bool bIsPainter = false;

	UPROPERTY(Replicated)
	int32 RoundScore = 0;
};
