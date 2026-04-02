#pragma once

#include "CoreMinimal.h"

class DRAWNGUESS_API FDNGClipTokenizer
{
public:
	bool Initialize(const FString& InTokenizerDirectory, FString& OutError);
	bool Encode(const FString& Text, TArray<int64>& OutInputIds, TArray<int64>& OutAttentionMask, FString& OutError) const;

	bool IsInitialized() const { return bInitialized; }
	int32 GetEosTokenId() const { return EosTokenId; }

private:
	bool LoadVocabulary(const FString& VocabPath, FString& OutError);
	bool LoadMerges(const FString& MergesPath, FString& OutError);
	void BuildByteEncoder();
	TArray<FString> RegexLikeSplit(const FString& Text) const;
	FString NormalizeInputText(const FString& Text) const;
	FString EncodeBytesToUnicode(const FString& Token) const;
	FString ApplyBPE(const FString& Token) const;
	TArray<FString> SplitIntoSymbols(const FString& EncodedToken) const;
	static TSet<TPair<FString, FString>> GetPairs(const TArray<FString>& Symbols);
	static FString MakePairKey(const FString& Left, const FString& Right);

	bool bInitialized = false;
	FString TokenizerDirectory;
	TMap<FString, int32> Vocabulary;
	TMap<FString, int32> MergeRanks;
	TMap<uint8, FString> ByteEncoder;
	mutable TMap<FString, FString> BpeCache;
	int32 BosTokenId = INDEX_NONE;
	int32 EosTokenId = INDEX_NONE;
};
