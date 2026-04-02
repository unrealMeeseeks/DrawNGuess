#pragma once

#include "CoreMinimal.h"
#include "DNGTypes.generated.h"

UENUM(BlueprintType)
enum class EDNGMatchPhase : uint8
{
	Lobby,
	Drawing,
	Guessing,
	Results
};

UENUM(BlueprintType)
enum class EDNGDrawTool : uint8
{
	Pencil,
	Eraser
};

USTRUCT(BlueprintType)
struct FDNGPromptSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PositivePrefix = TEXT("a drawing of {}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString NegativePrefix = TEXT("a drawing that is not {}");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LastJoinAddress = TEXT("127.0.0.1");
};

USTRUCT(BlueprintType)
struct FDNGRoundResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString GuessText;

	UPROPERTY(BlueprintReadOnly)
	FString PositivePrompt;

	UPROPERTY(BlueprintReadOnly)
	FString NegativePrompt;

	UPROPERTY(BlueprintReadOnly)
	float PositiveScore = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float NegativeScore = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	bool bGuessAccepted = false;

	UPROPERTY(BlueprintReadOnly)
	float PositiveProbability = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float NegativeProbability = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FString ScoringBackend;

	UPROPERTY(BlueprintReadOnly)
	FString DiagnosticMessage;

	UPROPERTY(BlueprintReadOnly)
	FString SavedBoardPath;
};

USTRUCT(BlueprintType)
struct FDNGDrawSegment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector2D Start = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D End = FVector2D::ZeroVector;

	UPROPERTY()
	EDNGDrawTool Tool = EDNGDrawTool::Pencil;

	UPROPERTY()
	float Thickness = 6.0f;
};
