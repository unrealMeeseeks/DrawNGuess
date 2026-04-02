#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DNGTypes.h"
#include "DNGGameState.generated.h"

class ADNGBoardActor;
class ADNGPlayerState;

UCLASS()
class DRAWNGUESS_API ADNGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADNGGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable)
	EDNGMatchPhase GetMatchPhase() const { return MatchPhase; }

	UFUNCTION(BlueprintCallable)
	int32 GetRoundNumber() const { return RoundNumber; }

	UFUNCTION(BlueprintCallable)
	ADNGPlayerState* GetPainterPlayerState() const { return PainterPlayerState; }

	UFUNCTION(BlueprintCallable)
	ADNGBoardActor* GetBoardActor() const { return BoardActor; }

	const FDNGPromptSettings& GetPromptSettings() const { return PromptSettings; }

	const FDNGRoundResult& GetLastRoundResult() const { return LastRoundResult; }

	bool IsPainter(const APlayerState* PlayerState) const;

	void SetMatchPhase(EDNGMatchPhase InPhase);
	void SetPainterPlayerState(ADNGPlayerState* InPainterPlayerState);
	void SetBoardActor(ADNGBoardActor* InBoardActor);
	void SetPromptSettings(const FDNGPromptSettings& InPromptSettings);
	void SetLastRoundResult(const FDNGRoundResult& InRoundResult);
	void SetRoundNumber(int32 InRoundNumber);

private:
	UPROPERTY(Replicated)
	EDNGMatchPhase MatchPhase = EDNGMatchPhase::Lobby;

	UPROPERTY(Replicated)
	int32 RoundNumber = 0;

	UPROPERTY(Replicated)
	TObjectPtr<ADNGPlayerState> PainterPlayerState = nullptr;

	UPROPERTY(Replicated)
	TObjectPtr<ADNGBoardActor> BoardActor = nullptr;

	UPROPERTY(Replicated)
	FDNGPromptSettings PromptSettings;

	UPROPERTY(Replicated)
	FDNGRoundResult LastRoundResult;
};
