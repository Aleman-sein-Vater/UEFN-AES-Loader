#pragma once
#include "pch.h"

template <class ArrayType>
struct TArray
{
    TArray() { Data = nullptr; NumElements = MaxElements = 0; }
    ArrayType* Data;
    int NumElements, MaxElements;
    int GetSlack() const { return MaxElements - NumElements; }
    ArrayType& operator[](int i) { return Data[i]; }
    int Size() { return NumElements; }
    bool Valid(int i) { return i < NumElements; }
    template<size_t N>
    bool New(ArrayType(&elems)[N])
    {
        ArrayType* NewAllocation = new ArrayType[sizeof(ArrayType) * N];
        for (int i = 0; i < NumElements; i++) NewAllocation[i] = Data[i];
        Data = NewAllocation;
        NumElements = MaxElements = static_cast<int>(N);
        return true;
    }
    bool Add(ArrayType& Element)
    {
        if (GetSlack() <= 0) return false;
        Data[NumElements++] = Element;
        return true;
    }
    bool Remove(int Index)
    {
        if (!Valid(Index)) return false;
        for (int i = Index; i < NumElements - 1; i++) Data[i] = Data[i + 1];
        NumElements--;
        return true;
    }
};

struct FString : private TArray<wchar_t>
{
    FString() {}
    explicit FString(const wchar_t* Other)
    {
        NumElements = MaxElements = *Other ? static_cast<int>(wcslen(Other)) + 1 : 0;
        if (NumElements) Data = const_cast<wchar_t*>(Other);
    }
    explicit FString(const wchar_t* Other, int Size)
    {
        NumElements = MaxElements = Size;
        if (NumElements) Data = const_cast<wchar_t*>(Other);
    }
    operator bool() { return Data != nullptr; }
    wchar_t* c_str() { return Data; }
    int Size() { return NumElements; }
};