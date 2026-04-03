#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "../../Core/DNGTypes.h"
#include "DNGGameMode.generated.h"

class ADNGBoardActor;
class ADNGGameState;
class ADNGPlayerController;
class ADNGPlayerState;
class UDNGClipScorer;
class UNNEModelData;

// Server-authoritative coordinator for the full game loop.
UCLASS()
class DRAWNGUESS_API ADNGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADNGGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// Applies one validated segment to the shared board.
	void HandleDrawSegment(ADNGPlayerController* RequestingController, const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool);

	// Finishes the drawing phase and enters the guessing phase.
	void HandleFinishDrawing(ADNGPlayerController* RequestingController);

	// Scores the guess and transitions into the result phase.
	void HandleSubmitGuess(ADNGPlayerController* RequestingController, const FString& GuessText);

	// Starts the next round from the result phase.
	void HandleNextRound(ADNGPlayerController* RequestingController);

private:
	// Finds a board already placed in the map or spawns a fallback instance.
	void EnsureBoardActor();

	// Configures and initializes the CLIP scoring service when available.
	void InitializeClipScorer();

	// Copies host prompt settings into the replicated GameState.
	void ApplyPromptSettings();

	// Starts a round when enough players are connected.
	void TryStartRound();

	// Performs the actual round initialization and role assignment.
	void StartRound();

	// Returns the match to the lobby state.
	void EnterLobby();

	// Re-locks all player cameras to the board actor.
	void RefreshBoardViewTargets() const;

	// Returns players in a stable order for painter rotation.
	TArray<ADNGPlayerState*> GetOrderedPlayers() const;

	// Selects which connected player becomes the painter this round.
	ADNGPlayerState* ResolvePainterForRound(const TArray<ADNGPlayerState*>& Players) const;

	// Produces a deterministic fallback result if CLIP is unavailable.
	FDNGRoundResult BuildFallbackRoundResult(const FString& GuessText, const FDNGPromptSettings& PromptSettings) const;

	// Converts cosine scores into softmax probabilities for the UI.
	void ApplySoftmaxProbabilities(FDNGRoundResult& InOutResult, float LogitScale) const;

	// Replaces the fallback result with real CLIP output when possible.
	bool TryPopulateClipRoundResult(FDNGRoundResult& InOutResult);

	UPROPERTY()
	TObjectPtr<ADNGBoardActor> SpawnedBoardActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDNGClipScorer> ClipScorer = nullptr;

	// Board subclass used when the map does not already contain a board actor.
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ADNGBoardActor> BoardActorClass;

	// Enables CLIP scoring whenever the backend is configured.
	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	bool bPreferClipScoring = true;

	// Name of the NNE runtime requested for ONNX execution.
	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	FString ClipRuntimeName = TEXT("NNERuntimeORTCpu");

	// Image encoder model imported as UNNEModelData.
	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	TSoftObjectPtr<UNNEModelData> ClipImageEncoderModel;

	// Text encoder model imported as UNNEModelData.
	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	TSoftObjectPtr<UNNEModelData> ClipTextEncoderModel;

	// Directory containing CLIP tokenizer assets such as vocab.json and merges.txt.
	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	FString ClipTokenizerDirectory = TEXT("../CLIP");

	// Scale applied before softmax so result probabilities are easier to read.
	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	float ClipLogitScale = 100.0f;
};
