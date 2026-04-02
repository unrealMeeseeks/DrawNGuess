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

UCLASS()
class DRAWNGUESS_API UDNGClipScorer : public UObject
{
	GENERATED_BODY()

public:
	void Configure(const FString& InRuntimeName, TSoftObjectPtr<UNNEModelData> InImageEncoderModel, TSoftObjectPtr<UNNEModelData> InTextEncoderModel, const FString& InTokenizerDirectory);

	bool Initialize(FString& OutError);
	bool EncodeImageFile(const FString& ImagePath, TArray<float>& OutEmbedding, FString& OutError);
	bool ScoreImageAgainstText(const FString& ImagePath, const FString& Text, float& OutScore, FString& OutError);

	bool HasImageEncoder() const { return !ImageEncoderModel.IsNull(); }
	bool HasTextEncoder() const { return !TextEncoderModel.IsNull(); }

	static float CosineSimilarity(TConstArrayView<float> Left, TConstArrayView<float> Right);

private:
	bool EnsureImageModel(FString& OutError);
	bool EnsureTextModel(FString& OutError);
	bool BuildImageTensor(const FString& ImagePath, TArray<float>& OutTensor, FString& OutError) const;
	bool RunImageModel(const TArray<float>& InputTensor, TArray<float>& OutEmbedding, FString& OutError);
	bool TokenizeText(const FString& Text, TArray<int64>& OutInputIds, TArray<int64>& OutAttentionMask, FString& OutError) const;
	int32 ResolveEosIndex(TConstArrayView<int64> InputIds, TConstArrayView<int64> AttentionMask) const;
	bool RunTextModel(const FString& Text, TArray<float>& OutEmbedding, FString& OutError);
	static void NormalizeInPlace(TArray<float>& Values);

	UPROPERTY()
	TSoftObjectPtr<UNNEModelData> ImageEncoderModel;

	UPROPERTY()
	TSoftObjectPtr<UNNEModelData> TextEncoderModel;

	UPROPERTY()
	FString RuntimeName = TEXT("NNERuntimeORTCpu");

	UPROPERTY()
	FString TokenizerDirectory;

	FDNGClipTokenizer Tokenizer;

	TSharedPtr<UE::NNE::IModelCPU> ImageModel;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ImageInstance;
	TSharedPtr<UE::NNE::IModelCPU> TextModel;
	TSharedPtr<UE::NNE::IModelInstanceCPU> TextInstance;

	bool bImageModelInitialized = false;
	bool bTextModelInitialized = false;
};
