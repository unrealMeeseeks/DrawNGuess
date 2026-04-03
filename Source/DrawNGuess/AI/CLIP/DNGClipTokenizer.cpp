#include "DNGClipTokenizer.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// CLIP text encoder constants and helpers used by the native tokenizer implementation.
	constexpr int32 GClipContextLength = 77;
	const TCHAR* GBosToken = TEXT("<|startoftext|>");
	const TCHAR* GEosToken = TEXT("<|endoftext|>");

	FString CodepointToString(uint32 Codepoint)
	{
		FString Result;
		Result.AppendChar(static_cast<TCHAR>(Codepoint));
		return Result;
	}
}

// Loads tokenizer assets from disk and prepares all lookup tables required for encoding.
bool FDNGClipTokenizer::Initialize(const FString& InTokenizerDirectory, FString& OutError)
{
	TokenizerDirectory = InTokenizerDirectory;
	if (TokenizerDirectory.IsEmpty())
	{
		TokenizerDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("CLIP"));
	}
	else if (FPaths::IsRelative(TokenizerDirectory))
	{
		TokenizerDirectory = FPaths::Combine(FPaths::ProjectDir(), TokenizerDirectory);
	}

	TokenizerDirectory = FPaths::ConvertRelativePathToFull(TokenizerDirectory);
	const FString VocabPath = FPaths::Combine(TokenizerDirectory, TEXT("vocab.json"));
	const FString MergesPath = FPaths::Combine(TokenizerDirectory, TEXT("merges.txt"));

	Vocabulary.Reset();
	MergeRanks.Reset();
	ByteEncoder.Reset();
	BpeCache.Reset();
	BosTokenId = INDEX_NONE;
	EosTokenId = INDEX_NONE;

	if (!LoadVocabulary(VocabPath, OutError))
	{
		return false;
	}

	if (!LoadMerges(MergesPath, OutError))
	{
		return false;
	}

	BuildByteEncoder();

	const int32* BosTokenIdPtr = Vocabulary.Find(GBosToken);
	const int32* EosTokenIdPtr = Vocabulary.Find(GEosToken);
	if (!BosTokenIdPtr || !EosTokenIdPtr)
	{
		OutError = TEXT("Failed to locate CLIP special tokens in vocab.json.");
		return false;
	}

	BosTokenId = *BosTokenIdPtr;
	EosTokenId = *EosTokenIdPtr;
	bInitialized = true;
	return true;
}

// Encodes one prompt into the fixed-size token id and attention mask tensors expected by CLIP.
bool FDNGClipTokenizer::Encode(const FString& Text, TArray<int64>& OutInputIds, TArray<int64>& OutAttentionMask, FString& OutError) const
{
	OutInputIds.Reset();
	OutAttentionMask.Reset();

	if (!bInitialized)
	{
		OutError = TEXT("CLIP tokenizer is not initialized.");
		return false;
	}

	TArray<int32> TokenIds;
	TokenIds.Reserve(GClipContextLength);
	TokenIds.Add(BosTokenId);

	for (const FString& Word : RegexLikeSplit(NormalizeInputText(Text)))
	{
		const FString EncodedWord = EncodeBytesToUnicode(Word);
		const FString BpeResult = ApplyBPE(EncodedWord);

		TArray<FString> BpeTokens;
		BpeResult.ParseIntoArrayWS(BpeTokens);
		for (const FString& BpeToken : BpeTokens)
		{
			if (const int32* TokenId = Vocabulary.Find(BpeToken))
			{
				TokenIds.Add(*TokenId);
			}
			else
			{
				TokenIds.Add(EosTokenId);
			}

			if (TokenIds.Num() >= GClipContextLength - 1)
			{
				break;
			}
		}

		if (TokenIds.Num() >= GClipContextLength - 1)
		{
			break;
		}
	}

	TokenIds.Add(EosTokenId);
	if (TokenIds.Num() > GClipContextLength)
	{
		TokenIds.SetNum(GClipContextLength);
		TokenIds.Last() = EosTokenId;
	}

	OutInputIds.Reserve(GClipContextLength);
	OutAttentionMask.Reserve(GClipContextLength);

	for (int32 Index = 0; Index < TokenIds.Num(); ++Index)
	{
		OutInputIds.Add(TokenIds[Index]);
		OutAttentionMask.Add(1);
	}

	while (OutInputIds.Num() < GClipContextLength)
	{
		OutInputIds.Add(EosTokenId);
		OutAttentionMask.Add(0);
	}

	return true;
}

// Loads the vocabulary file that maps token strings to integer ids.
bool FDNGClipTokenizer::LoadVocabulary(const FString& VocabPath, FString& OutError)
{
	FString VocabJson;
	if (!FFileHelper::LoadFileToString(VocabJson, *VocabPath))
	{
		OutError = FString::Printf(TEXT("Failed to read CLIP vocab file: %s"), *VocabPath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(VocabJson);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		OutError = FString::Printf(TEXT("Failed to parse CLIP vocab JSON: %s"), *VocabPath);
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : RootObject->Values)
	{
		const int32 TokenId = static_cast<int32>(Entry.Value->AsNumber());
		Vocabulary.Add(Entry.Key, TokenId);
	}

	return Vocabulary.Num() > 0;
}

// Loads the ordered merge rules used by byte-pair encoding.
bool FDNGClipTokenizer::LoadMerges(const FString& MergesPath, FString& OutError)
{
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *MergesPath))
	{
		OutError = FString::Printf(TEXT("Failed to read CLIP merges file: %s"), *MergesPath);
		return false;
	}

	int32 Rank = 0;
	for (const FString& RawLine : Lines)
	{
		const FString Line = RawLine.TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}

		TArray<FString> Parts;
		Line.ParseIntoArrayWS(Parts);
		if (Parts.Num() != 2)
		{
			continue;
		}

		MergeRanks.Add(MakePairKey(Parts[0], Parts[1]), Rank++);
	}

	return MergeRanks.Num() > 0;
}

// Builds the reversible byte-to-unicode mapping used by GPT/CLIP tokenizers.
void FDNGClipTokenizer::BuildByteEncoder()
{
	TArray<int32> ByteValues;
	for (int32 Value = 33; Value <= 126; ++Value)
	{
		ByteValues.Add(Value);
	}
	for (int32 Value = 161; Value <= 172; ++Value)
	{
		ByteValues.Add(Value);
	}
	for (int32 Value = 174; Value <= 255; ++Value)
	{
		ByteValues.Add(Value);
	}

	TArray<int32> UnicodeValues = ByteValues;
	int32 ExtraIndex = 0;
	for (int32 ByteValue = 0; ByteValue <= 255; ++ByteValue)
	{
		if (!ByteValues.Contains(ByteValue))
		{
			ByteValues.Add(ByteValue);
			UnicodeValues.Add(256 + ExtraIndex++);
		}
	}

	for (int32 Index = 0; Index < ByteValues.Num(); ++Index)
	{
		ByteEncoder.Add(static_cast<uint8>(ByteValues[Index]), CodepointToString(static_cast<uint32>(UnicodeValues[Index])));
	}
}

// Performs a CLIP-friendly token split before byte encoding and BPE.
TArray<FString> FDNGClipTokenizer::RegexLikeSplit(const FString& Text) const
{
	TArray<FString> Tokens;

	int32 Index = 0;
	while (Index < Text.Len())
	{
		const TCHAR Current = Text[Index];
		if (FChar::IsWhitespace(Current))
		{
			++Index;
			continue;
		}

		const FString Remaining = Text.Mid(Index);
		if (Remaining.StartsWith(TEXT("'s")) || Remaining.StartsWith(TEXT("'t")) || Remaining.StartsWith(TEXT("'m")) || Remaining.StartsWith(TEXT("'d")))
		{
			Tokens.Add(Text.Mid(Index, 2));
			Index += 2;
			continue;
		}
		if (Remaining.StartsWith(TEXT("'re")) || Remaining.StartsWith(TEXT("'ve")) || Remaining.StartsWith(TEXT("'ll")))
		{
			Tokens.Add(Text.Mid(Index, 3));
			Index += 3;
			continue;
		}

		if (FChar::IsAlpha(Current))
		{
			const int32 Start = Index;
			while (Index < Text.Len() && FChar::IsAlpha(Text[Index]))
			{
				++Index;
			}
			Tokens.Add(Text.Mid(Start, Index - Start));
			continue;
		}

		if (FChar::IsDigit(Current))
		{
			Tokens.Add(Text.Mid(Index, 1));
			++Index;
			continue;
		}

		const int32 Start = Index;
		while (Index < Text.Len() && !FChar::IsWhitespace(Text[Index]) && !FChar::IsAlpha(Text[Index]) && !FChar::IsDigit(Text[Index]))
		{
			const FString PunctuationRemainder = Text.Mid(Index);
			if (PunctuationRemainder.StartsWith(TEXT("'s")) || PunctuationRemainder.StartsWith(TEXT("'t")) || PunctuationRemainder.StartsWith(TEXT("'m")) || PunctuationRemainder.StartsWith(TEXT("'d")) || PunctuationRemainder.StartsWith(TEXT("'re")) || PunctuationRemainder.StartsWith(TEXT("'ve")) || PunctuationRemainder.StartsWith(TEXT("'ll")))
			{
				break;
			}
			++Index;
		}
		Tokens.Add(Text.Mid(Start, Index - Start));
	}

	return Tokens;
}

// Normalizes case and whitespace so tokenization matches the exported model's expectations.
FString FDNGClipTokenizer::NormalizeInputText(const FString& Text) const
{
	FString Lower = Text.ToLower();
	Lower.TrimStartAndEndInline();

	FString Result;
	Result.Reserve(Lower.Len());
	bool bPreviousWasWhitespace = false;
	for (TCHAR Character : Lower)
	{
		if (FChar::IsWhitespace(Character))
		{
			if (!bPreviousWasWhitespace)
			{
				Result.AppendChar(TEXT(' '));
				bPreviousWasWhitespace = true;
			}
		}
		else
		{
			Result.AppendChar(Character);
			bPreviousWasWhitespace = false;
		}
	}

	Result.TrimStartAndEndInline();
	return Result;
}

// Converts raw token bytes into the tokenizer's unicode-safe intermediate form.
FString FDNGClipTokenizer::EncodeBytesToUnicode(const FString& Token) const
{
	FTCHARToUTF8 Utf8(*Token);
	const uint8* Data = reinterpret_cast<const uint8*>(Utf8.Get());

	FString Result;
	for (int32 Index = 0; Index < Utf8.Length(); ++Index)
	{
		if (const FString* Mapped = ByteEncoder.Find(Data[Index]))
		{
			Result += *Mapped;
		}
	}

	return Result;
}

// Applies merge rules until the encoded token reaches its final BPE form.
FString FDNGClipTokenizer::ApplyBPE(const FString& Token) const
{
	if (const FString* Cached = BpeCache.Find(Token))
	{
		return *Cached;
	}

	TArray<FString> Word = SplitIntoSymbols(Token);
	if (Word.IsEmpty())
	{
		BpeCache.Add(Token, Token);
		return Token;
	}

	Word.Last() += TEXT("</w>");
	TSet<TPair<FString, FString>> Pairs = GetPairs(Word);
	if (Pairs.Num() == 0)
	{
		BpeCache.Add(Token, Word[0]);
		return Word[0];
	}

	while (true)
	{
		int32 BestRank = MAX_int32;
		TPair<FString, FString> BestPair;
		bool bFound = false;

		for (const TPair<FString, FString>& Pair : Pairs)
		{
			if (const int32* Rank = MergeRanks.Find(MakePairKey(Pair.Key, Pair.Value)))
			{
				if (*Rank < BestRank)
				{
					BestRank = *Rank;
					BestPair = Pair;
					bFound = true;
				}
			}
		}

		if (!bFound)
		{
			break;
		}

		TArray<FString> NewWord;
		for (int32 Index = 0; Index < Word.Num();)
		{
			if (Index < Word.Num() - 1 && Word[Index] == BestPair.Key && Word[Index + 1] == BestPair.Value)
			{
				NewWord.Add(Word[Index] + Word[Index + 1]);
				Index += 2;
			}
			else
			{
				NewWord.Add(Word[Index]);
				++Index;
			}
		}

		Word = MoveTemp(NewWord);
		if (Word.Num() == 1)
		{
			break;
		}

		Pairs = GetPairs(Word);
	}

	const FString Result = FString::Join(Word, TEXT(" "));
	BpeCache.Add(Token, Result);
	return Result;
}

// Splits the encoded token into its smallest mergeable symbol units.
TArray<FString> FDNGClipTokenizer::SplitIntoSymbols(const FString& EncodedToken) const
{
	TArray<FString> Symbols;
	Symbols.Reserve(EncodedToken.Len());
	for (TCHAR Character : EncodedToken)
	{
		Symbols.Add(FString(1, &Character));
	}
	return Symbols;
}

// Collects adjacent symbol pairs so the next merge candidate can be selected.
TSet<TPair<FString, FString>> FDNGClipTokenizer::GetPairs(const TArray<FString>& Symbols)
{
	TSet<TPair<FString, FString>> Result;
	if (Symbols.Num() < 2)
	{
		return Result;
	}

	for (int32 Index = 0; Index < Symbols.Num() - 1; ++Index)
	{
		Result.Add(TPair<FString, FString>(Symbols[Index], Symbols[Index + 1]));
	}

	return Result;
}

// Builds a stable key for merge-rank lookups.
FString FDNGClipTokenizer::MakePairKey(const FString& Left, const FString& Right)
{
	return Left + TEXT(" ") + Right;
}
