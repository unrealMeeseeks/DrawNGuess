#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../../Core/DNGTypes.h"
#include "DNGBoardActor.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

// Shared board actor responsible for the visible drawing surface,
// replicated stroke history, fixed camera, and PNG export.
UCLASS()
class DRAWNGUESS_API ADNGBoardActor : public AActor
{
	GENERATED_BODY()

public:
	ADNGBoardActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Projects a world-space point onto the board plane.
	UFUNCTION(BlueprintCallable)
	bool ProjectWorldToBoard(const FVector& WorldLocation, FVector2D& OutBoardPoint) const;

	// Converts a logical board-space point into world space.
	UFUNCTION(BlueprintCallable)
	FVector BoardToWorld(const FVector2D& BoardPoint) const;

	// Returns the logical board size used by helper conversions.
	const FVector2D& GetBoardSize() const { return BoardSize; }

	// Exposes the board mesh for Blueprint-driven visual customization.
	UFUNCTION(BlueprintCallable)
	UStaticMeshComponent* GetBoardMesh() const { return BoardMesh; }

	// Returns the active render target that stores the painted board image.
	UFUNCTION(BlueprintCallable)
	UTextureRenderTarget2D* GetBoardRenderTarget() const { return BoardRenderTarget; }

	// Converts normalized UV coordinates into render-target pixel coordinates.
	UFUNCTION(BlueprintCallable)
	FVector2D UVToPixel(const FVector2D& UV) const;

	// Converts legacy board-space coordinates into normalized UV coordinates.
	UFUNCTION(BlueprintCallable)
	FVector2D BoardPointToUV(const FVector2D& BoardPoint) const;

	// Rebuilds the board texture from authoritative and predicted segments.
	UFUNCTION(BlueprintCallable)
	void RefreshBoardVisuals();

	// Saves the current board render target to disk as a PNG file.
	UFUNCTION(BlueprintCallable)
	bool SaveBoardImage(FString& OutSavedPath);

	// Builds an SVG snapshot from the current visible board content for agent revision context.
	UFUNCTION(BlueprintCallable)
	bool BuildBoardSvgSnapshot(FString& OutSvg) const;

	// Returns the locally visible segment history, including pending predicted segments.
	void GetVisibleSegments(TArray<FDNGDrawSegment>& OutSegments) const;

	// Adds an authoritative segment to the replicated board history.
	void AddSegment(const FDNGDrawSegment& Segment);

	// Adds a predicted local segment so the painter sees immediate feedback.
	void AddPredictedSegment(const FDNGDrawSegment& Segment);

	// Clears the board state and resets the render target.
	void ClearBoard();

private:
	virtual void BeginPlay() override;

	// Reacts to replicated segment updates from the server.
	UFUNCTION()
	void OnRep_Segments();

	// Lazily creates the render target and dynamic material instance.
	void EnsureBoardSurface();

	// Clears the render target to the configured board color.
	void ResetBoardSurface();

	// Applies only the newly replicated segments to the local render target.
	void SyncReplicatedSegments();

	// Draws one segment onto the render target.
	void DrawSegmentToBoard(const FDNGDrawSegment& Segment);

	// Visual surface used to display the board material.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BoardMesh = nullptr;

	// Fixed orthographic camera that player controllers lock onto.
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> BoardCamera = nullptr;

	// Logical board size used for framing and coordinate helpers.
	UPROPERTY(EditDefaultsOnly)
	FVector2D BoardSize = FVector2D(1200.0f, 800.0f);

	// Full authoritative stroke history replicated from the server.
	UPROPERTY(ReplicatedUsing = OnRep_Segments)
	TArray<FDNGDrawSegment> Segments;

	// Runtime render target that stores the current board image.
	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> BoardRenderTarget = nullptr;

	// Dynamic material instance bound to the board mesh.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoardMaterialInstance = nullptr;

	// Optional source material used to create the dynamic board material.
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	TObjectPtr<UMaterialInterface> BoardSurfaceMaterial = nullptr;

	// Brush color for normal drawing.
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FLinearColor PencilColor = FLinearColor::Black;

	// Brush color used by the eraser tool.
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FLinearColor EraserColor = FLinearColor::White;

	// Clear color applied when the board is reset.
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FLinearColor BoardClearColor = FLinearColor::White;

	// Resolution of the board render target.
	UPROPERTY(EditDefaultsOnly, Category = "Board")
	FIntPoint RenderTargetSize = FIntPoint(2048, 2048);

	// Number of replicated segments already drawn into the local texture.
	int32 AppliedReplicatedSegmentCount = 0;

	// Local predicted segments waiting for server confirmation.
	TArray<FDNGDrawSegment> PendingPredictedSegments;
};
