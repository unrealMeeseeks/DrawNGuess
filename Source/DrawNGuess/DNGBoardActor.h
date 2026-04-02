#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DNGTypes.h"
#include "DNGBoardActor.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

UCLASS()
class DRAWNGUESS_API ADNGBoardActor : public AActor
{
	GENERATED_BODY()

public:
	ADNGBoardActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable)
	bool ProjectWorldToBoard(const FVector& WorldLocation, FVector2D& OutBoardPoint) const;

	UFUNCTION(BlueprintCallable)
	FVector BoardToWorld(const FVector2D& BoardPoint) const;

	const FVector2D& GetBoardSize() const { return BoardSize; }

	UFUNCTION(BlueprintCallable)
	UStaticMeshComponent* GetBoardMesh() const { return BoardMesh; }

	UFUNCTION(BlueprintCallable)
	UTextureRenderTarget2D* GetBoardRenderTarget() const { return BoardRenderTarget; }

	UFUNCTION(BlueprintCallable)
	FVector2D UVToPixel(const FVector2D& UV) const;

	UFUNCTION(BlueprintCallable)
	FVector2D BoardPointToUV(const FVector2D& BoardPoint) const;

	UFUNCTION(BlueprintCallable)
	void RefreshBoardVisuals();

	UFUNCTION(BlueprintCallable)
	bool SaveBoardImage(FString& OutSavedPath);

	void AddSegment(const FDNGDrawSegment& Segment);
	void AddPredictedSegment(const FDNGDrawSegment& Segment);
	void ClearBoard();

private:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Segments();

	void EnsureBoardSurface();
	void ResetBoardSurface();
	void SyncReplicatedSegments();
	void DrawSegmentToBoard(const FDNGDrawSegment& Segment);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BoardMesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> BoardCamera = nullptr;

	UPROPERTY(EditDefaultsOnly)
	FVector2D BoardSize = FVector2D(1200.0f, 800.0f);

	UPROPERTY(ReplicatedUsing = OnRep_Segments)
	TArray<FDNGDrawSegment> Segments;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> BoardRenderTarget = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoardMaterialInstance = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TObjectPtr<UMaterialInterface> BoardSurfaceMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FLinearColor PencilColor = FLinearColor::Black;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FLinearColor EraserColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FLinearColor BoardClearColor = FLinearColor::White;

	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FIntPoint RenderTargetSize = FIntPoint(2048, 2048);

	int32 AppliedReplicatedSegmentCount = 0;
	TArray<FDNGDrawSegment> PendingPredictedSegments;
};
