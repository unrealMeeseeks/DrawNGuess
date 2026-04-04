#include "DNGSvgParser.h"

#include "Containers/Queue.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/Char.h"
#include "Misc/DefaultValueHelper.h"
#include "XmlFile.h"

namespace
{
	struct FDNGSvgViewBox
	{
		double MinX = 0.0;
		double MinY = 0.0;
		double Width = 1000.0;
		double Height = 1000.0;
	};

	struct FDNGSvgStyle
	{
		FString StrokeColor = TEXT("black");
		FString FillColor = TEXT("none");
		double StrokeWidth = 7.0;
		bool bHasStroke = false;
		bool bHasFill = false;
		bool bHasStrokeWidth = false;
		bool bVisible = true;
	};

	struct FDNGSvgPathContext
	{
		FDNGSvgViewBox ViewBox;
		FDNGSvgParseOptions Options;
	};

	static bool IsSvgWhitespace(TCHAR Char)
	{
		return FChar::IsWhitespace(Char) || Char == TCHAR(',');
	}

	static bool IsPathCommandLetter(TCHAR Char)
	{
		switch (Char)
		{
		case TCHAR('M'):
		case TCHAR('m'):
		case TCHAR('L'):
		case TCHAR('l'):
		case TCHAR('H'):
		case TCHAR('h'):
		case TCHAR('V'):
		case TCHAR('v'):
		case TCHAR('C'):
		case TCHAR('c'):
		case TCHAR('Q'):
		case TCHAR('q'):
		case TCHAR('S'):
		case TCHAR('s'):
		case TCHAR('T'):
		case TCHAR('t'):
		case TCHAR('A'):
		case TCHAR('a'):
		case TCHAR('Z'):
		case TCHAR('z'):
			return true;
		default:
			return false;
		}
	}

	static void SkipSvgSeparators(const FString& Text, int32& InOutIndex)
	{
		while (InOutIndex < Text.Len() && IsSvgWhitespace(Text[InOutIndex]))
		{
			++InOutIndex;
		}
	}

	static bool ParseSvgNumber(const FString& Text, int32& InOutIndex, double& OutValue)
	{
		SkipSvgSeparators(Text, InOutIndex);
		if (InOutIndex >= Text.Len())
		{
			return false;
		}

		const int32 StartIndex = InOutIndex;
		bool bSawDigit = false;

		if (Text[InOutIndex] == TCHAR('+') || Text[InOutIndex] == TCHAR('-'))
		{
			++InOutIndex;
		}

		while (InOutIndex < Text.Len() && FChar::IsDigit(Text[InOutIndex]))
		{
			bSawDigit = true;
			++InOutIndex;
		}

		if (InOutIndex < Text.Len() && Text[InOutIndex] == TCHAR('.'))
		{
			++InOutIndex;
			while (InOutIndex < Text.Len() && FChar::IsDigit(Text[InOutIndex]))
			{
				bSawDigit = true;
				++InOutIndex;
			}
		}

		if (!bSawDigit)
		{
			InOutIndex = StartIndex;
			return false;
		}

		if (InOutIndex < Text.Len() && (Text[InOutIndex] == TCHAR('e') || Text[InOutIndex] == TCHAR('E')))
		{
			const int32 ExponentStart = InOutIndex;
			++InOutIndex;
			if (InOutIndex < Text.Len() && (Text[InOutIndex] == TCHAR('+') || Text[InOutIndex] == TCHAR('-')))
			{
				++InOutIndex;
			}

			bool bSawExponentDigit = false;
			while (InOutIndex < Text.Len() && FChar::IsDigit(Text[InOutIndex]))
			{
				bSawExponentDigit = true;
				++InOutIndex;
			}

			if (!bSawExponentDigit)
			{
				InOutIndex = ExponentStart;
			}
		}

		OutValue = FCString::Atod(*Text.Mid(StartIndex, InOutIndex - StartIndex));
		return true;
	}

	static bool ParseSvgFlag(const FString& Text, int32& InOutIndex, bool& OutFlag)
	{
		double Value = 0.0;
		if (!ParseSvgNumber(Text, InOutIndex, Value))
		{
			return false;
		}

		OutFlag = !FMath::IsNearlyZero(Value);
		return true;
	}

	static bool ParseNumberList(const FString& Text, TArray<double>& OutValues)
	{
		OutValues.Reset();
		int32 Index = 0;

		while (Index < Text.Len())
		{
			double Value = 0.0;
			if (!ParseSvgNumber(Text, Index, Value))
			{
				SkipSvgSeparators(Text, Index);
				if (Index < Text.Len())
				{
					return false;
				}
				break;
			}

			OutValues.Add(Value);
			SkipSvgSeparators(Text, Index);
		}

		return OutValues.Num() > 0;
	}

	static bool ParseNumericAttribute(const FString& RawValue, double& OutValue)
	{
		int32 Index = 0;
		if (!ParseSvgNumber(RawValue, Index, OutValue))
		{
			return false;
		}

		return true;
	}

	static bool ParsePointListAttribute(const FString& RawValue, TArray<FVector2D>& OutPoints)
	{
		OutPoints.Reset();
		TArray<double> Values;
		if (!ParseNumberList(RawValue, Values) || Values.Num() < 2)
		{
			return false;
		}

		for (int32 Index = 0; Index + 1 < Values.Num(); Index += 2)
		{
			OutPoints.Add(FVector2D(static_cast<float>(Values[Index]), static_cast<float>(Values[Index + 1])));
		}

		return OutPoints.Num() > 0;
	}

	static bool ParseViewBox(const FXmlNode* RootNode, FDNGSvgViewBox& OutViewBox)
	{
		if (!RootNode)
		{
			return false;
		}

		TArray<double> Values;
		if (ParseNumberList(RootNode->GetAttribute(TEXT("viewBox")), Values) && Values.Num() >= 4)
		{
			OutViewBox.MinX = Values[0];
			OutViewBox.MinY = Values[1];
			OutViewBox.Width = Values[2];
			OutViewBox.Height = Values[3];
		}
		else
		{
			double Width = 1000.0;
			double Height = 1000.0;
			ParseNumericAttribute(RootNode->GetAttribute(TEXT("width")), Width);
			ParseNumericAttribute(RootNode->GetAttribute(TEXT("height")), Height);
			OutViewBox.Width = Width;
			OutViewBox.Height = Height;
		}

		return OutViewBox.Width > UE_DOUBLE_SMALL_NUMBER && OutViewBox.Height > UE_DOUBLE_SMALL_NUMBER;
	}

	static FVector2D NormalizeSvgPoint(const FDNGSvgViewBox& ViewBox, const FVector2D& Point)
	{
		return FVector2D(
			FMath::Clamp(static_cast<float>((Point.X - ViewBox.MinX) / ViewBox.Width), 0.0f, 1.0f),
			FMath::Clamp(static_cast<float>((Point.Y - ViewBox.MinY) / ViewBox.Height), 0.0f, 1.0f));
	}

	static FLinearColor ParseRawSvgColor(const FString& RawColor, bool& bOutValid)
	{
		bOutValid = false;
		const FString Color = RawColor.TrimStartAndEnd().ToLower();
		if (Color.IsEmpty() || Color == TEXT("none"))
		{
			return FLinearColor::Transparent;
		}

		if (Color == TEXT("black"))
		{
			bOutValid = true;
			return FLinearColor::Black;
		}

		if (Color == TEXT("red"))
		{
			bOutValid = true;
			return FLinearColor(0.85f, 0.15f, 0.15f, 1.0f);
		}

		if (Color == TEXT("blue"))
		{
			bOutValid = true;
			return FLinearColor(0.15f, 0.35f, 0.95f, 1.0f);
		}

		if (Color == TEXT("green"))
		{
			bOutValid = true;
			return FLinearColor(0.15f, 0.7f, 0.25f, 1.0f);
		}

		if (Color == TEXT("white"))
		{
			bOutValid = true;
			return FLinearColor::White;
		}

		if (Color.StartsWith(TEXT("#")))
		{
			bOutValid = true;
			return FColor::FromHex(Color).ReinterpretAsLinear();
		}

		if (Color.StartsWith(TEXT("rgb(")) && Color.EndsWith(TEXT(")")))
		{
			const FString Numbers = Color.Mid(4, Color.Len() - 5);
			TArray<FString> Parts;
			Numbers.ParseIntoArray(Parts, TEXT(","), true);
			if (Parts.Num() == 3)
			{
				bOutValid = true;
				return FLinearColor(
					FCString::Atof(*Parts[0]) / 255.0f,
					FCString::Atof(*Parts[1]) / 255.0f,
					FCString::Atof(*Parts[2]) / 255.0f,
					1.0f);
			}
		}

		return FLinearColor::Transparent;
	}

	static FLinearColor SnapToPalette(const FLinearColor& SourceColor)
	{
		struct FPaletteEntry
		{
			FLinearColor Color;
		};

		static const FPaletteEntry Palette[] =
		{
			{ FLinearColor::Black },
			{ FLinearColor(0.85f, 0.15f, 0.15f, 1.0f) },
			{ FLinearColor(0.15f, 0.35f, 0.95f, 1.0f) },
			{ FLinearColor(0.15f, 0.7f, 0.25f, 1.0f) }
		};

		double BestDistance = TNumericLimits<double>::Max();
		FLinearColor BestColor = Palette[0].Color;
		for (const FPaletteEntry& Entry : Palette)
		{
			const FVector Delta(
				SourceColor.R - Entry.Color.R,
				SourceColor.G - Entry.Color.G,
				SourceColor.B - Entry.Color.B);
			const double Distance = Delta.SizeSquared();
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				BestColor = Entry.Color;
			}
		}

		return BestColor;
	}

	static float ResolveStrokeThickness(double StrokeWidth, const FDNGSvgParseOptions& Options)
	{
		if (StrokeWidth <= 0.0)
		{
			return Options.MediumThickness;
		}

		if (StrokeWidth <= 5.5)
		{
			return Options.SmallThickness;
		}

		if (StrokeWidth <= 9.5)
		{
			return Options.MediumThickness;
		}

		return Options.LargeThickness;
	}

	static void ApplyStyleDeclaration(FDNGSvgStyle& InOutStyle, const FString& Key, const FString& Value)
	{
		const FString NormalizedKey = Key.TrimStartAndEnd().ToLower();
		const FString NormalizedValue = Value.TrimStartAndEnd();
		if (NormalizedValue.IsEmpty())
		{
			return;
		}

		if (NormalizedKey == TEXT("stroke"))
		{
			InOutStyle.StrokeColor = NormalizedValue;
			InOutStyle.bHasStroke = true;
		}
		else if (NormalizedKey == TEXT("fill"))
		{
			InOutStyle.FillColor = NormalizedValue;
			InOutStyle.bHasFill = true;
		}
		else if (NormalizedKey == TEXT("stroke-width"))
		{
			double NumericValue = 0.0;
			if (ParseNumericAttribute(NormalizedValue, NumericValue))
			{
				InOutStyle.StrokeWidth = NumericValue;
				InOutStyle.bHasStrokeWidth = true;
			}
		}
		else if (NormalizedKey == TEXT("display") && NormalizedValue.TrimStartAndEnd().ToLower() == TEXT("none"))
		{
			InOutStyle.bVisible = false;
		}
	}

	static void ApplyStyleAttribute(FDNGSvgStyle& InOutStyle, const FString& StyleAttribute)
	{
		TArray<FString> Declarations;
		StyleAttribute.ParseIntoArray(Declarations, TEXT(";"), true);
		for (const FString& Declaration : Declarations)
		{
			FString Left;
			FString Right;
			if (Declaration.Split(TEXT(":"), &Left, &Right))
			{
				ApplyStyleDeclaration(InOutStyle, Left, Right);
			}
		}
	}

	static FDNGSvgStyle ResolveNodeStyle(const FXmlNode* Node, const FDNGSvgStyle& ParentStyle)
	{
		FDNGSvgStyle Result = ParentStyle;
		if (!Node)
		{
			return Result;
		}

		ApplyStyleDeclaration(Result, TEXT("stroke"), Node->GetAttribute(TEXT("stroke")));
		ApplyStyleDeclaration(Result, TEXT("fill"), Node->GetAttribute(TEXT("fill")));
		ApplyStyleDeclaration(Result, TEXT("stroke-width"), Node->GetAttribute(TEXT("stroke-width")));
		ApplyStyleDeclaration(Result, TEXT("display"), Node->GetAttribute(TEXT("display")));
		ApplyStyleAttribute(Result, Node->GetAttribute(TEXT("style")));
		return Result;
	}

	static FString GetLocalTagName(const FString& TagName)
	{
		FString Left;
		FString Right;
		if (TagName.Split(TEXT(":"), &Left, &Right, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			return Right.ToLower();
		}

		return TagName.ToLower();
	}

	static void AppendSegment(const FVector2D& Start, const FVector2D& End, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments)
	{
		const FVector2D NormalizedStart = NormalizeSvgPoint(Context.ViewBox, Start);
		const FVector2D NormalizedEnd = NormalizeSvgPoint(Context.ViewBox, End);
		const float Distance = FVector2D::Distance(NormalizedStart, NormalizedEnd);
		const int32 SegmentCount = Distance <= KINDA_SMALL_NUMBER
			? 1
			: FMath::Max(1, FMath::CeilToInt(Distance / Context.Options.MaxSegmentLength));

		bool bValidColor = false;
		const FLinearColor RawColor = ParseRawSvgColor(Style.bHasStroke ? Style.StrokeColor : Style.FillColor, bValidColor);
		if (!bValidColor || RawColor.A <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const FLinearColor DrawColor = SnapToPalette(RawColor);
		const float Thickness = ResolveStrokeThickness(Style.StrokeWidth, Context.Options);

		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const float Alpha0 = static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
			const float Alpha1 = static_cast<float>(SegmentIndex + 1) / static_cast<float>(SegmentCount);

			FDNGDrawSegment Segment;
			Segment.Start = FMath::Lerp(NormalizedStart, NormalizedEnd, Alpha0);
			Segment.End = FMath::Lerp(NormalizedStart, NormalizedEnd, Alpha1);
			Segment.Tool = EDNGDrawTool::Pencil;
			Segment.Color = DrawColor;
			Segment.Thickness = Thickness;
			OutSegments.Add(Segment);
		}
	}

	static void AppendPolyline(const TArray<FVector2D>& Points, bool bClosed, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments)
	{
		if (Points.Num() < 2)
		{
			return;
		}

		const int32 SegmentCount = bClosed ? Points.Num() : Points.Num() - 1;
		for (int32 Index = 0; Index < SegmentCount; ++Index)
		{
			AppendSegment(Points[Index], Points[(Index + 1) % Points.Num()], Context, Style, OutSegments);
		}
	}

	static void AppendEllipseOutline(const FVector2D& Center, float RadiusX, float RadiusY, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments)
	{
		if (RadiusX <= KINDA_SMALL_NUMBER || RadiusY <= KINDA_SMALL_NUMBER)
		{
			return;
		}

		const float CircumferenceEstimate = 2.0f * PI * FMath::Max(RadiusX, RadiusY);
		const double TargetStep = FMath::Max(Context.ViewBox.Width, Context.ViewBox.Height) * Context.Options.MaxSegmentLength;
		const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(CircumferenceEstimate / TargetStep), 24, 768);

		TArray<FVector2D> Points;
		Points.Reserve(SampleCount);
		for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
		{
			const float Angle = (2.0f * PI * static_cast<float>(SampleIndex)) / static_cast<float>(SampleCount);
			Points.Add(FVector2D(
				Center.X + FMath::Cos(Angle) * RadiusX,
				Center.Y + FMath::Sin(Angle) * RadiusY));
		}

		AppendPolyline(Points, true, Context, Style, OutSegments);
	}

	static FVector2D EvaluateQuadratic(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, float T)
	{
		const float OneMinusT = 1.0f - T;
		return (OneMinusT * OneMinusT * P0) + (2.0f * OneMinusT * T * P1) + (T * T * P2);
	}

	static FVector2D EvaluateCubic(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, float T)
	{
		const float OneMinusT = 1.0f - T;
		const float OneMinusTSquared = OneMinusT * OneMinusT;
		const float TSquared = T * T;
		return (OneMinusTSquared * OneMinusT * P0)
			+ (3.0f * OneMinusTSquared * T * P1)
			+ (3.0f * OneMinusT * TSquared * P2)
			+ (TSquared * T * P3);
	}

	static void AppendQuadratic(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments)
	{
		const float EstimatedLength = FVector2D::Distance(P0, P1) + FVector2D::Distance(P1, P2);
		const double TargetStep = FMath::Max(Context.ViewBox.Width, Context.ViewBox.Height) * Context.Options.MaxSegmentLength;
		const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(EstimatedLength / TargetStep), 8, 512);
		TArray<FVector2D> Points;
		Points.Reserve(SampleCount + 1);
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const float T = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			Points.Add(EvaluateQuadratic(P0, P1, P2, T));
		}
		AppendPolyline(Points, false, Context, Style, OutSegments);
	}

	static void AppendCubic(const FVector2D& P0, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments)
	{
		const float EstimatedLength = FVector2D::Distance(P0, P1) + FVector2D::Distance(P1, P2) + FVector2D::Distance(P2, P3);
		const double TargetStep = FMath::Max(Context.ViewBox.Width, Context.ViewBox.Height) * Context.Options.MaxSegmentLength;
		const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(EstimatedLength / TargetStep), 12, 768);
		TArray<FVector2D> Points;
		Points.Reserve(SampleCount + 1);
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const float T = static_cast<float>(SampleIndex) / static_cast<float>(SampleCount);
			Points.Add(EvaluateCubic(P0, P1, P2, P3, T));
		}
		AppendPolyline(Points, false, Context, Style, OutSegments);
	}

	static double VectorAngle(const FVector2D& U, const FVector2D& V)
	{
		const double Dot = (U.X * V.X) + (U.Y * V.Y);
		const double LengthProduct = FMath::Sqrt(FMath::Max(UE_DOUBLE_SMALL_NUMBER, static_cast<double>(U.SizeSquared()) * static_cast<double>(V.SizeSquared())));
		double Angle = FMath::Acos(FMath::Clamp(Dot / LengthProduct, -1.0, 1.0));
		const double Cross = (U.X * V.Y) - (U.Y * V.X);
		if (Cross < 0.0)
		{
			Angle = -Angle;
		}
		return Angle;
	}

	static bool AppendArc(const FVector2D& Start, double RadiusX, double RadiusY, double AxisRotationDegrees, bool bLargeArc, bool bSweep, const FVector2D& End, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments)
	{
		if (RadiusX <= UE_DOUBLE_SMALL_NUMBER || RadiusY <= UE_DOUBLE_SMALL_NUMBER)
		{
			AppendSegment(Start, End, Context, Style, OutSegments);
			return true;
		}

		const double Phi = FMath::DegreesToRadians(AxisRotationDegrees);
		const double CosPhi = FMath::Cos(Phi);
		const double SinPhi = FMath::Sin(Phi);
		const double Dx2 = (Start.X - End.X) * 0.5;
		const double Dy2 = (Start.Y - End.Y) * 0.5;
		const double X1Prime = (CosPhi * Dx2) + (SinPhi * Dy2);
		const double Y1Prime = (-SinPhi * Dx2) + (CosPhi * Dy2);

		double AdjustedRx = FMath::Abs(RadiusX);
		double AdjustedRy = FMath::Abs(RadiusY);
		const double Lambda = ((X1Prime * X1Prime) / (AdjustedRx * AdjustedRx)) + ((Y1Prime * Y1Prime) / (AdjustedRy * AdjustedRy));
		if (Lambda > 1.0)
		{
			const double Scale = FMath::Sqrt(Lambda);
			AdjustedRx *= Scale;
			AdjustedRy *= Scale;
		}

		const double RxSq = AdjustedRx * AdjustedRx;
		const double RySq = AdjustedRy * AdjustedRy;
		const double X1PrimeSq = X1Prime * X1Prime;
		const double Y1PrimeSq = Y1Prime * Y1Prime;

		const double Numerator = (RxSq * RySq) - (RxSq * Y1PrimeSq) - (RySq * X1PrimeSq);
		const double Denominator = (RxSq * Y1PrimeSq) + (RySq * X1PrimeSq);
		const double Coefficient = ((bLargeArc == bSweep) ? -1.0 : 1.0) * FMath::Sqrt(FMath::Max(0.0, Numerator / FMath::Max(UE_DOUBLE_SMALL_NUMBER, Denominator)));

		const double CxPrime = Coefficient * ((AdjustedRx * Y1Prime) / AdjustedRy);
		const double CyPrime = Coefficient * (-(AdjustedRy * X1Prime) / AdjustedRx);

		const double CenterX = (CosPhi * CxPrime) - (SinPhi * CyPrime) + ((Start.X + End.X) * 0.5);
		const double CenterY = (SinPhi * CxPrime) + (CosPhi * CyPrime) + ((Start.Y + End.Y) * 0.5);

		const FVector2D U(
			static_cast<float>((X1Prime - CxPrime) / AdjustedRx),
			static_cast<float>((Y1Prime - CyPrime) / AdjustedRy));
		const FVector2D V(
			static_cast<float>((-X1Prime - CxPrime) / AdjustedRx),
			static_cast<float>((-Y1Prime - CyPrime) / AdjustedRy));

		double Theta1 = VectorAngle(FVector2D(1.0f, 0.0f), U);
		double DeltaTheta = VectorAngle(U, V);
		if (!bSweep && DeltaTheta > 0.0)
		{
			DeltaTheta -= UE_DOUBLE_PI * 2.0;
		}
		else if (bSweep && DeltaTheta < 0.0)
		{
			DeltaTheta += UE_DOUBLE_PI * 2.0;
		}

		const double EstimatedLength = FMath::Abs(DeltaTheta) * FMath::Max(AdjustedRx, AdjustedRy);
		const double TargetStep = FMath::Max(Context.ViewBox.Width, Context.ViewBox.Height) * Context.Options.MaxSegmentLength;
		const int32 SampleCount = FMath::Clamp(FMath::CeilToInt(EstimatedLength / TargetStep), 12, 768);

		TArray<FVector2D> Points;
		Points.Reserve(SampleCount + 1);
		for (int32 SampleIndex = 0; SampleIndex <= SampleCount; ++SampleIndex)
		{
			const double T = static_cast<double>(SampleIndex) / static_cast<double>(SampleCount);
			const double Angle = Theta1 + (DeltaTheta * T);
			const double CosAngle = FMath::Cos(Angle);
			const double SinAngle = FMath::Sin(Angle);
			Points.Add(FVector2D(
				static_cast<float>(CenterX + (CosPhi * AdjustedRx * CosAngle) - (SinPhi * AdjustedRy * SinAngle)),
				static_cast<float>(CenterY + (SinPhi * AdjustedRx * CosAngle) + (CosPhi * AdjustedRy * SinAngle))));
		}

		AppendPolyline(Points, false, Context, Style, OutSegments);
		return true;
	}

	static bool ParsePathData(const FString& PathData, const FDNGSvgPathContext& Context, const FDNGSvgStyle& Style, TArray<FDNGDrawSegment>& OutSegments, FString& OutError)
	{
		int32 Index = 0;
		TCHAR ActiveCommand = 0;
		FVector2D CurrentPoint = FVector2D::ZeroVector;
		FVector2D SubPathStart = FVector2D::ZeroVector;
		bool bHasCurrentPoint = false;
		FVector2D LastCubicControl = FVector2D::ZeroVector;
		FVector2D LastQuadraticControl = FVector2D::ZeroVector;
		bool bPreviousWasCubic = false;
		bool bPreviousWasQuadratic = false;

		while (true)
		{
			SkipSvgSeparators(PathData, Index);
			if (Index >= PathData.Len())
			{
				return true;
			}

			if (IsPathCommandLetter(PathData[Index]))
			{
				ActiveCommand = PathData[Index++];
			}
			else if (ActiveCommand == 0)
			{
				OutError = TEXT("SVG path data started without a path command.");
				return false;
			}

			const bool bRelative = FChar::IsLower(ActiveCommand);
			switch (FChar::ToUpper(ActiveCommand))
			{
			case TCHAR('M'):
			{
				double X = 0.0;
				double Y = 0.0;
				if (!ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
				{
					OutError = TEXT("SVG move command was missing coordinates.");
					return false;
				}

				CurrentPoint = bRelative && bHasCurrentPoint ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
				SubPathStart = CurrentPoint;
				bHasCurrentPoint = true;
				bPreviousWasCubic = false;
				bPreviousWasQuadratic = false;

				while (true)
				{
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D NextPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendSegment(CurrentPoint, NextPoint, Context, Style, OutSegments);
					CurrentPoint = NextPoint;
					bPreviousWasCubic = false;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('L'):
			{
				while (true)
				{
					double X = 0.0;
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D NextPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendSegment(CurrentPoint, NextPoint, Context, Style, OutSegments);
					CurrentPoint = NextPoint;
					bHasCurrentPoint = true;
					bPreviousWasCubic = false;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('H'):
			{
				while (true)
				{
					double X = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X))
					{
						Index = SavedIndex;
						break;
					}

					FVector2D NextPoint = CurrentPoint;
					NextPoint.X = bRelative ? CurrentPoint.X + static_cast<float>(X) : static_cast<float>(X);
					AppendSegment(CurrentPoint, NextPoint, Context, Style, OutSegments);
					CurrentPoint = NextPoint;
					bHasCurrentPoint = true;
					bPreviousWasCubic = false;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('V'):
			{
				while (true)
				{
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					FVector2D NextPoint = CurrentPoint;
					NextPoint.Y = bRelative ? CurrentPoint.Y + static_cast<float>(Y) : static_cast<float>(Y);
					AppendSegment(CurrentPoint, NextPoint, Context, Style, OutSegments);
					CurrentPoint = NextPoint;
					bHasCurrentPoint = true;
					bPreviousWasCubic = false;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('Q'):
			{
				while (true)
				{
					double X1 = 0.0;
					double Y1 = 0.0;
					double X = 0.0;
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X1) || !ParseSvgNumber(PathData, Index, Y1) || !ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D ControlPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X1), static_cast<float>(Y1)) : FVector2D(static_cast<float>(X1), static_cast<float>(Y1));
					const FVector2D EndPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendQuadratic(CurrentPoint, ControlPoint, EndPoint, Context, Style, OutSegments);
					CurrentPoint = EndPoint;
					bHasCurrentPoint = true;
					LastQuadraticControl = ControlPoint;
					bPreviousWasQuadratic = true;
					bPreviousWasCubic = false;
				}
				break;
			}
			case TCHAR('T'):
			{
				while (true)
				{
					double X = 0.0;
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D ReflectedControl = bPreviousWasQuadratic ? (CurrentPoint * 2.0f) - LastQuadraticControl : CurrentPoint;
					const FVector2D EndPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendQuadratic(CurrentPoint, ReflectedControl, EndPoint, Context, Style, OutSegments);
					CurrentPoint = EndPoint;
					bHasCurrentPoint = true;
					LastQuadraticControl = ReflectedControl;
					bPreviousWasQuadratic = true;
					bPreviousWasCubic = false;
				}
				break;
			}
			case TCHAR('C'):
			{
				while (true)
				{
					double X1 = 0.0;
					double Y1 = 0.0;
					double X2 = 0.0;
					double Y2 = 0.0;
					double X = 0.0;
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X1) || !ParseSvgNumber(PathData, Index, Y1)
						|| !ParseSvgNumber(PathData, Index, X2) || !ParseSvgNumber(PathData, Index, Y2)
						|| !ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D ControlPoint1 = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X1), static_cast<float>(Y1)) : FVector2D(static_cast<float>(X1), static_cast<float>(Y1));
					const FVector2D ControlPoint2 = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X2), static_cast<float>(Y2)) : FVector2D(static_cast<float>(X2), static_cast<float>(Y2));
					const FVector2D EndPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendCubic(CurrentPoint, ControlPoint1, ControlPoint2, EndPoint, Context, Style, OutSegments);
					CurrentPoint = EndPoint;
					bHasCurrentPoint = true;
					LastCubicControl = ControlPoint2;
					bPreviousWasCubic = true;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('S'):
			{
				while (true)
				{
					double X2 = 0.0;
					double Y2 = 0.0;
					double X = 0.0;
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, X2) || !ParseSvgNumber(PathData, Index, Y2)
						|| !ParseSvgNumber(PathData, Index, X) || !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D ControlPoint1 = bPreviousWasCubic ? (CurrentPoint * 2.0f) - LastCubicControl : CurrentPoint;
					const FVector2D ControlPoint2 = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X2), static_cast<float>(Y2)) : FVector2D(static_cast<float>(X2), static_cast<float>(Y2));
					const FVector2D EndPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendCubic(CurrentPoint, ControlPoint1, ControlPoint2, EndPoint, Context, Style, OutSegments);
					CurrentPoint = EndPoint;
					bHasCurrentPoint = true;
					LastCubicControl = ControlPoint2;
					bPreviousWasCubic = true;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('A'):
			{
				while (true)
				{
					double RadiusX = 0.0;
					double RadiusY = 0.0;
					double Rotation = 0.0;
					bool bLargeArc = false;
					bool bSweep = false;
					double X = 0.0;
					double Y = 0.0;
					const int32 SavedIndex = Index;
					if (!ParseSvgNumber(PathData, Index, RadiusX)
						|| !ParseSvgNumber(PathData, Index, RadiusY)
						|| !ParseSvgNumber(PathData, Index, Rotation)
						|| !ParseSvgFlag(PathData, Index, bLargeArc)
						|| !ParseSvgFlag(PathData, Index, bSweep)
						|| !ParseSvgNumber(PathData, Index, X)
						|| !ParseSvgNumber(PathData, Index, Y))
					{
						Index = SavedIndex;
						break;
					}

					const FVector2D EndPoint = bRelative ? CurrentPoint + FVector2D(static_cast<float>(X), static_cast<float>(Y)) : FVector2D(static_cast<float>(X), static_cast<float>(Y));
					AppendArc(CurrentPoint, RadiusX, RadiusY, Rotation, bLargeArc, bSweep, EndPoint, Context, Style, OutSegments);
					CurrentPoint = EndPoint;
					bHasCurrentPoint = true;
					bPreviousWasCubic = false;
					bPreviousWasQuadratic = false;
				}
				break;
			}
			case TCHAR('Z'):
			{
				if (bHasCurrentPoint && !CurrentPoint.Equals(SubPathStart, KINDA_SMALL_NUMBER))
				{
					AppendSegment(CurrentPoint, SubPathStart, Context, Style, OutSegments);
					CurrentPoint = SubPathStart;
				}
				bHasCurrentPoint = true;
				bPreviousWasCubic = false;
				bPreviousWasQuadratic = false;
				break;
			}
			default:
				OutError = FString::Printf(TEXT("Unsupported SVG path command '%c'."), ActiveCommand);
				return false;
			}
		}
	}

	static void TraverseSvgNode(const FXmlNode* Node, const FDNGSvgPathContext& Context, const FDNGSvgStyle& ParentStyle, TArray<FDNGDrawSegment>& OutSegments, FString& InOutError)
	{
		if (!Node || !InOutError.IsEmpty())
		{
			return;
		}

		const FDNGSvgStyle NodeStyle = ResolveNodeStyle(Node, ParentStyle);
		if (!NodeStyle.bVisible)
		{
			return;
		}

		const FString Tag = GetLocalTagName(Node->GetTag());
		if (Tag == TEXT("path"))
		{
			const FString PathData = Node->GetAttribute(TEXT("d"));
			if (!PathData.TrimStartAndEnd().IsEmpty())
			{
				FDNGSvgStyle EffectiveStyle = NodeStyle;
				if (!EffectiveStyle.bHasStroke && EffectiveStyle.bHasFill && EffectiveStyle.FillColor.TrimStartAndEnd().ToLower() != TEXT("none"))
				{
					EffectiveStyle.StrokeColor = EffectiveStyle.FillColor;
					EffectiveStyle.bHasStroke = true;
				}

				if (EffectiveStyle.bHasStroke || EffectiveStyle.bHasFill)
				{
					ParsePathData(PathData, Context, EffectiveStyle, OutSegments, InOutError);
				}
			}
		}
		else if (Tag == TEXT("line"))
		{
			double X1 = 0.0;
			double Y1 = 0.0;
			double X2 = 0.0;
			double Y2 = 0.0;
			if (ParseNumericAttribute(Node->GetAttribute(TEXT("x1")), X1)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("y1")), Y1)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("x2")), X2)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("y2")), Y2))
			{
				AppendSegment(FVector2D(static_cast<float>(X1), static_cast<float>(Y1)), FVector2D(static_cast<float>(X2), static_cast<float>(Y2)), Context, NodeStyle, OutSegments);
			}
		}
		else if (Tag == TEXT("polyline") || Tag == TEXT("polygon"))
		{
			TArray<FVector2D> Points;
			if (ParsePointListAttribute(Node->GetAttribute(TEXT("points")), Points))
			{
				AppendPolyline(Points, Tag == TEXT("polygon"), Context, NodeStyle, OutSegments);
			}
		}
		else if (Tag == TEXT("circle"))
		{
			double CX = 0.0;
			double CY = 0.0;
			double Radius = 0.0;
			if (ParseNumericAttribute(Node->GetAttribute(TEXT("cx")), CX)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("cy")), CY)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("r")), Radius))
			{
				AppendEllipseOutline(FVector2D(static_cast<float>(CX), static_cast<float>(CY)), static_cast<float>(Radius), static_cast<float>(Radius), Context, NodeStyle, OutSegments);
			}
		}
		else if (Tag == TEXT("ellipse"))
		{
			double CX = 0.0;
			double CY = 0.0;
			double RadiusX = 0.0;
			double RadiusY = 0.0;
			if (ParseNumericAttribute(Node->GetAttribute(TEXT("cx")), CX)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("cy")), CY)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("rx")), RadiusX)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("ry")), RadiusY))
			{
				AppendEllipseOutline(FVector2D(static_cast<float>(CX), static_cast<float>(CY)), static_cast<float>(RadiusX), static_cast<float>(RadiusY), Context, NodeStyle, OutSegments);
			}
		}
		else if (Tag == TEXT("rect"))
		{
			double X = 0.0;
			double Y = 0.0;
			double Width = 0.0;
			double Height = 0.0;
			if (ParseNumericAttribute(Node->GetAttribute(TEXT("x")), X)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("y")), Y)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("width")), Width)
				&& ParseNumericAttribute(Node->GetAttribute(TEXT("height")), Height))
			{
				TArray<FVector2D> RectPoints;
				RectPoints.Add(FVector2D(static_cast<float>(X), static_cast<float>(Y)));
				RectPoints.Add(FVector2D(static_cast<float>(X + Width), static_cast<float>(Y)));
				RectPoints.Add(FVector2D(static_cast<float>(X + Width), static_cast<float>(Y + Height)));
				RectPoints.Add(FVector2D(static_cast<float>(X), static_cast<float>(Y + Height)));
				AppendPolyline(RectPoints, true, Context, NodeStyle, OutSegments);
			}
		}

		for (const FXmlNode* ChildNode : Node->GetChildrenNodes())
		{
			TraverseSvgNode(ChildNode, Context, NodeStyle, OutSegments, InOutError);
			if (!InOutError.IsEmpty())
			{
				return;
			}
		}
	}
}

bool FDNGSvgParser::ParseSvgToSegments(const FString& Svg, const FDNGSvgParseOptions& Options, TArray<FDNGDrawSegment>& OutSegments, FString& OutError)
{
	OutSegments.Reset();
	OutError.Reset();

	const FString TrimmedSvg = Svg.TrimStartAndEnd();
	if (TrimmedSvg.IsEmpty())
	{
		OutError = TEXT("SVG content was empty.");
		return false;
	}

	FXmlFile XmlFile(TrimmedSvg, EConstructMethod::ConstructFromBuffer);
	if (!XmlFile.IsValid())
	{
		OutError = FString::Printf(TEXT("SVG XML parse failed: %s"), *XmlFile.GetLastError());
		return false;
	}

	const FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode || GetLocalTagName(RootNode->GetTag()) != TEXT("svg"))
	{
		OutError = TEXT("SVG root element was missing.");
		return false;
	}

	FDNGSvgPathContext Context;
	Context.Options = Options;
	if (!ParseViewBox(RootNode, Context.ViewBox))
	{
		OutError = TEXT("SVG viewBox was invalid.");
		return false;
	}

	FDNGSvgStyle RootStyle;
	RootStyle.bHasStroke = true;
	RootStyle.StrokeColor = TEXT("black");
	RootStyle.bHasStrokeWidth = true;
	RootStyle.StrokeWidth = Options.MediumThickness;

	TraverseSvgNode(RootNode, Context, RootStyle, OutSegments, OutError);
	if (!OutError.IsEmpty())
	{
		OutSegments.Reset();
		return false;
	}

	if (OutSegments.Num() == 0)
	{
		OutError = TEXT("SVG did not contain any drawable path strokes.");
		return false;
	}

	return true;
}
