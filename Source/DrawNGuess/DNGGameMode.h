#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "UObject/SoftObjectPtr.h"
#include "DNGTypes.h"
#include "DNGGameMode.generated.h"

class ADNGBoardActor;
class ADNGGameState;
class ADNGPlayerController;
class ADNGPlayerState;
class UDNGClipScorer;
class UNNEModelData;

UCLASS()
class DRAWNGUESS_API ADNGGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADNGGameMode();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void HandleDrawSegment(ADNGPlayerController* RequestingController, const FVector2D& Start, const FVector2D& End, EDNGDrawTool Tool);
	void HandleFinishDrawing(ADNGPlayerController* RequestingController);
	void HandleSubmitGuess(ADNGPlayerController* RequestingController, const FString& GuessText);
	void HandleNextRound(ADNGPlayerController* RequestingController);

private:
	void EnsureBoardActor();
	void InitializeClipScorer();
	void ApplyPromptSettings();
	void TryStartRound();
	void StartRound();
	void EnterLobby();
	void RefreshBoardViewTargets() const;
	TArray<ADNGPlayerState*> GetOrderedPlayers() const;
	ADNGPlayerState* ResolvePainterForRound(const TArray<ADNGPlayerState*>& Players) const;
	FDNGRoundResult BuildFallbackRoundResult(const FString& GuessText, const FDNGPromptSettings& PromptSettings) const;
	void ApplySoftmaxProbabilities(FDNGRoundResult& InOutResult, float LogitScale) const;
	bool TryPopulateClipRoundResult(FDNGRoundResult& InOutResult);

	UPROPERTY()
	TObjectPtr<ADNGBoardActor> SpawnedBoardActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDNGClipScorer> ClipScorer = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ADNGBoardActor> BoardActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	bool bPreferClipScoring = true;

	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	FString ClipRuntimeName = TEXT("NNERuntimeORTCpu");

	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	TSoftObjectPtr<UNNEModelData> ClipImageEncoderModel;

	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	TSoftObjectPtr<UNNEModelData> ClipTextEncoderModel;

	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	FString ClipTokenizerDirectory = TEXT("../CLIP");

	UPROPERTY(EditDefaultsOnly, Category = "CLIP")
	float ClipLogitScale = 100.0f;
};
