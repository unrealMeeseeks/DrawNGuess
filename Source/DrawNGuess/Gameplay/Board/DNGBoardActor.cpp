#include "DNGBoardActor.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Engine/StaticMesh.h"
#include "Math/Color.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	static FString FormatSvgFloat(float Value)
	{
		return FString::SanitizeFloat(Value, 2);
	}

	static FString LinearColorToSvgColor(const FLinearColor& Color)
	{
		const FColor QuantizedColor = Color.ToFColor(true);
		return FString::Printf(TEXT("#%02X%02X%02X"), QuantizedColor.R, QuantizedColor.G, QuantizedColor.B);
	}
}

// Initializes the default board mesh, camera, collision, and network update settings.
ADNGBoardActor::ADNGBoardActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);

	BoardMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoardMesh"));
	SetRootComponent(BoardMesh);
	BoardMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoardMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoardMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoardMesh->SetGenerateOverlapEvents(false);
	BoardMesh->SetMobility(EComponentMobility::Static);
	BoardMesh->bReturnMaterialOnMove = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		BoardMesh->SetStaticMesh(PlaneMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SurfaceMaterial(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque.Widget3DPassThrough_Opaque"));
	if (SurfaceMaterial.Succeeded())
	{
		BoardSurfaceMaterial = SurfaceMaterial.Object;
		BoardMesh->SetMaterial(0, BoardSurfaceMaterial);
	}

	BoardCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("BoardCamera"));
	BoardCamera->SetupAttachment(BoardMesh);
	BoardCamera->SetRelativeLocation(FVector(0.0f, 0.0f, 1800.0f));
	BoardCamera->SetRelativeRotation(FRotator(-90.0f, -90.0f, 0.0f));
	BoardCamera->ProjectionMode = ECameraProjectionMode::Orthographic;
	BoardCamera->OrthoWidth = 1800.0f;
}

// Ensures the board surface exists before the actor becomes interactive.
void ADNGBoardActor::BeginPlay()
{
	Super::BeginPlay();

	BoardCamera->OrthoWidth = FMath::Max(BoardSize.X, BoardSize.Y) * 1.2f;
	RefreshBoardVisuals();
}

// Registers the authoritative stroke history for replication.
void ADNGBoardActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADNGBoardActor, Segments);
}

// Projects a world-space point into the board's local 2D coordinate space.
bool ADNGBoardActor::ProjectWorldToBoard(const FVector& WorldLocation, FVector2D& OutBoardPoint) const
{
	const FVector LocalPoint = GetActorTransform().InverseTransformPosition(WorldLocation);
	const FVector2D HalfSize = BoardSize * 0.5f;
	OutBoardPoint = FVector2D(
		FMath::Clamp(LocalPoint.X, -HalfSize.X, HalfSize.X),
		FMath::Clamp(LocalPoint.Y, -HalfSize.Y, HalfSize.Y));

	return FMath::Abs(LocalPoint.X) <= HalfSize.X + KINDA_SMALL_NUMBER
		&& FMath::Abs(LocalPoint.Y) <= HalfSize.Y + KINDA_SMALL_NUMBER;
}

// Converts a logical board-space point back into world space relative to the actor transform.
FVector ADNGBoardActor::BoardToWorld(const FVector2D& BoardPoint) const
{
	const FVector LocalPoint(BoardPoint.X, BoardPoint.Y, 1.0f);
	return GetActorTransform().TransformPosition(LocalPoint);
}

// Appends an authoritative segment and immediately draws it on the server.
void ADNGBoardActor::AddSegment(const FDNGDrawSegment& Segment)
{
	if (!HasAuthority())
	{
		return;
	}

	Segments.Add(Segment);
	SyncReplicatedSegments();
	ForceNetUpdate();
}

// Adds a local predicted segment so the painter sees instant feedback before replication returns.
void ADNGBoardActor::AddPredictedSegment(const FDNGDrawSegment& Segment)
{
	EnsureBoardSurface();
	DrawSegmentToBoard(Segment);
	PendingPredictedSegments.Add(Segment);
}

// Clears all board state and resets the visible render target.
void ADNGBoardActor::ClearBoard()
{
	EnsureBoardSurface();

	if (!HasAuthority())
	{
		ResetBoardSurface();
		PendingPredictedSegments.Reset();
		AppliedReplicatedSegmentCount = 0;
		return;
	}

	Segments.Reset();
	ResetBoardSurface();
	PendingPredictedSegments.Reset();
	AppliedReplicatedSegmentCount = 0;
	ForceNetUpdate();
}

// Applies any newly replicated segments to the local board texture.
void ADNGBoardActor::OnRep_Segments()
{
	SyncReplicatedSegments();
}

// Lazily creates the runtime render target and binds it to the board material.
void ADNGBoardActor::EnsureBoardSurface()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!BoardRenderTarget)
	{
		BoardRenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(World, RenderTargetSize.X, RenderTargetSize.Y, RTF_RGBA8);
	}

	if (!BoardMaterialInstance)
	{
		UMaterialInterface* SourceMaterial = BoardSurfaceMaterial ? BoardSurfaceMaterial.Get() : BoardMesh->GetMaterial(0);
		if (SourceMaterial)
		{
			BoardMaterialInstance = UMaterialInstanceDynamic::Create(SourceMaterial, this);
			if (BoardMaterialInstance)
			{
				BoardMaterialInstance->SetTextureParameterValue(TEXT("SlateUI"), BoardRenderTarget);
				BoardMesh->SetMaterial(0, BoardMaterialInstance);
			}
		}
	}

	if (BoardRenderTarget && AppliedReplicatedSegmentCount == 0 && Segments.Num() == 0 && PendingPredictedSegments.Num() == 0)
	{
		ResetBoardSurface();
	}
}

// Rebuilds the render target from authoritative strokes plus still-pending local predictions.
void ADNGBoardActor::RefreshBoardVisuals()
{
	EnsureBoardSurface();

	if (!BoardRenderTarget)
	{
		return;
	}

	ResetBoardSurface();

	for (const FDNGDrawSegment& Segment : Segments)
	{
		DrawSegmentToBoard(Segment);
	}

	for (const FDNGDrawSegment& Segment : PendingPredictedSegments)
	{
		DrawSegmentToBoard(Segment);
	}

	AppliedReplicatedSegmentCount = Segments.Num();
}

// Exports the current render target to the Saved/Drawings folder as a PNG file.
bool ADNGBoardActor::SaveBoardImage(FString& OutSavedPath)
{
	OutSavedPath.Reset();

	UWorld* World = GetWorld();
	EnsureBoardSurface();
	if (!World || !BoardRenderTarget)
	{
		return false;
	}

	const FString SaveDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Drawings"));
	IFileManager::Get().MakeDirectory(*SaveDirectory, true);

	const FString FileBaseName = FString::Printf(
		TEXT("board_%s_%s"),
		*FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")),
		*FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString FileName = FileBaseName + TEXT(".png");

	UKismetRenderingLibrary::ExportRenderTarget(World, BoardRenderTarget, SaveDirectory, FileName);

	OutSavedPath = FPaths::Combine(SaveDirectory, FileName);
	return true;
}

// Returns the segments that currently define the visible board image on this machine.
void ADNGBoardActor::GetVisibleSegments(TArray<FDNGDrawSegment>& OutSegments) const
{
	OutSegments = Segments;
	OutSegments.Append(PendingPredictedSegments);
}

// Converts the current visible board stroke history into a compact standalone SVG snapshot.
bool ADNGBoardActor::BuildBoardSvgSnapshot(FString& OutSvg) const
{
	OutSvg.Reset();

	TArray<FDNGDrawSegment> VisibleSegments;
	GetVisibleSegments(VisibleSegments);

	FString Svg = TEXT("<svg viewBox=\"0 0 512 512\" xmlns=\"http://www.w3.org/2000/svg\">");
	Svg += TEXT("<rect width=\"512\" height=\"512\" fill=\"white\"/>");

	auto AppendPath = [&Svg](const FString& PathData, const FString& StrokeColor, float StrokeWidth)
	{
		if (PathData.IsEmpty())
		{
			return;
		}

		Svg += FString::Printf(
			TEXT("<path d=\"%s\" fill=\"none\" stroke=\"%s\" stroke-width=\"%s\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>"),
			*PathData,
			*StrokeColor,
			*FormatSvgFloat(StrokeWidth));
	};

	FString CurrentPathData;
	FString CurrentStrokeColor;
	float CurrentStrokeWidth = 0.0f;
	FVector2D CurrentEnd = FVector2D::ZeroVector;
	bool bHasOpenPath = false;

	for (const FDNGDrawSegment& Segment : VisibleSegments)
	{
		const FVector2D StartPoint(Segment.Start.X * 512.0f, Segment.Start.Y * 512.0f);
		const FVector2D EndPoint(Segment.End.X * 512.0f, Segment.End.Y * 512.0f);
		const FString StrokeColor = LinearColorToSvgColor(Segment.Tool == EDNGDrawTool::Eraser ? FLinearColor::White : Segment.Color);
		const float StrokeWidth = FMath::Max(1.0f, Segment.Thickness * (512.0f / static_cast<float>(RenderTargetSize.X)));
		const bool bContinuePath =
			bHasOpenPath
			&& CurrentStrokeColor == StrokeColor
			&& FMath::IsNearlyEqual(CurrentStrokeWidth, StrokeWidth, 0.01f)
			&& CurrentEnd.Equals(StartPoint, 0.25f);

		if (!bContinuePath)
		{
			if (bHasOpenPath)
			{
				AppendPath(CurrentPathData, CurrentStrokeColor, CurrentStrokeWidth);
			}

			CurrentPathData = FString::Printf(TEXT("M %s %s L %s %s"), *FormatSvgFloat(StartPoint.X), *FormatSvgFloat(StartPoint.Y), *FormatSvgFloat(EndPoint.X), *FormatSvgFloat(EndPoint.Y));
			CurrentStrokeColor = StrokeColor;
			CurrentStrokeWidth = StrokeWidth;
			bHasOpenPath = true;
		}
		else
		{
			CurrentPathData += FString::Printf(TEXT(" L %s %s"), *FormatSvgFloat(EndPoint.X), *FormatSvgFloat(EndPoint.Y));
		}

		CurrentEnd = EndPoint;
	}

	if (bHasOpenPath)
	{
		AppendPath(CurrentPathData, CurrentStrokeColor, CurrentStrokeWidth);
	}

	Svg += TEXT("</svg>");
	OutSvg = Svg;
	return true;
}

// Clears the board texture back to its configured base color.
void ADNGBoardActor::ResetBoardSurface()
{
	UWorld* World = GetWorld();
	if (!World || !BoardRenderTarget)
	{
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, BoardRenderTarget, BoardClearColor);
}

// Draws only the replicated segments that have not been applied yet.
void ADNGBoardActor::SyncReplicatedSegments()
{
	EnsureBoardSurface();

	if (!BoardRenderTarget)
	{
		return;
	}

	if (Segments.Num() < AppliedReplicatedSegmentCount)
	{
		AppliedReplicatedSegmentCount = 0;
		PendingPredictedSegments.Reset();
		ResetBoardSurface();
	}

	while (AppliedReplicatedSegmentCount < Segments.Num())
	{
		const FDNGDrawSegment& Segment = Segments[AppliedReplicatedSegmentCount];
		if (PendingPredictedSegments.Num() > 0
			&& PendingPredictedSegments[0].Tool == Segment.Tool
			&& PendingPredictedSegments[0].Color.Equals(Segment.Color, 0.001f)
			&& FMath::IsNearlyEqual(PendingPredictedSegments[0].Thickness, Segment.Thickness)
			&& PendingPredictedSegments[0].Start.Equals(Segment.Start, 0.1f)
			&& PendingPredictedSegments[0].End.Equals(Segment.End, 0.1f))
		{
			PendingPredictedSegments.RemoveAt(0);
		}
		else
		{
			DrawSegmentToBoard(Segment);
		}

		++AppliedReplicatedSegmentCount;
	}
}

// Renders a single segment onto the board render target using a simple line primitive.
void ADNGBoardActor::DrawSegmentToBoard(const FDNGDrawSegment& Segment)
{
	UWorld* World = GetWorld();
	if (!World || !BoardRenderTarget)
	{
		return;
	}

	UCanvas* Canvas = nullptr;
	FVector2D CanvasSize = FVector2D::ZeroVector;
	FDrawToRenderTargetContext DrawContext;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, BoardRenderTarget, Canvas, CanvasSize, DrawContext);

	if (Canvas)
	{
		const FVector2D StartPixel = UVToPixel(Segment.Start);
		const FVector2D EndPixel = UVToPixel(Segment.End);
		const FLinearColor DrawColor = Segment.Tool == EDNGDrawTool::Eraser ? EraserColor : Segment.Color;
		const float PixelThickness = FMath::Max(1.0f, Segment.Thickness);

		if (StartPixel.Equals(EndPixel, 0.5f))
		{
			Canvas->K2_DrawBox(StartPixel - FVector2D(PixelThickness * 0.5f), FVector2D(PixelThickness, PixelThickness), 1.0f, DrawColor);
		}
		else
		{
			Canvas->K2_DrawLine(StartPixel, EndPixel, PixelThickness, DrawColor);
		}
	}

	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, DrawContext);
}

// Converts normalized UV coordinates into absolute render-target pixels.
FVector2D ADNGBoardActor::UVToPixel(const FVector2D& UV) const
{
	return FVector2D(
		FMath::Clamp(UV.X, 0.0f, 1.0f) * static_cast<float>(RenderTargetSize.X),
		FMath::Clamp(UV.Y, 0.0f, 1.0f) * static_cast<float>(RenderTargetSize.Y));
}

// Converts legacy board-space coordinates into normalized UV coordinates.
FVector2D ADNGBoardActor::BoardPointToUV(const FVector2D& BoardPoint) const
{
	return FVector2D(
		FMath::Clamp((BoardPoint.X / BoardSize.X) + 0.5f, 0.0f, 1.0f),
		FMath::Clamp(0.5f - (BoardPoint.Y / BoardSize.Y), 0.0f, 1.0f));
}
