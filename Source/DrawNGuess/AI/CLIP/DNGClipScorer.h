#pragma once

#include "CoreMinimal.h"
#include "DNGClipTokenizer.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "DNGClipScorer.generated.h"

class UNNEModelData;

namespace UE::NNE
{
	class IModelCPU;
	class IModelInstanceCPU;
}

// Service object that wraps CLIP image/text inference through UE NNE.
UCLASS()
class DRAWNGUESS_API UDNGClipScorer : public UObject
{
	GENERATED_BODY()

public:
	// Supplies runtime, model, and tokenizer configuration before initialization.
	void Configure(const FString& InRuntimeName, TSoftObjectPtr<UNNEModelData> InImageEncoderModel, TSoftObjectPtr<UNNEModelData> InTextEncoderModel, const FString& InTokenizerDirectory);

	// Loads tokenizer data and creates model instances.
	bool Initialize(FString& OutError);

	// Encodes a saved image file into a normalized embedding.
	bool EncodeImageFile(const FString& ImagePath, TArray<float>& OutEmbedding, FString& OutError);

	// Scores one image file against one text prompt.
	bool ScoreImageAgainstText(const FString& ImagePath, const FString& Text, float& OutScore, FString& OutError);

	bool HasImageEncoder() const { return !ImageEncoderModel.IsNull(); }
	bool HasTextEncoder() const { return !TextEncoderModel.IsNull(); }

	// Computes cosine similarity between two vectors.
	static float CosineSimilarity(TConstArrayView<float> Left, TConstArrayView<float> Right);

private:
	// Lazily initializes the image encoder branch.
	bool EnsureImageModel(FString& OutError);

	// Lazily initializes the text encoder branch.
	bool EnsureTextModel(FString& OutError);

	// Reads a saved PNG and converts it into CLIP-normalized tensor data.
	bool BuildImageTensor(const FString& ImagePath, TArray<float>& OutTensor, FString& OutError) const;

	// Runs the image encoder synchronously.
	bool RunImageModel(const TArray<float>& InputTensor, TArray<float>& OutEmbedding, FString& OutError);

	// Tokenizes one text prompt into model-ready tensors.
	bool TokenizeText(const FString& Text, TArray<int64>& OutInputIds, TArray<int64>& OutAttentionMask, FString& OutError) const;

	// Resolves the EOS token position used for final text pooling.
	int32 ResolveEosIndex(TConstArrayView<int64> InputIds, TConstArrayView<int64> AttentionMask) const;

	// Runs the text encoder and extracts the pooled prompt embedding.
	bool RunTextModel(const FString& Text, TArray<float>& OutEmbedding, FString& OutError);

	// Normalizes an embedding in place before similarity comparison.
	static void NormalizeInPlace(TArray<float>& Values);

	// Imported NNE model asset for image encoding.
	UPROPERTY()
	TSoftObjectPtr<UNNEModelData> ImageEncoderModel;

	// Imported NNE model asset for text encoding.
	UPROPERTY()
	TSoftObjectPtr<UNNEModelData> TextEncoderModel;

	// Name of the NNE runtime used for ONNX execution.
	UPROPERTY()
	FString RuntimeName = TEXT("NNERuntimeORTCpu");

	// Directory containing tokenizer files.
	UPROPERTY()
	FString TokenizerDirectory;

	// Native tokenizer used by the text branch.
	FDNGClipTokenizer Tokenizer;

	// Cached runtime handles and instantiated execution objects.
	TSharedPtr<UE::NNE::IModelCPU> ImageModel;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ImageInstance;
	TSharedPtr<UE::NNE::IModelCPU> TextModel;
	TSharedPtr<UE::NNE::IModelInstanceCPU> TextInstance;

	// Separate flags allow one branch to fail without hiding the other.
	bool bImageModelInitialized = false;
	bool bTextModelInitialized = false;
};
