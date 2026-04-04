#pragma once

#include "CoreMinimal.h"
#include "../../Core/DNGTypes.h"

// Sampling options used when turning SVG paths into board stroke segments.
struct FDNGSvgParseOptions
{
	// Actual board thickness used for the small brush preset.
	float SmallThickness = 4.0f;

	// Actual board thickness used for the medium brush preset.
	float MediumThickness = 7.0f;

	// Actual board thickness used for the large brush preset.
	float LargeThickness = 12.0f;

	// Maximum UV-space step used when approximating curves and long lines.
	float MaxSegmentLength = 0.008f;
};

// Parses a constrained SVG document and converts it into board stroke segments.
class DRAWNGUESS_API FDNGSvgParser
{
public:
	// Converts DeepSeek SVG output into normalized board stroke segments.
	static bool ParseSvgToSegments(const FString& Svg, const FDNGSvgParseOptions& Options, TArray<FDNGDrawSegment>& OutSegments, FString& OutError);
};
