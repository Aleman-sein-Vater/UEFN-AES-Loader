#pragma once
#include "pch.h"
#include <string>
#include <stdexcept>
#include <iostream>
#include <optional>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <cstdint>
#include <concepts>
#include <type_traits>
#include <random>

// Thank you Dumper-7 and Unreal Engine for letting me Skid.

#ifndef TEXT
#define TEXT(x) L##x
#endif

typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

namespace UtfN
{
	template<typename StrType = std::string>
	inline StrType Utf16StringToUtf8String(const wchar_t* Str, int Len)
	{
		if (!Str || Len <= 0)
			return StrType();
		StrType Out;
		Out.reserve(static_cast<size_t>(Len));
		for (int i = 0; i < Len; ++i)
		{
			uint32_t cp = static_cast<uint16_t>(Str[i]);
			if (cp == 0)
				break;
			if (cp >= 0xD800 && cp <= 0xDBFF && (i + 1) < Len)
			{
				uint32_t low = static_cast<uint16_t>(Str[i + 1]);
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					cp = (((cp - 0xD800) << 10) | (low - 0xDC00)) + 0x10000;
					++i;
				}
			}
			if (cp <= 0x7F)
			{
				Out.push_back(static_cast<char>(cp));
			}
			else if (cp <= 0x7FF)
			{
				Out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
				Out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
			}
			else if (cp <= 0xFFFF)
			{
				Out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
				Out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
				Out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
			}
			else if (cp <= 0x10FFFF)
			{
				Out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
				Out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
				Out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
				Out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
			}
		}
		return Out;
	}
}

template<typename ArrayElementType>
class TArray;

namespace Iterators
{
	template<typename ArrayType>
	class TArrayIterator;
}

template<typename ArrayElementType>
class TArray
{
protected:
	static constexpr uint64 ElementAlign = alignof(ArrayElementType);
	static constexpr uint64 ElementSize = sizeof(ArrayElementType);

	ArrayElementType* Data;
	int32 NumElements;
	int32 MaxElements;

public:
	TArray()
		: Data(nullptr), NumElements(0), MaxElements(0)
	{
	}

	TArray(ArrayElementType* InData, int32 InNumElements, int32 InMaxElements)
		: Data(InData), NumElements(InNumElements), MaxElements(InMaxElements)
	{
	}

	TArray(const TArray&) = default;
	TArray(TArray&&) = default;
	TArray& operator=(TArray&&) = default;
	TArray& operator=(const TArray&) = default;

private:
	inline int32 GetSlack() const { return MaxElements - NumElements; }
	inline void VerifyIndex(int32 Index) const { if (!IsValidIndex(Index)) throw std::out_of_range("Index was out of range!"); }
	inline       ArrayElementType& GetUnsafe(int32 Index)       { return Data[Index]; }
	inline const ArrayElementType& GetUnsafe(int32 Index) const { return Data[Index]; }

public:
	inline bool Reserve(int32 NewMax)
	{
		if (NewMax <= MaxElements)
			return true;
		ArrayElementType* NewData = static_cast<ArrayElementType*>(std::realloc(Data, static_cast<size_t>(NewMax) * ElementSize));
		if (!NewData)
			return false;
		Data = NewData;
		MaxElements = NewMax;
		return true;
	}

	inline void SetNum(int32 NewNum)
	{
		if (NewNum > MaxElements)
			Reserve(NewNum);
		NumElements = NewNum;
	}

	inline bool Add(const ArrayElementType& Element)
	{
		if (GetSlack() <= 0)
		{
			const int32 NewMax = MaxElements > 0 ? (MaxElements * 2) : 4;
			if (!Reserve(NewMax))
				return false;
		}
		Data[NumElements] = Element;
		NumElements++;
		return true;
	}

	inline void FreeArray()
	{
		if (Data)
			std::free(Data);
		Data = nullptr;
		NumElements = 0;
		MaxElements = 0;
	}

	inline bool Remove(int32 Index)
	{
		if (!IsValidIndex(Index))
			return false;
		NumElements--;
		for (int i = Index; i < NumElements; i++)
			Data[i] = Data[i + 1];
		return true;
	}

	inline void Clear()
	{
		if (Data && NumElements > 0)
			memset(Data, 0, NumElements * ElementSize);
		NumElements = 0;
	}

	inline int32 Num() const { return NumElements; }
	inline int32 Max() const { return MaxElements; }
	inline bool IsEmpty() const { return NumElements == 0; }

	inline       ArrayElementType* GetData()          { return Data; }
	inline const ArrayElementType* GetData() const    { return Data; }
	inline const ArrayElementType* GetDataPtr() const { return Data; }

	inline std::optional<ArrayElementType> Find(const ArrayElementType& Element, bool (* const Equals)(const ArrayElementType& Left, const ArrayElementType& Right)) const
	{
		for (int32 i = 0; i < NumElements; i++)
		{
			if (Equals(Data[i], Element))
				return Data[i];
		}
		return std::nullopt;
	}

	inline std::optional<ArrayElementType> Find(const ArrayElementType& Element) const requires (std::equality_comparable<ArrayElementType>)
	{
		return Find(Element, [](const ArrayElementType& Left, const ArrayElementType& Right) -> bool { return Left == Right; });
	}

	inline bool Contains(const ArrayElementType& Element, bool (* const Equals)(const ArrayElementType& Left, const ArrayElementType& Right)) const
	{
		return Find(Element, Equals).has_value();
	}

	inline bool Contains(const ArrayElementType& Element) const requires (std::equality_comparable<ArrayElementType>)
	{
		return Find(Element).has_value();
	}

	inline bool IsValidIndex(int32 Index) const { return Data && Index >= 0 && Index < NumElements; }
	inline bool IsValid() const { return Data && NumElements > 0 && MaxElements >= NumElements; }

	inline       ArrayElementType& operator[](int32 Index)       { VerifyIndex(Index); return Data[Index]; }
	inline const ArrayElementType& operator[](int32 Index) const { VerifyIndex(Index); return Data[Index]; }

	inline bool operator==(const TArray<ArrayElementType>& Other) const { return Data == Other.Data; }
	inline bool operator!=(const TArray<ArrayElementType>& Other) const { return Data != Other.Data; }
	inline explicit operator bool() const { return IsValid(); }

	template<typename T> friend Iterators::TArrayIterator<T> begin(const TArray<T>& Array);
	template<typename T> friend Iterators::TArrayIterator<T> end  (const TArray<T>& Array);
};

class FString : public TArray<wchar_t>
{
public:
	friend std::ostream& operator<<(std::ostream& Stream, const FString& Str) { return Stream << Str.ToString(); }

	using TArray::TArray;

	FString(const wchar_t* Str)
	{
		const uint32 NullTerminatedLength = Str ? static_cast<uint32>(wcslen(Str) + 0x1) : 0;
		Data = const_cast<wchar_t*>(Str);
		NumElements = NullTerminatedLength;
		MaxElements = NullTerminatedLength;
	}

	FString(const wchar_t* Str, int32 Size)
	{
		Data = const_cast<wchar_t*>(Str);
		NumElements = Size;
		MaxElements = Size;
	}

	FString(wchar_t* Str, int32 Num, int32 Max)
	{
		Data = Str;
		NumElements = Num;
		MaxElements = Max;
	}

	inline std::string ToString() const
	{
		if (*this)
			return UtfN::Utf16StringToUtf8String<std::string>(Data, NumElements - 1);
		return "";
	}

	inline std::wstring ToWString() const
	{
		if (*this)
			return std::wstring(Data);
		return L"";
	}

	inline       wchar_t* CStr()       { return Data; }
	inline const wchar_t* CStr() const { return Data; }
	inline const wchar_t* c_str() const { return Data ? Data : L""; }
	inline const wchar_t* operator*() const { return Data ? Data : L""; }
	inline int32 Len() const { return Data ? static_cast<int32>(wcslen(Data)) : 0; }

	inline int32 ParseIntoArray(TArray<FString>& OutArray, const wchar_t* pchDelim, bool InCullEmpty = true) const
	{
		OutArray.Clear();
		if (!Data || !pchDelim || *pchDelim == L'\0')
			return 0;

		const wchar_t* Start = Data;
		const size_t DelimLen = wcslen(pchDelim);
		if (DelimLen == 0)
			return 0;

		auto AddToken = [&](const wchar_t* TokenStart, size_t TokenLen)
		{
			if (TokenLen == 0 && InCullEmpty)
				return;
			FString Token;
			Token.Reserve(static_cast<int32>(TokenLen + 1));
			for (size_t i = 0; i < TokenLen; ++i)
				Token.Add(TokenStart[i]);
			Token.Add(L'\0');
			OutArray.Add(Token);
		};

		const wchar_t* Found = wcsstr(Start, pchDelim);
		while (Found)
		{
			AddToken(Start, static_cast<size_t>(Found - Start));
			Start = Found + DelimLen;
			Found = wcsstr(Start, pchDelim);
		}

		AddToken(Start, wcslen(Start));
		return OutArray.Num();
	}

	inline bool operator==(const FString& Other) const { return Other ? NumElements == Other.NumElements && wcscmp(Data, Other.Data) == 0 : false; }
	inline bool operator!=(const FString& Other) const { return Other ? NumElements != Other.NumElements || wcscmp(Data, Other.Data) != 0 : true; }
};

namespace Iterators
{
	template<typename ArrayType>
	class TArrayIterator
	{
	private:
		TArray<ArrayType>& IteratedArray;
		int32 Index;

	public:
		TArrayIterator(const TArray<ArrayType>& Array, int32 StartIndex = 0x0)
			: IteratedArray(const_cast<TArray<ArrayType>&>(Array)), Index(StartIndex)
		{
		}

		inline int32 GetIndex() { return Index; }
		inline int32 IsValid() { return IteratedArray.IsValidIndex(GetIndex()); }

		inline TArrayIterator& operator++() { ++Index; return *this; }
		inline TArrayIterator& operator--() { --Index; return *this; }

		inline       ArrayType& operator*()       { return IteratedArray[GetIndex()]; }
		inline const ArrayType& operator*() const { return IteratedArray[GetIndex()]; }
		inline       ArrayType* operator->()       { return &IteratedArray[GetIndex()]; }
		inline const ArrayType* operator->() const { return &IteratedArray[GetIndex()]; }

		inline bool operator==(const TArrayIterator& Other) const { return &IteratedArray == &Other.IteratedArray && Index == Other.Index; }
		inline bool operator!=(const TArrayIterator& Other) const { return &IteratedArray != &Other.IteratedArray || Index != Other.Index; }
	};
}

template<typename T> inline Iterators::TArrayIterator<T> begin(const TArray<T>& Array) { return Iterators::TArrayIterator<T>(Array, 0); }
template<typename T> inline Iterators::TArrayIterator<T> end  (const TArray<T>& Array) { return Iterators::TArrayIterator<T>(Array, Array.Num()); }

/**
 * Enumerates known GUID formats.
 */
enum class EGuidFormats
{
	/**
	 * 32 digits.
	 *
	 * For example: "00000000000000000000000000000000"
	 */
	Digits,

	/**
	 * 32 digits in lowercase
	 *
	 * For example: "0123abc456def789abcd123ef4a5b6c7"
	 */
	DigitsLower,

	/**
	 * 32 digits separated by hyphens.
	 *
	 * For example: 00000000-0000-0000-0000-000000000000
	 */
	DigitsWithHyphens,

	/**
	 * 32 digits separated by hyphens, in lowercase as described by RFC 4122.
	 *
	 * For example: bd048ce3-358b-46c5-8cee-627c719418f8
	 */
	DigitsWithHyphensLower,

	/**
	 * 32 digits separated by hyphens and enclosed in braces.
	 *
	 * For example: {00000000-0000-0000-0000-000000000000}
	 */
	DigitsWithHyphensInBraces,

	/**
	 * 32 digits separated by hyphens and enclosed in parentheses.
	 *
	 * For example: (00000000-0000-0000-0000-000000000000)
	 */
	DigitsWithHyphensInParentheses,

	/**
	 * Comma-separated hexadecimal values enclosed in braces.
	 *
	 * For example: {0x00000000,0x0000,0x0000,{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}}
	 */
	HexValuesInBraces,

	/**
	 * This format is currently used by the FUniqueObjectGuid class.
	 *
	 * For example: 00000000-00000000-00000000-00000000
	 */
	UniqueObjectGuid,
};

/**
 * Implements a globally unique identifier.
 */
struct FGuid
{
public:
	/** Default constructor. */
	constexpr FGuid()
		: A(0)
		, B(0)
		, C(0)
		, D(0)
	{
	}

	/**
	 * Creates and initializes a new GUID from the specified components.
	 *
	 * @param InA The first component.
	 * @param InB The second component.
	 * @param InC The third component.
	 * @param InD The fourth component.
	 */
	explicit constexpr FGuid(uint32_t InA, uint32_t InB, uint32_t InC, uint32_t InD)
		: A(InA), B(InB), C(InC), D(InD)
	{
	}

	/**
	 * Creates a GUID parsed from a string, or an invalid GUID if the string could not be parsed.
	 */
	template <typename StringType>
	explicit constexpr FGuid(const StringType& InGuidStr)
		: A(0), B(0), C(0), D(0)
	{
		if (!Parse(InGuidStr, *this))
		{
			Invalidate();
		}
	}

public:
	/**
	 * Compares two GUIDs for equality.
	 *
	 * @param X The first GUID to compare.
	 * @param Y The second GUID to compare.
	 * @return true if the GUIDs are equal, false otherwise.
	 */
	friend constexpr bool operator==(const FGuid& X, const FGuid& Y)
	{
		return ((X.A ^ Y.A) | (X.B ^ Y.B) | (X.C ^ Y.C) | (X.D ^ Y.D)) == 0;
	}

	/**
	 * Compares two GUIDs for inequality.
	 *
	 * @param X The first GUID to compare.
	 * @param Y The second GUID to compare.
	 * @return true if the GUIDs are not equal, false otherwise.
	 */
	friend constexpr bool operator!=(const FGuid& X, const FGuid& Y)
	{
		return ((X.A ^ Y.A) | (X.B ^ Y.B) | (X.C ^ Y.C) | (X.D ^ Y.D)) != 0;
	}

	/**
	 * Compares two GUIDs.
	 *
	 * @param X The first GUID to compare.
	 * @param Y The second GUID to compare.
	 * @return true if the first GUID is less than the second one.
	 */
	friend constexpr bool operator<(const FGuid& X, const FGuid& Y)
	{
		return	((X.A < Y.A) ? true : ((X.A > Y.A) ? false :
				((X.B < Y.B) ? true : ((X.B > Y.B) ? false :
				((X.C < Y.C) ? true : ((X.C > Y.C) ? false :
				((X.D < Y.D) ? true : ((X.D > Y.D) ? false : false)))))))); //-V583
	}

	/**
	 * Provides access to the GUIDs components.
	 *
	 * @param Index The index of the component to return (0...3).
	 * @return The component.
	 */
	constexpr uint32_t& operator[](int32_t Index)
	{
		switch (Index)
		{
		case 0: return A;
		case 1: return B;
		case 2: return C;
		case 3: return D;
		default: return A;
		}
	}

	/**
	 * Provides read-only access to the GUIDs components.
	 *
	 * @param Index The index of the component to return (0...3).
	 * @return The component.
	 */
	constexpr const uint32_t& operator[](int32_t Index) const
	{
		switch (Index)
		{
		case 0: return A;
		case 1: return B;
		case 2: return C;
		case 3: return D;
		default: return A;
		}
	}

	/**
	 * Guid default string conversion.
	 */
	friend FString LexToString(const FGuid& Value)
	{
		return Value.ToString();
	}

	template <typename StringType>
	friend void LexFromString(FGuid& Result, const StringType& String)
	{
		FGuid::Parse(String, Result);
	}

public:
	/**
	 * Invalidates the GUID.
	 *
	 * @see IsValid
	 */
	constexpr void Invalidate()
	{
		A = B = C = D = 0;
	}

	/**
	 * Checks whether this GUID is valid or not.
	 *
	 * A GUID that has all its components set to zero is considered invalid.
	 *
	 * @return true if valid, false otherwise.
	 * @see Invalidate
	 */
	constexpr bool IsValid() const
	{
		return ((A | B | C | D) != 0);
	}

	/**
	 * Converts this GUID to its string representation.
	 *
	 * @param Format The string format to use.
	 * @return The string representation.
	 */
	FString ToString(EGuidFormats Format = EGuidFormats::Digits) const
	{
		FString Out;
		AppendString(Out, Format);
		return Out;
	}

	/**
	 * Converts this GUID to its string representation using the specified format.
	 *
	 * @param Format The string format to use.
	 */
	void AppendString(FString& Out, EGuidFormats Format = EGuidFormats::Digits) const
	{
		char Buf[70] = {};
		const int32_t Len = FormatToBuffer(Buf, Format);

		if (Out.Num() > 0 && Out[Out.Num() - 1] == L'\0')
		{
			Out.Remove(Out.Num() - 1);
		}

		Out.Reserve(Out.Num() + Len + 1);
		for (int32_t i = 0; i < Len; ++i)
		{
			Out.Add(static_cast<wchar_t>(Buf[i]));
		}
		Out.Add(L'\0');
	}

public:
	/**
	 * Calculates the hash for a GUID.
	 *
	 * @param Guid The GUID to calculate the hash for.
	 * @return The hash.
	 */
	[[nodiscard]] friend constexpr uint32_t GetTypeHash(const FGuid& Guid)
	{
		uint32_t Hash = Guid.A;
		Hash ^= Guid.B + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
		Hash ^= Guid.C + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
		Hash ^= Guid.D + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
		return Hash;
	}

public:
	/**
	 * Returns a new GUID.
	 *
	 * @return A new GUID.
	 */
	static FGuid NewGuid()
	{
		FGuid Result(0, 0, 0, 0);

		std::random_device Rd;
		Result.A = Rd();
		Result.B = (Rd() & ~0x0000f000u) | 0x00004000u; // Version 4
		Result.C = (Rd() & ~0xc0000000u) | 0x80000000u; // Variant 1 (RFC 4122 UUID)
		Result.D = Rd();

		return Result;
	}

	/**
	 * Returns a GUID which is a combination of the two provided ones.
	 *
	 * @return The combined GUID.
	 */
	static constexpr FGuid Combine(const FGuid& GuidA, const FGuid& GuidB)
	{
		auto HashCombine = [](uint32_t A, uint32_t B) constexpr -> uint32_t
		{
			B += 0x9e3779b9u + (A << 6) + (A >> 2);
			return A ^ B;
		};

		return FGuid(
			HashCombine(GuidA.A, GuidB.A),
			HashCombine(GuidA.B, GuidB.B),
			HashCombine(GuidA.C, GuidB.C),
			HashCombine(GuidA.D, GuidB.D)
		);
	}

	/**
	 * Returns a GUID which is a combination of the two provided ones, ignoring any invalid ones in the process.
	 *
	 * @return The combined GUID, invalid if both inputs are invalid.
	 */
	static constexpr FGuid CombineValid(const FGuid& GuidA, const FGuid& GuidB)
	{
		if (GuidA.IsValid())
		{
			if (GuidB.IsValid())
			{
				return Combine(GuidA, GuidB);
			}

			return GuidA;
		}

		return GuidB;
	}

	/**
	 * Converts a string to a GUID.
	 *
	 * @param GuidString The string to convert.
	 * @param OutGuid Will contain the parsed GUID.
	 * @return true if the string was converted successfully, false otherwise.
	 * @see ParseExact, ToString
	 */
	template <typename StringType>
	static constexpr bool Parse(const StringType& GuidString, FGuid& OutGuid)
	{
		const auto* Ptr = ExtractPtr(GuidString);
		if (!Ptr)
		{
			return false;
		}

		const int32_t Len = ExtractLen(GuidString, Ptr);
		switch (Len)
		{
		case 32: return ParseExactBuffer(Ptr, Len, EGuidFormats::Digits, OutGuid);
		case 35: return ParseExactBuffer(Ptr, Len, EGuidFormats::UniqueObjectGuid, OutGuid);
		case 36: return ParseExactBuffer(Ptr, Len, EGuidFormats::DigitsWithHyphens, OutGuid);
		case 38: return ParseExactBuffer(Ptr, Len, Ptr[0] == '{' ? EGuidFormats::DigitsWithHyphensInBraces : EGuidFormats::DigitsWithHyphensInParentheses, OutGuid);
		case 68: return ParseExactBuffer(Ptr, Len, EGuidFormats::HexValuesInBraces, OutGuid);
		default: return false;
		}
	}

	/**
	 * Converts a string with the specified format to a GUID.
	 *
	 * @param GuidString The string to convert.
	 * @param Format The string format to parse.
	 * @param OutGuid Will contain the parsed GUID.
	 * @return true if the string was converted successfully, false otherwise.
	 * @see Parse, ToString
	 */
	template <typename StringType>
	static constexpr bool ParseExact(const StringType& GuidString, EGuidFormats Format, FGuid& OutGuid)
	{
		const auto* Ptr = ExtractPtr(GuidString);
		if (!Ptr)
		{
			return false;
		}
		return ParseExactBuffer(Ptr, ExtractLen(GuidString, Ptr), Format, OutGuid);
	}

private:
	static constexpr char GetGuidNibbleCharFromPart(const uint32_t Part, const uint32_t Index, const bool bLower)
	{
		const uint32_t NibbleIndex = 7u - Index;
		const uint32_t PartShift = NibbleIndex << 2u;
		const uint32_t Nibble = (Part >> PartShift) & 15u;
		if (Nibble < 10u)
		{
			return static_cast<char>('0' + Nibble);
		}
		return static_cast<char>((bLower ? 'a' : 'A') + (Nibble - 10u));
	}

	constexpr int32_t FormatToBuffer(char* Out, EGuidFormats Format) const
	{
		const uint32_t Parts[4] = { A, B, C, D };
		char* Ptr = Out;
		const bool bLower = (Format == EGuidFormats::DigitsLower || Format == EGuidFormats::DigitsWithHyphensLower);

		// "%08x-%04x-%04x-%04x-%04x%08x"
		auto AppendHexDigitsWithHyphens = [&]()
		{
			for (uint32_t PartIndex = 0; PartIndex < 4; ++PartIndex)
			{
				const uint32_t& Part = Parts[PartIndex];
				for (uint32_t i = 0; i < 4; ++i)
				{
					*Ptr++ = GetGuidNibbleCharFromPart(Part, i, bLower);
				}
				if (PartIndex == 1 || PartIndex == 2)
				{
					*Ptr++ = '-';
				}
				for (uint32_t i = 4; i < 8; ++i)
				{
					*Ptr++ = GetGuidNibbleCharFromPart(Part, i, bLower);
				}
				if (PartIndex == 0 || PartIndex == 1)
				{
					*Ptr++ = '-';
				}
			}
		};

		switch (Format)
		{
		case EGuidFormats::DigitsWithHyphens:
			// "%08X-%04X-%04X-%04X-%04X%08X"
			AppendHexDigitsWithHyphens();
			break;

		case EGuidFormats::DigitsWithHyphensLower:
			// "%08x-%04x-%04x-%04x-%04x%08x"
			AppendHexDigitsWithHyphens();
			break;

		case EGuidFormats::DigitsWithHyphensInBraces:
			// "{%08X-%04X-%04X-%04X-%04X%08X}"
			*Ptr++ = '{';
			AppendHexDigitsWithHyphens();
			*Ptr++ = '}';
			break;

		case EGuidFormats::DigitsWithHyphensInParentheses:
			// "(%08X-%04X-%04X-%04X-%04X%08X)"
			*Ptr++ = '(';
			AppendHexDigitsWithHyphens();
			*Ptr++ = ')';
			break;

		case EGuidFormats::UniqueObjectGuid:
			// "%08X-%08X-%08X-%08X"
			for (uint32_t PartIndex = 0; PartIndex < 4; ++PartIndex)
			{
				const uint32_t& Part = Parts[PartIndex];
				for (uint32_t i = 0; i < 8; ++i)
				{
					*Ptr++ = GetGuidNibbleCharFromPart(Part, i, false);
				}
				if (PartIndex != 3)
				{
					*Ptr++ = '-';
				}
			}
			break;

		case EGuidFormats::HexValuesInBraces:
		{
			// "{0x%08X,0x%04X,0x%04X,{0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X,0x%02X}}"
			auto AppendLiteral = [&](const char* Str) { while (*Str) *Ptr++ = *Str++; };
			auto AppendNibbles = [&](uint32_t Part, uint32_t Start, uint32_t End)
			{
				for (uint32_t i = Start; i < End; ++i)
				{
					*Ptr++ = GetGuidNibbleCharFromPart(Part, i, false);
				}
			};

			// {0xAAAAAAAA
			AppendLiteral("{0x");
			AppendNibbles(A, 0, 8);

			// ,0xBBBB
			AppendLiteral(",0x");
			AppendNibbles(B, 0, 4);

			// ,0xCCCC
			AppendLiteral(",0x");
			AppendNibbles(B, 4, 8);

			// ,{0xDD,0xEE,0xFF,0xGG  (C bytes)
			AppendLiteral(",{0x");
			AppendNibbles(C, 0, 2);
			AppendLiteral(",0x");
			AppendNibbles(C, 2, 4);
			AppendLiteral(",0x");
			AppendNibbles(C, 4, 6);
			AppendLiteral(",0x");
			AppendNibbles(C, 6, 8);

			// ,0xHH,0xII,0xJJ,0xKK  (D bytes)
			AppendLiteral(",0x");
			AppendNibbles(D, 0, 2);
			AppendLiteral(",0x");
			AppendNibbles(D, 2, 4);
			AppendLiteral(",0x");
			AppendNibbles(D, 4, 6);
			AppendLiteral(",0x");
			AppendNibbles(D, 6, 8);

			// }}
			*Ptr++ = '}';
			*Ptr++ = '}';
			break;
		}

		case EGuidFormats::DigitsLower:
			// "%08x%08x%08x%08x"
			for (const uint32_t& Part : Parts)
			{
				for (uint32_t i = 0; i < 8; ++i)
				{
					*Ptr++ = GetGuidNibbleCharFromPart(Part, i, true);
				}
			}
			break;

		case EGuidFormats::Digits:
		default:
			// "%08X%08X%08X%08X"
			for (const uint32_t& Part : Parts)
			{
				for (uint32_t i = 0; i < 8; ++i)
				{
					*Ptr++ = GetGuidNibbleCharFromPart(Part, i, false);
				}
			}
			break;
		}

		*Ptr = '\0';
		return static_cast<int32_t>(Ptr - Out);
	}

	template <typename StringType>
	static constexpr const auto* ExtractPtr(const StringType& Str)
	{
		if constexpr (std::is_pointer_v<std::decay_t<StringType>>)
		{
			return Str;
		}
		else if constexpr (requires { Str.GetData(); })
		{
			return Str.GetData();
		}
		else if constexpr (requires { Str.Data(); })
		{
			return Str.Data();
		}
		else if constexpr (requires { Str.CStr(); })
		{
			return Str.CStr();
		}
		else if constexpr (requires { Str.c_str(); })
		{
			return Str.c_str();
		}
		else if constexpr (requires { *Str; })
		{
			return *Str;
		}
		else if constexpr (requires { &Str[0]; })
		{
			return &Str[0];
		}
	}

	template <typename StringType, typename CharType>
	static constexpr int32_t ExtractLen(const StringType& Str, const CharType* Ptr)
	{
		if constexpr (!std::is_pointer_v<std::decay_t<StringType>> && requires { Str.Len(); })
		{
			return static_cast<int32_t>(Str.Len());
		}
		else
		{
			int32_t Len = 0;
			while (Ptr[Len] != 0)
			{
				++Len;
			}
			return Len;
		}
	}

	/** Compile-time and runtime parser that supports all hex-based EGuidFormats. */
	template <typename CharType>
	static constexpr bool ParseExactBuffer(const CharType* Str, int32_t Len, EGuidFormats Format, FGuid& OutGuid)
	{
		// 'X' is a hex digit and every other character must match exactly.
		const char* Pattern = nullptr;
		int32_t ExpectedLen = 0;

		switch (Format)
		{
		case EGuidFormats::Digits:
		case EGuidFormats::DigitsLower:
			Pattern = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
			ExpectedLen = 32;
			break;
		case EGuidFormats::UniqueObjectGuid:
			Pattern = "XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX";
			ExpectedLen = 35;
			break;
		case EGuidFormats::DigitsWithHyphens:
		case EGuidFormats::DigitsWithHyphensLower:
			Pattern = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";
			ExpectedLen = 36;
			break;
		case EGuidFormats::DigitsWithHyphensInBraces:
			Pattern = "{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX}";
			ExpectedLen = 38;
			break;
		case EGuidFormats::DigitsWithHyphensInParentheses:
			Pattern = "(XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX)";
			ExpectedLen = 38;
			break;
		case EGuidFormats::HexValuesInBraces:
			Pattern = "{0xXXXXXXXX,0xXXXX,0xXXXX,{0xXX,0xXX,0xXX,0xXX,0xXX,0xXX,0xXX,0xXX}}";
			ExpectedLen = 68;
			break;
		default:
			return false;
		}

		if (Len != ExpectedLen)
		{
			return false;
		}

		uint32_t Words[4] = {};
		int32_t DigitIndex = 0;

		for (int32_t CharIndex = 0; CharIndex < Len; ++CharIndex)
		{
			const CharType Char = Str[CharIndex];
			if (Pattern[CharIndex] != 'X')
			{
				if (Char != static_cast<CharType>(Pattern[CharIndex]))
				{
					return false;
				}
				continue;
			}

			uint32_t Digit = 0;
			if (Char >= '0' && Char <= '9')
			{
				Digit = static_cast<uint32_t>(Char - '0');
			}
			else if (Char >= 'a' && Char <= 'f')
			{
				Digit = static_cast<uint32_t>(Char - 'a' + 10);
			}
			else if (Char >= 'A' && Char <= 'F')
			{
				Digit = static_cast<uint32_t>(Char - 'A' + 10);
			}
			else
			{
				return false;
			}

			uint32_t& Word = Words[DigitIndex / 8];
			Word = (Word << 4) | Digit;
			++DigitIndex;
		}

		OutGuid = FGuid(Words[0], Words[1], Words[2], Words[3]);
		return true;
	}

public:
	/** Holds the first component. */
	uint32_t A;

	/** Holds the second component. */
	uint32_t B;

	/** Holds the third component. */
	uint32_t C;

	/** Holds the fourth component. */
	uint32_t D;
};

struct FAES
{
	static constexpr uint32 AESBlockSize = 16;

	/** 
	 * Class representing a 256 bit AES key
	 */
	struct FAESKey
	{
		static constexpr int32 KeySize = 32;

		uint8 Key[KeySize];

		FAESKey()
		{
			Reset();
		}

		bool IsValid() const
		{
			const uint32* Words = reinterpret_cast<const uint32*>(Key);
			for (int32 Index = 0; Index < KeySize / 4; ++Index)
			{
				if (Words[Index] != 0)
				{
					return true;
				}
			}
			return false;
		}

		void Reset()
		{
			std::memset(Key, 0, KeySize);
		}

		bool operator==(const FAESKey& Other) const
		{
			return std::memcmp(Key, Other.Key, KeySize) == 0;
		}
	};
};

/** Mode to use for base64 encoding and decoding */
enum class EBase64Mode : uint8
{
	/** Use the standard set of character mappings. (Table 1 from RFC 4648, known as "base64") */
	Standard,
	/** Use the "URL and Filename safe" set of character mappings. (Table 2 from RFC 4648, known as "base64url") */
	UrlSafe
};

/**
 * Class for encoding/decoding Base64 data (RFC 4648)
 */
struct FBase64
{
private:
	/** The table used to encode a 6 bit value as an ascii character using Table 1 from RFC 4648 ("base64") */
	static constexpr uint8 GBase64EncodingAlphabet[64] =
	{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
	};

	/** The table used to encode a 6 bit value as an ascii character using Table 2 from RFC 4648 ("base64url") */
	static constexpr uint8 GBase64UrlSafeEncodingAlphabet[64] =
	{
		'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
		'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
		'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
		'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-', '_'
	};

	/** The table used to convert an ascii character into a 6 bit value using Table 1 from RFC 4648 ("base64") */
	static constexpr uint8 GBase64DecodingAlphabet[128] =
	{
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x00-0x0f
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x10-0x1f
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, 0xFF, 0x3F, // 0x20-0x2f
		0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x30-0x3f
		0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, // 0x40-0x4f
		0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x50-0x5f
		0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // 0x60-0x6f
		0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // 0x70-0x7f
	};

	/** The table used to convert an ascii character into a 6 bit value using Table 2 from RFC 4648 ("base64url") */
	static constexpr uint8 GBase64UrlSafeDecodingAlphabet[128] =
	{
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x00-0x0f
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x10-0x1f
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3E, 0xFF, 0xFF, // 0x20-0x2f
		0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x30-0x3f
		0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, // 0x40-0x4f
		0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, // 0x50-0x5f
		0xFF, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, // 0x60-0x6f
		0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // 0x70-0x7f
	};

public:
	/**
	 * Encodes a binary uint8 array into a Base64 string
	 *
	 * @param Source The binary data to convert
	 * @param Mode The mode to use for encoding. Default is EBase64Mode::Standard
	 *
	 * @return A string that encodes the binary data in a way that can be safely transmitted via various Internet protocols
	 */
	static FString Encode(const TArray<uint8>& Source, EBase64Mode Mode = EBase64Mode::Standard)
	{
		return Encode(Source.GetData(), static_cast<uint32>(Source.Num()), Mode);
	}

	/**
	 * Encodes the source into a Base64 string
	 *
	 * @param Source The binary data to encode
	 * @param Length Length of the binary data to be encoded
	 * @param Mode The mode to use for encoding. Default is EBase64Mode::Standard
	 *
	 * @return Base64 encoded string containing the binary data.
	 */
	static FString Encode(const uint8* Source, uint32 Length, EBase64Mode Mode = EBase64Mode::Standard)
	{
		const uint32 ExpectedLength = GetEncodedDataSize(Length);

		FString OutBuffer;
		OutBuffer.SetNum(static_cast<int32>(ExpectedLength + 1));
		Encode(Source, Length, OutBuffer.GetData(), Mode);

		return OutBuffer;
	}

	/**
	 * Encodes the source into a Base64 string, storing it in a preallocated buffer.
	 *
	 * @param Source The binary data to encode
	 * @param Length Length of the binary data to be encoded
	 * @param Dest Buffer to receive the encoded data. Must be large enough to contain the entire output data (see GetEncodedDataSize()). Can point to the same buffer as Source
	 * @param Mode The mode to use for encoding. Default is EBase64Mode::Standard
	 *
	 * @return The length of the encoded data
	 */
	template<typename CharType>
	static uint32 Encode(const uint8* Source, uint32 Length, CharType* Dest, EBase64Mode Mode = EBase64Mode::Standard)
	{
		const uint8* const EncodingAlphabet = (Mode == EBase64Mode::UrlSafe ? GBase64UrlSafeEncodingAlphabet : GBase64EncodingAlphabet);

		const uint32 PaddingLength = Length % 3;
		const uint32 ExpectedLength = GetEncodedDataSize(Length);
		CharType* EncodedBytes = Dest + ExpectedLength;

		// Add a null terminator
		*EncodedBytes = static_cast<CharType>('\0');

		if (Length == 0)
		{
			return 0;
		}

		const uint8* ReversedSource = Source + Length;

		// Since this algorithm operates on blocks, we may need to pad the last chunks
		if (PaddingLength > 0)
		{
			EncodedBytes -= 4;

			uint8 A = 0;
			uint8 B = 0;
			uint8 C = 0;
			// Grab the second character if it is a 2 uint8 finish
			if (PaddingLength == 2)
			{
				B = *--ReversedSource;
			}
			A = *--ReversedSource;

			uint32 ByteTriplet = A << 16 | B << 8 | C;
			// Pad with = to make a 4 uint8 chunk
			EncodedBytes[3] = static_cast<CharType>('=');
			ByteTriplet >>= 6;
			// If there's only one 1 uint8 left in the source, then you need 2 pad chars
			if (PaddingLength == 1)
			{
				EncodedBytes[2] = static_cast<CharType>('=');
			}
			else
			{
				EncodedBytes[2] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
			}
			// Now encode the remaining bits the same way
			ByteTriplet >>= 6;
			EncodedBytes[1] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
			ByteTriplet >>= 6;
			EncodedBytes[0] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
		}

		Length -= PaddingLength;

		// Loop through the buffer converting 3 bytes of binary data at a time
		while (Length > 0)
		{
			EncodedBytes -= 4;

			uint8 C = *--ReversedSource;
			uint8 B = *--ReversedSource;
			uint8 A = *--ReversedSource;
			Length -= 3;

			// The algorithm takes 24 bits of data (3 bytes) and breaks it into 4 6bit chunks represented as ascii
			uint32 ByteTriplet = A << 16 | B << 8 | C;

			// Use the 6bit block to find the representation ascii character for it
			EncodedBytes[3] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
			ByteTriplet >>= 6;
			EncodedBytes[2] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
			ByteTriplet >>= 6;
			EncodedBytes[1] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
			ByteTriplet >>= 6;
			EncodedBytes[0] = static_cast<CharType>(EncodingAlphabet[ByteTriplet & 0x3F]);
		}

		return ExpectedLength;
	}

	/**
	 * Get the encoded data size for the given number of bytes.
	 *
	 * @param NumBytes The number of bytes of input
	 *
	 * @return The number of characters in the encoded data.
	 */
	static inline constexpr uint32 GetEncodedDataSize(uint32 NumBytes)
	{
		return ((NumBytes + 2) / 3) * 4;
	}

	/**
	 * Decodes a Base64 string into an array of bytes
	 *
	 * @param Source The Base64 encoded string
	 * @param OutDest Array to receive the decoded data
	 * @param Mode The mode to use for decoding. Default is EBase64Mode::Standard
	 *
	 * @return true if the buffer was decoded, false if it was invalid.
	 */
	static bool Decode(const FString& Source, TArray<uint8>& OutDest, EBase64Mode Mode = EBase64Mode::Standard)
	{
		OutDest.SetNum(static_cast<int32>(GetDecodedDataSize(Source)));
		return Decode(*Source, static_cast<uint32>(Source.Len()), OutDest.GetData(), Mode);
	}

	/**
	 * Decodes a Base64 string into a preallocated buffer
	 *
	 * @param Source The Base64 encoded string
	 * @param Length Length of the Base64 encoded string
	 * @param Dest Buffer to receive the decoded data. Must be large enough to contain the entire output data (see GetDecodedDataSize()). Can point to the same buffer as Source
	 * @param Mode The mode to use for decoding. Default is EBase64Mode::Standard
	 *
	 * @return true if the buffer was decoded, false if it was invalid.
	 */
	template<typename CharType>
	static bool Decode(const CharType* Source, uint32 Length, uint8* Dest, EBase64Mode Mode = EBase64Mode::Standard)
	{
		// Remove the trailing '=' characters, so we can handle padded and non-padded input the same
		while (Length > 0 && Source[Length - 1] == '=')
		{
			Length--;
		}

		// Make sure the length is valid. Only lengths modulo 4 of 0, 2, and 3 are valid.
		if ((Length & 3) == 1)
		{
			return false;
		}

		const uint8* const DecodingAlphabet = (Mode == EBase64Mode::UrlSafe ? GBase64UrlSafeDecodingAlphabet : GBase64DecodingAlphabet);

		// Convert all the full chunks of data
		for (; Length >= 4; Length -= 4)
		{
			// Decode the next 4 BYTEs
			uint32 OriginalTriplet = 0;
			for (int32 Index = 0; Index < 4; Index++)
			{
				uint32 SourceChar = static_cast<uint32>(*Source++);
				if (SourceChar >= 128)
				{
					return false;
				}
				uint8 DecodedValue = DecodingAlphabet[SourceChar];
				if (DecodedValue == 0xFF)
				{
					return false;
				}
				OriginalTriplet = (OriginalTriplet << 6) | DecodedValue;
			}

			// Now we can tear the uint32 into bytes
			Dest[2] = OriginalTriplet & 0xFF;
			OriginalTriplet >>= 8;
			Dest[1] = OriginalTriplet & 0xFF;
			OriginalTriplet >>= 8;
			Dest[0] = OriginalTriplet & 0xFF;

			// Move to the next output chunk
			Dest += 3;
		}

		// Convert the last chunk of data
		if (Length > 0)
		{
			// Decode the next 4 BYTEs, or up to the end of the input buffer
			uint32 OriginalTriplet = 0;
			for (uint32 Index = 0; Index < Length; Index++)
			{
				uint32 SourceChar = static_cast<uint32>(*Source++);
				if (SourceChar >= 128)
				{
					return false;
				}
				uint8 DecodedValue = DecodingAlphabet[SourceChar];
				if (DecodedValue == 0xFF)
				{
					return false;
				}
				OriginalTriplet = (OriginalTriplet << 6) | DecodedValue;
			}
			for (int32 Index = Length; Index < 4; Index++)
			{
				OriginalTriplet = (OriginalTriplet << 6);
			}

			// Now we can tear the uint32 into bytes
			OriginalTriplet >>= 8;
			if (Length >= 3)
			{
				Dest[1] = OriginalTriplet & 0xFF;
			}
			OriginalTriplet >>= 8;
			Dest[0] = OriginalTriplet & 0xFF;
		}
		return true;
	}

	/**
	 * Determine the decoded data size for the incoming base64 encoded string
	 *
	 * @param Source The Base64 encoded string
	 *
	 * @return The size in bytes of the decoded data
	 */
	static uint32 GetDecodedDataSize(const FString& Source)
	{
		return GetDecodedDataSize(*Source, static_cast<uint32>(Source.Len()));
	}

	/**
	 * Determine the decoded data size for the incoming base64 encoded string
	 *
	 * @param Source The Base64 encoded string
	 * @param Length Length of the Base64 encoded string
	 *
	 * @return The size in bytes of the decoded data
	 */
	template<typename CharType>
	static uint32 GetDecodedDataSize(const CharType* Source, uint32 Length)
	{
		uint32 NumBytes = 0;
		if (Length > 0)
		{
			// Get the source length without the trailing padding characters
			while (Length > 0 && Source[Length - 1] == '=')
			{
				Length--;
			}

			// Get the lower bound for number of bytes, excluding the last partial chunk
			NumBytes += (Length / 4) * 3;
			if ((Length & 3) == 3)
			{
				NumBytes += 2;
			}
			if ((Length & 3) == 2)
			{
				NumBytes++;
			}
		}
		return NumBytes;
	}

	/**
	 * Get the maximum decoded data size for the given number of input characters.
	 *
	 * @param NumChars The number of input characters.
	 *
	 * @return The maximum number of bytes that can be decoded from this input stream.
	 */
	static inline constexpr uint32 GetMaxDecodedDataSize(uint32 NumChars)
	{
		return ((NumChars * 3) + 3) / 4;
	}
};
