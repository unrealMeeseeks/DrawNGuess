#pragma once

#include "CoreMinimal.h"
#include "DNGTypes.generated.h"

// Enumerates the high-level phases that drive the full match loop.
UENUM(BlueprintType)
enum class EDNGMatchPhase : uint8
{
	Lobby,
	Drawing,
	Guessing,
	Results
};

// Enumerates the currently supported drawing tools.
UENUM(BlueprintType)
enum class EDNGDrawTool : uint8
{
	Pencil,
	Eraser
};

// Stores prompt fragments and the last-used join address.
// These values are edited in the lobby, saved locally, and replicated through GameState.
USTRUCT(BlueprintType)
struct FDNGPromptSettings
{
	GENERATED_BODY()

	// Positive prompt template. "{}" is replaced with the submitted guess text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PositivePrefix = TEXT("a drawing of {}");

	// Negative prompt template. "{}" is replaced with the submitted guess text.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString NegativePrefix = TEXT("a drawing that is not {}");

	// Last IP address entered in the join field.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LastJoinAddress = TEXT("127.0.0.1");
};

// Complete scoring payload for one finished round.
USTRUCT(BlueprintType)
struct FDNGRoundResult
{
	GENERATED_BODY()

	// Raw guess text submitted by the guessing player.
	UPROPERTY(BlueprintReadOnly)
	FString GuessText;

	// Positive prompt passed into the scoring backend.
	UPROPERTY(BlueprintReadOnly)
	FString PositivePrompt;

	// Negative prompt passed into the scoring backend.
	UPROPERTY(BlueprintReadOnly)
	FString NegativePrompt;

	// Raw cosine similarity for the positive prompt.
	UPROPERTY(BlueprintReadOnly)
	float PositiveScore = 0.0f;

	// Raw cosine similarity for the negative prompt.
	UPROPERTY(BlueprintReadOnly)
	float NegativeScore = 0.0f;

	// Final gameplay decision derived from comparing both prompts.
	UPROPERTY(BlueprintReadOnly)
	bool bGuessAccepted = false;

	// Softmax probability derived from the positive score.
	UPROPERTY(BlueprintReadOnly)
	float PositiveProbability = 0.0f;

	// Softmax probability derived from the negative score.
	UPROPERTY(BlueprintReadOnly)
	float NegativeProbability = 0.0f;

	// Human-readable backend label, for example "Mock" or "CLIP".
	UPROPERTY(BlueprintReadOnly)
	FString ScoringBackend;

	// Additional debug information shown in the result screen.
	UPROPERTY(BlueprintReadOnly)
	FString DiagnosticMessage;

	// Saved server-side board image path used for scoring/debugging.
	UPROPERTY(BlueprintReadOnly)
	FString SavedBoardPath;
};

// One shared board stroke segment expressed in normalized UV space.
USTRUCT(BlueprintType)
struct FDNGDrawSegment
{
	GENERATED_BODY()

	// Segment start in normalized board UV coordinates.
	UPROPERTY()
	FVector2D Start = FVector2D::ZeroVector;

	// Segment end in normalized board UV coordinates.
	UPROPERTY()
	FVector2D End = FVector2D::ZeroVector;

	// Tool used to render the segment.
	UPROPERTY()
	EDNGDrawTool Tool = EDNGDrawTool::Pencil;

	// Pencil color carried with the segment so both peers render the same result.
	UPROPERTY()
	FLinearColor Color = FLinearColor::Black;

	// Brush thickness expressed in render-target pixels.
	UPROPERTY()
	float Thickness = 6.0f;
};
