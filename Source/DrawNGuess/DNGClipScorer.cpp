#include "DNGClipScorer.h"

#include "ImageUtils.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNETypes.h"

namespace
{
	constexpr int32 GClipImageSize = 224;
	constexpr int32 GClipEmbeddingFallbackSize = 512;
	constexpr int32 GClipContextLength = 77;
	constexpr float GClipMean[3] = { 0.48145466f, 0.45782750f, 0.40821073f };
	constexpr float GClipStd[3] = { 0.26862954f, 0.26130258f, 0.27577711f };
}

void UDNGClipScorer::Configure(const FString& InRuntimeName, TSoftObjectPtr<UNNEModelData> InImageEncoderModel, TSoftObjectPtr<UNNEModelData> InTextEncoderModel, const FString& InTokenizerDirectory)
{
	RuntimeName = InRuntimeName.IsEmpty() ? TEXT("NNERuntimeORTCpu") : InRuntimeName;
	ImageEncoderModel = InImageEncoderModel;
	TextEncoderModel = InTextEncoderModel;
	TokenizerDirectory = InTokenizerDirectory;

	ImageModel.Reset();
	ImageInstance.Reset();
	TextModel.Reset();
	TextInstance.Reset();
	bImageModelInitialized = false;
	bTextModelInitialized = false;
	Tokenizer = FDNGClipTokenizer();
}

bool UDNGClipScorer::Initialize(FString& OutError)
{
	if (!EnsureImageModel(OutError))
	{
		return false;
	}

	if (!TextEncoderModel.IsNull())
	{
		if (!EnsureTextModel(OutError))
		{
			return false;
		}
	}

	return true;
}

bool UDNGClipScorer::EncodeImageFile(const FString& ImagePath, TArray<float>& OutEmbedding, FString& OutError)
{
	OutEmbedding.Reset();

	if (!EnsureImageModel(OutError))
	{
		return false;
	}

	TArray<float> InputTensor;
	if (!BuildImageTensor(ImagePath, InputTensor, OutError))
	{
		return false;
	}

	return RunImageModel(InputTensor, OutEmbedding, OutError);
}

bool UDNGClipScorer::ScoreImageAgainstText(const FString& ImagePath, const FString& Text, float& OutScore, FString& OutError)
{
	OutScore = 0.0f;

	TArray<float> ImageEmbedding;
	if (!EncodeImageFile(ImagePath, ImageEmbedding, OutError))
	{
		return false;
	}

	TArray<float> TextEmbedding;
	if (!RunTextModel(Text, TextEmbedding, OutError))
	{
		return false;
	}

	OutScore = CosineSimilarity(ImageEmbedding, TextEmbedding);
	return true;
}

float UDNGClipScorer::CosineSimilarity(TConstArrayView<float> Left, TConstArrayView<float> Right)
{
	if (Left.Num() == 0 || Left.Num() != Right.Num())
	{
		return 0.0f;
	}

	double Dot = 0.0;
	double LeftNorm = 0.0;
	double RightNorm = 0.0;
	for (int32 Index = 0; Index < Left.Num(); ++Index)
	{
		Dot += static_cast<double>(Left[Index]) * static_cast<double>(Right[Index]);
		LeftNorm += static_cast<double>(Left[Index]) * static_cast<double>(Left[Index]);
		RightNorm += static_cast<double>(Right[Index]) * static_cast<double>(Right[Index]);
	}

	if (LeftNorm <= UE_DOUBLE_SMALL_NUMBER || RightNorm <= UE_DOUBLE_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return static_cast<float>(Dot / FMath::Sqrt(LeftNorm * RightNorm));
}

bool UDNGClipScorer::EnsureImageModel(FString& OutError)
{
	if (bImageModelInitialized && ImageInstance.IsValid())
	{
		return true;
	}

	if (ImageEncoderModel.IsNull())
	{
		OutError = TEXT("CLIP image encoder model is not assigned.");
		return false;
	}

	const TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(RuntimeName);
	if (!Runtime.IsValid())
	{
		OutError = FString::Printf(TEXT("NNE runtime '%s' is not available."), *RuntimeName);
		return false;
	}

	UNNEModelData* ImageModelData = ImageEncoderModel.LoadSynchronous();
	if (!ImageModelData)
	{
		OutError = TEXT("Failed to load CLIP image encoder model asset.");
		return false;
	}

	if (Runtime->CanCreateModelCPU(ImageModelData) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("The selected image encoder model cannot be created by the active NNE runtime.");
		return false;
	}

	ImageModel = Runtime->CreateModelCPU(ImageModelData);
	if (!ImageModel.IsValid())
	{
		OutError = TEXT("Failed to create the CLIP image encoder.");
		return false;
	}

	ImageInstance = ImageModel->CreateModelInstanceCPU();
	if (!ImageInstance.IsValid())
	{
		OutError = TEXT("Failed to create the CLIP image encoder instance.");
		return false;
	}

	const TArray<UE::NNE::FTensorShape> InputShapes = {
		UE::NNE::FTensorShape::Make({ 1u, 3u, static_cast<uint32>(GClipImageSize), static_cast<uint32>(GClipImageSize) })
	};
	if (ImageInstance->SetInputTensorShapes(InputShapes) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("Failed to set input tensor shapes for the CLIP image encoder.");
		ImageInstance.Reset();
		ImageModel.Reset();
		return false;
	}

	bImageModelInitialized = true;
	return true;
}

bool UDNGClipScorer::EnsureTextModel(FString& OutError)
{
	if (bTextModelInitialized && TextInstance.IsValid())
	{
		return true;
	}

	if (TextEncoderModel.IsNull())
	{
		OutError = TEXT("CLIP text encoder model is not assigned.");
		return false;
	}

	if (!Tokenizer.IsInitialized() && !Tokenizer.Initialize(TokenizerDirectory, OutError))
	{
		return false;
	}

	const TWeakInterfacePtr<INNERuntimeCPU> Runtime = UE::NNE::GetRuntime<INNERuntimeCPU>(RuntimeName);
	if (!Runtime.IsValid())
	{
		OutError = FString::Printf(TEXT("NNE runtime '%s' is not available."), *RuntimeName);
		return false;
	}

	UNNEModelData* TextModelData = TextEncoderModel.LoadSynchronous();
	if (!TextModelData)
	{
		OutError = TEXT("Failed to load CLIP text encoder model asset.");
		return false;
	}

	if (Runtime->CanCreateModelCPU(TextModelData) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("The selected text encoder model cannot be created by the active NNE runtime.");
		return false;
	}

	TextModel = Runtime->CreateModelCPU(TextModelData);
	if (!TextModel.IsValid())
	{
		OutError = TEXT("Failed to create the CLIP text encoder.");
		return false;
	}

	TextInstance = TextModel->CreateModelInstanceCPU();
	if (!TextInstance.IsValid())
	{
		OutError = TEXT("Failed to create the CLIP text encoder instance.");
		return false;
	}

	TArray<UE::NNE::FTensorShape> InputShapes;
	const TConstArrayView<UE::NNE::FTensorDesc> InputTensorDescs = TextInstance->GetInputTensorDescs();
	for (int32 Index = 0; Index < InputTensorDescs.Num(); ++Index)
	{
		InputShapes.Add(UE::NNE::FTensorShape::Make({ 1u, static_cast<uint32>(GClipContextLength) }));
	}

	if (InputShapes.IsEmpty() || TextInstance->SetInputTensorShapes(InputShapes) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("Failed to set input tensor shapes for the CLIP text encoder.");
		TextInstance.Reset();
		TextModel.Reset();
		return false;
	}

	bTextModelInitialized = true;
	return true;
}

bool UDNGClipScorer::BuildImageTensor(const FString& ImagePath, TArray<float>& OutTensor, FString& OutError) const
{
	OutTensor.Reset();

	FImage LoadedImage;
	if (!FImageUtils::LoadImage(*ImagePath, LoadedImage))
	{
		OutError = FString::Printf(TEXT("Failed to load image '%s'."), *ImagePath);
		return false;
	}

	LoadedImage.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);

	const int32 CropSize = FMath::Min(LoadedImage.SizeX, LoadedImage.SizeY);
	const int32 OffsetX = (LoadedImage.SizeX - CropSize) / 2;
	const int32 OffsetY = (LoadedImage.SizeY - CropSize) / 2;

	FImage CroppedImage(CropSize, CropSize, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const TArrayView64<const FColor> SourcePixels = LoadedImage.AsBGRA8();
	TArrayView64<FColor> CroppedPixels = CroppedImage.AsBGRA8();

	for (int32 Y = 0; Y < CropSize; ++Y)
	{
		for (int32 X = 0; X < CropSize; ++X)
		{
			const int64 SourceIndex = static_cast<int64>(Y + OffsetY) * LoadedImage.SizeX + (X + OffsetX);
			const int64 DestIndex = static_cast<int64>(Y) * CropSize + X;
			CroppedPixels[DestIndex] = SourcePixels[SourceIndex];
		}
	}

	FImage ResizedImage;
	FImageCore::ResizeImageAllocDest(CroppedImage, ResizedImage, GClipImageSize, GClipImageSize, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const TArrayView64<const FColor> ResizedPixels = ResizedImage.AsBGRA8();

	OutTensor.SetNumZeroed(3 * GClipImageSize * GClipImageSize);
	const int32 PlaneSize = GClipImageSize * GClipImageSize;
	for (int32 Y = 0; Y < GClipImageSize; ++Y)
	{
		for (int32 X = 0; X < GClipImageSize; ++X)
		{
			const int32 PixelIndex = Y * GClipImageSize + X;
			const FColor Pixel = ResizedPixels[PixelIndex];
			const float Red = static_cast<float>(Pixel.R) / 255.0f;
			const float Green = static_cast<float>(Pixel.G) / 255.0f;
			const float Blue = static_cast<float>(Pixel.B) / 255.0f;

			OutTensor[PixelIndex] = (Red - GClipMean[0]) / GClipStd[0];
			OutTensor[PlaneSize + PixelIndex] = (Green - GClipMean[1]) / GClipStd[1];
			OutTensor[(2 * PlaneSize) + PixelIndex] = (Blue - GClipMean[2]) / GClipStd[2];
		}
	}

	return true;
}

bool UDNGClipScorer::RunImageModel(const TArray<float>& InputTensor, TArray<float>& OutEmbedding, FString& OutError)
{
	if (!ImageInstance.IsValid())
	{
		OutError = TEXT("CLIP image encoder is not initialized.");
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorShape> OutputShapes = ImageInstance->GetOutputTensorShapes();
	const int32 OutputSize = (OutputShapes.Num() > 0)
		? static_cast<int32>(OutputShapes[0].Volume())
		: GClipEmbeddingFallbackSize;

	OutEmbedding.SetNumZeroed(OutputSize);

	const TArray<UE::NNE::FTensorBindingCPU> InputBindings = {
		{ const_cast<float*>(InputTensor.GetData()), static_cast<uint64>(InputTensor.Num() * sizeof(float)) }
	};
	const TArray<UE::NNE::FTensorBindingCPU> OutputBindings = {
		{ OutEmbedding.GetData(), static_cast<uint64>(OutEmbedding.Num() * sizeof(float)) }
	};

	if (ImageInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("Running the CLIP image encoder failed.");
		OutEmbedding.Reset();
		return false;
	}

	NormalizeInPlace(OutEmbedding);
	return true;
}

bool UDNGClipScorer::TokenizeText(const FString& Text, TArray<int64>& OutInputIds, TArray<int64>& OutAttentionMask, FString& OutError) const
{
	return Tokenizer.Encode(Text, OutInputIds, OutAttentionMask, OutError);
}

bool UDNGClipScorer::RunTextModel(const FString& Text, TArray<float>& OutEmbedding, FString& OutError)
{
	OutEmbedding.Reset();

	if (!EnsureTextModel(OutError))
	{
		return false;
	}

	TArray<int64> InputIds;
	TArray<int64> AttentionMask;
	if (!TokenizeText(Text, InputIds, AttentionMask, OutError))
	{
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorShape> OutputShapes = TextInstance->GetOutputTensorShapes();
	const int32 SequenceOutputSize = (OutputShapes.Num() > 0)
		? static_cast<int32>(OutputShapes[0].Volume())
		: (GClipContextLength * GClipEmbeddingFallbackSize);
	TArray<float> SequenceEmbedding;
	SequenceEmbedding.SetNumZeroed(SequenceOutputSize);

	TArray<UE::NNE::FTensorBindingCPU> InputBindings;
	InputBindings.Add({ InputIds.GetData(), static_cast<uint64>(InputIds.Num() * sizeof(int64)) });
	if (TextInstance->GetInputTensorDescs().Num() > 1)
	{
		InputBindings.Add({ AttentionMask.GetData(), static_cast<uint64>(AttentionMask.Num() * sizeof(int64)) });
	}

	const TArray<UE::NNE::FTensorBindingCPU> OutputBindings = {
		{ SequenceEmbedding.GetData(), static_cast<uint64>(SequenceEmbedding.Num() * sizeof(float)) }
	};

	if (TextInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("Running the CLIP text encoder failed.");
		return false;
	}

	const int32 HiddenSize = GClipEmbeddingFallbackSize;
	if (SequenceEmbedding.Num() % HiddenSize != 0)
	{
		OutError = FString::Printf(TEXT("Unexpected CLIP text output size: %d"), SequenceEmbedding.Num());
		return false;
	}

	const int32 EosIndex = ResolveEosIndex(InputIds, AttentionMask);
	const int32 SequenceLength = SequenceEmbedding.Num() / HiddenSize;
	if (EosIndex < 0 || EosIndex >= SequenceLength)
	{
		OutError = FString::Printf(TEXT("Resolved invalid eos index %d for sequence length %d"), EosIndex, SequenceLength);
		return false;
	}

	OutEmbedding.SetNumUninitialized(HiddenSize);
	const int32 StartIndex = EosIndex * HiddenSize;
	for (int32 Index = 0; Index < HiddenSize; ++Index)
	{
		OutEmbedding[Index] = SequenceEmbedding[StartIndex + Index];
	}

	NormalizeInPlace(OutEmbedding);
	return true;
}

int32 UDNGClipScorer::ResolveEosIndex(TConstArrayView<int64> InputIds, TConstArrayView<int64> AttentionMask) const
{
	const int32 EosTokenId = Tokenizer.GetEosTokenId();
	for (int32 Index = 1; Index < InputIds.Num(); ++Index)
	{
		if (InputIds[Index] == EosTokenId)
		{
			if (!AttentionMask.IsValidIndex(Index) || AttentionMask[Index] == 1)
			{
				return Index;
			}
			return FMath::Max(0, Index - 1);
		}
	}

	for (int32 Index = 0; Index < AttentionMask.Num(); ++Index)
	{
		if (AttentionMask[Index] == 0)
		{
			return FMath::Max(0, Index - 1);
		}
	}

	return FMath::Max(0, InputIds.Num() - 1);
}

void UDNGClipScorer::NormalizeInPlace(TArray<float>& Values)
{
	double SquaredNorm = 0.0;
	for (const float Value : Values)
	{
		SquaredNorm += static_cast<double>(Value) * static_cast<double>(Value);
	}

	if (SquaredNorm <= UE_DOUBLE_SMALL_NUMBER)
	{
		return;
	}

	const float Scale = 1.0f / static_cast<float>(FMath::Sqrt(SquaredNorm));
	for (float& Value : Values)
	{
		Value *= Scale;
	}
}
