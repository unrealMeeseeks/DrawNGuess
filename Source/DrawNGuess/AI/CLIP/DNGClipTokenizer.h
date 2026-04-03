#pragma once

#include "CoreMinimal.h"

// Native CLIP byte-level BPE tokenizer used by the UE runtime.
class DRAWNGUESS_API FDNGClipTokenizer
{
public:
	// Loads tokenizer assets and prepares lookup tables.
	bool Initialize(const FString& InTokenizerDirectory, FString& OutError);

	// Encodes a text prompt into fixed-length token and mask tensors.
	bool Encode(const FString& Text, TArray<int64>& OutInputIds, TArray<int64>& OutAttentionMask, FString& OutError) const;

	bool IsInitialized() const { return bInitialized; }
	int32 GetEosTokenId() const { return EosTokenId; }

private:
	// Loads the vocabulary file that maps token strings to ids.
	bool LoadVocabulary(const FString& VocabPath, FString& OutError);

	// Loads the merge-rank table used by BPE.
	bool LoadMerges(const FString& MergesPath, FString& OutError);

	// Builds the reversible byte-to-unicode mapping used before BPE.
	void BuildByteEncoder();

	// Splits normalized input text into CLIP-like pre-BPE tokens.
	TArray<FString> RegexLikeSplit(const FString& Text) const;

	// Applies lowercase and whitespace normalization before tokenization.
	FString NormalizeInputText(const FString& Text) const;

	// Converts raw bytes into CLIP's unicode-safe representation.
	FString EncodeBytesToUnicode(const FString& Token) const;

	// Applies byte-pair merges to one encoded token.
	FString ApplyBPE(const FString& Token) const;

	// Splits an encoded token into mergeable symbols.
	TArray<FString> SplitIntoSymbols(const FString& EncodedToken) const;

	// Helper functions used during pair ranking/merging.
	static TSet<TPair<FString, FString>> GetPairs(const TArray<FString>& Symbols);
	static FString MakePairKey(const FString& Left, const FString& Right);

	// Indicates whether initialization completed successfully.
	bool bInitialized = false;

	// Root directory containing vocab.json and merges.txt.
	FString TokenizerDirectory;

	// Token string -> token id lookup table.
	TMap<FString, int32> Vocabulary;

	// Pair key -> merge rank lookup table.
	TMap<FString, int32> MergeRanks;

	// Byte -> unicode-safe symbol mapping.
	TMap<uint8, FString> ByteEncoder;

	// Cache of merged token strings to avoid repeated BPE work.
	mutable TMap<FString, FString> BpeCache;

	// Special CLIP token ids used for prompt framing.
	int32 BosTokenId = INDEX_NONE;
	int32 EosTokenId = INDEX_NONE;
};
