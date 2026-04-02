#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "DNGTypes.h"
#include "DNGGameMode.generated.h"

class ADNGBoardActor;
class ADNGGameState;
class ADNGPlayerController;
class ADNGPlayerState;

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
	void ApplyPromptSettings();
	void TryStartRound();
	void StartRound();
	void EnterLobby();
	void RefreshBoardViewTargets() const;
	TArray<ADNGPlayerState*> GetOrderedPlayers() const;
	ADNGPlayerState* ResolvePainterForRound(const TArray<ADNGPlayerState*>& Players) const;

	UPROPERTY()
	TObjectPtr<ADNGBoardActor> SpawnedBoardActor = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TSubclassOf<ADNGBoardActor> BoardActorClass;
};
