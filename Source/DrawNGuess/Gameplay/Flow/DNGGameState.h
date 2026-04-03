#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "../../Core/DNGTypes.h"
#include "DNGGameState.generated.h"

class ADNGBoardActor;
class ADNGPlayerState;

// Replicated match snapshot consumed by controllers and widgets.
UCLASS()
class DRAWNGUESS_API ADNGGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADNGGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Returns the current authoritative phase.
	UFUNCTION(BlueprintCallable)
	EDNGMatchPhase GetMatchPhase() const { return MatchPhase; }

	// Returns the active round number.
	UFUNCTION(BlueprintCallable)
	int32 GetRoundNumber() const { return RoundNumber; }

	// Returns the player currently assigned as painter.
	UFUNCTION(BlueprintCallable)
	ADNGPlayerState* GetPainterPlayerState() const { return PainterPlayerState; }

	// Returns the shared board actor.
	UFUNCTION(BlueprintCallable)
	ADNGBoardActor* GetBoardActor() const { return BoardActor; }

	// Returns the current prompt settings.
	const FDNGPromptSettings& GetPromptSettings() const { return PromptSettings; }

	// Returns the result payload of the most recent round.
	const FDNGRoundResult& GetLastRoundResult() const { return LastRoundResult; }

	// Convenience helper used to validate painter-only actions.
	bool IsPainter(const APlayerState* PlayerState) const;

	// Setters used by the GameMode when the authoritative state changes.
	void SetMatchPhase(EDNGMatchPhase InPhase);
	void SetPainterPlayerState(ADNGPlayerState* InPainterPlayerState);
	void SetBoardActor(ADNGBoardActor* InBoardActor);
	void SetPromptSettings(const FDNGPromptSettings& InPromptSettings);
	void SetLastRoundResult(const FDNGRoundResult& InRoundResult);
	void SetRoundNumber(int32 InRoundNumber);

private:
	// Current high-level state of the match loop.
	UPROPERTY(Replicated)
	EDNGMatchPhase MatchPhase = EDNGMatchPhase::Lobby;

	// Monotonic round counter.
	UPROPERTY(Replicated)
	int32 RoundNumber = 0;

	// Player currently allowed to draw.
	UPROPERTY(Replicated)
	TObjectPtr<ADNGPlayerState> PainterPlayerState = nullptr;

	// Shared board actor observed by all players.
	UPROPERTY(Replicated)
	TObjectPtr<ADNGBoardActor> BoardActor = nullptr;

	// Replicated prompt configuration supplied by the host.
	UPROPERTY(Replicated)
	FDNGPromptSettings PromptSettings;

	// Last round result used by the result screen.
	UPROPERTY(Replicated)
	FDNGRoundResult LastRoundResult;
};
