// ReSharper disable CppClangTidyClangDiagnosticFloatConversion
// ReSharper disable CppClangTidyClangDiagnosticImplicitIntConversion
// ReSharper disable CppClangTidyBugproneNarrowingConversions
// ReSharper disable CppClangTidyClangDiagnosticImplicitIntFloatConversion
// ReSharper disable CppClangTidyClangDiagnosticFloatEqual
// ReSharper disable CppDefaultCaseNotHandledInSwitchStatement

#include "Distributions.h"

#include "DistributionFloat.h"
#include "DistributionFloatUniform.h"
#include "DistributionVector.h"
#include "DistributionVectorUniform.h"
#include "Particles/ParticleModules/ParticleModule.h"
#include "UObject/Property.h"

// UDistribution 객체들을 기본 서브 오브젝트가 되지 않도록 PostInitProps로 이동
// 에디터에서 0.0으로 반올림될 정도로 충분히 작은 값이지만,
// 초기화되지 않은 기본값을 제대로 감지할 수 있을 만큼은 충분히 큰 값
const float UDistribution::DefaultValue = 1.2345E-20f;

// The error threshold used when optimizing lookup table sample counts.
#define LOOKUP_TABLE_ERROR_THRESHOLD (0.05f)

enum : uint8
{
    // The maximum number of values to store in a lookup table.
    LOOKUP_TABLE_MAX_SAMPLES = 128,

    // UDistribution will bake out (if CanBeBaked returns true)
    DISTRIBUTIONS_BAKES_OUT = 1
};

// The maximum number of samples must be a power of two.
static_assert((LOOKUP_TABLE_MAX_SAMPLES & (LOOKUP_TABLE_MAX_SAMPLES - 1)) == 0, "Lookup table max samples is not a power of two.");


/*-----------------------------------------------------------------------------
	Lookup table related functions.
------------------------------------------------------------------------------*/
namespace
{
//@todo.CONSOLE: Until we have cooking or something in place, these need to be exposed.
/**
 * Builds a lookup table that returns a constant value.
 * @param OutTable - The table to build.
 * @param ValuesPerEntry - The number of values per entry in the table.
 * @param Values - The values to place in the table.
 */
void BuildConstantLookupTable(FDistributionLookupTable* OutTable, int32 ValuesPerEntry, const float* Values)
{
    assert(OutTable != nullptr);
    assert(Values != nullptr);

    OutTable->Values.Empty(ValuesPerEntry);
    OutTable->Values.AddUninitialized(ValuesPerEntry);
    OutTable->Op = RDO_None;
    OutTable->EntryCount = 1;
    OutTable->EntryStride = ValuesPerEntry;
    OutTable->SubEntryStride = 0;
    OutTable->TimeBias = 0.0f;
    OutTable->TimeScale = 0.0f;
    for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
    {
        OutTable->Values[ValueIndex] = Values[ValueIndex];
    }
}

/**
 * Builds a lookup table that returns zero.
 * @param OutTable - The table to build.
 * @param ValuesPerEntry - The number of values per entry in the table.
 */
void BuildZeroLookupTable(FDistributionLookupTable* OutTable, int32 ValuesPerEntry)
{
    assert(OutTable != nullptr);
    assert(ValuesPerEntry >= 1 && ValuesPerEntry <= 4);

    float Zero[4] = {0};
    BuildConstantLookupTable(OutTable, ValuesPerEntry, Zero);
}

/**
 * Builds a lookup table from a distribution.
 * @param OutTable - The table to build.
 * @param Distribution - The distribution for which to build a lookup table.
 */
template <typename DistributionType>
void BuildLookupTable(FDistributionLookupTable* OutTable, const DistributionType& Distribution)
{
    assert(Distribution);

    // Always clear the table.
    OutTable->Empty();

    // Nothing to do if we don't have a distribution.
    if (!Distribution->CanBeBaked())
    {
        BuildZeroLookupTable(OutTable, Distribution->GetValueCount());
        return;
    }

    // Always build a lookup table of maximal size. This can/will be optimized later.
    constexpr int32 EntryCount = LOOKUP_TABLE_MAX_SAMPLES;

    // Determine the domain of the distribution.
    float MinIn, MaxIn;
    Distribution->GetInRange(MinIn, MaxIn);
    const float TimeScale = (MaxIn - MinIn) / static_cast<float>(EntryCount - 1);

    // Get the operation to use, and calculate the number of values needed for that operation.
    const uint8 Op = Distribution->GetOperation();
    const int32 ValuesPerEntry = Distribution->GetValueCount();
    const uint32 EntryStride = ((Op == RDO_None) ? 1 : 2) * static_cast<uint32>(ValuesPerEntry);

    // Get the lock flag to use.
    const uint8 LockFlag = Distribution->GetLockFlag();

    // Allocate a lookup table of the appropriate size.
    OutTable->Op = Op;
    OutTable->EntryCount = EntryCount;
    OutTable->EntryStride = EntryStride;
    OutTable->SubEntryStride = (Op == RDO_None) ? 0 : ValuesPerEntry;
    OutTable->TimeScale = (TimeScale > 0.0f) ? (1.0f / TimeScale) : 0.0f;
    OutTable->TimeBias = MinIn;
    OutTable->Values.Empty(EntryCount * EntryStride);
    OutTable->Values.AddZeroed(EntryCount * EntryStride);
    OutTable->LockFlag = LockFlag;

    // Sample the distribution.
    for (uint32 SampleIndex = 0; SampleIndex < EntryCount; SampleIndex++)
    {
        const float Time = MinIn + SampleIndex * TimeScale;
        float Values[8];
        Distribution->InitializeRawEntry(Time, Values);
        for (uint32 ValueIndex = 0; ValueIndex < EntryStride; ValueIndex++)
        {
            OutTable->Values[SampleIndex * EntryStride + ValueIndex] = Values[ValueIndex];
        }
    }
}

/**
 * Appends one lookup table to another.
 * @param Table - Table which contains the first set of components [1-3].
 * @param OtherTable - Table to append which contains a single component.
 */
void AppendLookupTable(FDistributionLookupTable* Table, const FDistributionLookupTable& OtherTable)
{
    assert(Table != nullptr);
    assert(Table->GetValuesPerEntry() >= 1 && Table->GetValuesPerEntry() <= 3);
    assert(OtherTable.GetValuesPerEntry() == 1);

    // Copy the input table.
    FDistributionLookupTable TableCopy = *Table;

    // Compute the domain of the composed distribution.
    const float OneOverTimeScale = (TableCopy.TimeScale == 0.0f) ? 0.0f : 1.0f / TableCopy.TimeScale;
    const float OneOverOtherTimeScale = (OtherTable.TimeScale == 0.0f) ? 0.0f : 1.0f / OtherTable.TimeScale;
    const float MinIn = FMath::Min(TableCopy.TimeBias, OtherTable.TimeBias);
    const float MaxIn = FMath::Max(
        TableCopy.TimeBias + (TableCopy.EntryCount - 1) * OneOverTimeScale, OtherTable.TimeBias + (OtherTable.EntryCount - 1) * OneOverOtherTimeScale
    );

    const int32 InValuesPerEntry = TableCopy.GetValuesPerEntry();
    constexpr int32 OtherValuesPerEntry = 1;
    const int32 NewValuesPerEntry = InValuesPerEntry + OtherValuesPerEntry;
    const uint8 NewOp = (TableCopy.Op == RDO_None) ? OtherTable.Op : TableCopy.Op;
    constexpr int32 NewEntryCount = LOOKUP_TABLE_MAX_SAMPLES;
    const int32 NewStride = (NewOp == RDO_None) ? NewValuesPerEntry : NewValuesPerEntry * 2;
    const float NewTimeScale = (MaxIn - MinIn) / static_cast<float>(NewEntryCount - 1);

    // Now build the new lookup table.
    Table->Op = NewOp;
    Table->EntryCount = NewEntryCount;
    Table->EntryStride = NewStride;
    Table->SubEntryStride = (NewOp == RDO_None) ? 0 : NewValuesPerEntry;
    Table->TimeScale = (NewTimeScale > 0.0f) ? 1.0f / NewTimeScale : 0.0f;
    Table->TimeBias = MinIn;
    Table->Values.Empty(NewEntryCount * NewStride);
    Table->Values.AddZeroed(NewEntryCount * NewStride);
    for (int32 SampleIndex = 0; SampleIndex < NewEntryCount; ++SampleIndex)
    {
        const float* InEntry1;
        const float* InEntry2;
        const float* OtherEntry1;
        const float* OtherEntry2;
        float InLerpAlpha;
        float OtherLerpAlpha;

        const float Time = MinIn + SampleIndex * NewTimeScale;
        TableCopy.GetEntry(Time, InEntry1, InEntry2, InLerpAlpha);
        OtherTable.GetEntry(Time, OtherEntry1, OtherEntry2, OtherLerpAlpha);

        // Store sub-entry 1.
        for (int32 ValueIndex = 0; ValueIndex < InValuesPerEntry; ++ValueIndex)
        {
            Table->Values[SampleIndex * NewStride + ValueIndex] =
                FMath::Lerp(InEntry1[ValueIndex], InEntry2[ValueIndex], InLerpAlpha);
        }
        Table->Values[SampleIndex * NewStride + InValuesPerEntry] =
            FMath::Lerp(OtherEntry1[0], OtherEntry2[0], OtherLerpAlpha);

        // Store sub-entry 2 if needed. 
        if (NewOp != RDO_None)
        {
            InEntry1 += TableCopy.SubEntryStride;
            InEntry2 += TableCopy.SubEntryStride;
            OtherEntry1 += OtherTable.SubEntryStride;
            OtherEntry2 += OtherTable.SubEntryStride;

            for (int32 ValueIndex = 0; ValueIndex < InValuesPerEntry; ++ValueIndex)
            {
                Table->Values[SampleIndex * NewStride + NewValuesPerEntry + ValueIndex] =
                    FMath::Lerp(InEntry1[ValueIndex], InEntry2[ValueIndex], InLerpAlpha);
            }
            Table->Values[SampleIndex * NewStride + NewValuesPerEntry + InValuesPerEntry] =
                FMath::Lerp(OtherEntry1[0], OtherEntry2[0], OtherLerpAlpha);
        }
    }
}

/**
 * Keeps only the first components of each entry in the table.
 * @param Table - Table to slice.
 * @param ChannelsToKeep - The number of channels to keep.
 */
void SliceLookupTable(FDistributionLookupTable* Table, int32 ChannelsToKeep)
{
    assert(Table != nullptr);
    assert(Table->GetValuesPerEntry() >= ChannelsToKeep);

    // If the table only has the requested number of channels there is nothing to do.
    if (Table->GetValuesPerEntry() == ChannelsToKeep)
    {
        return;
    }

    // Copy the table.
    FDistributionLookupTable OldTable = *Table;

    // The new table will have the same number of entries as the input table.
    const int32 NewEntryCount = OldTable.EntryCount;
    const int32 NewStride = (OldTable.Op == RDO_None) ? (ChannelsToKeep) : (2 * ChannelsToKeep);
    Table->Op = OldTable.Op;
    Table->EntryCount = NewEntryCount;
    Table->EntryStride = NewStride;
    Table->SubEntryStride = (OldTable.Op == RDO_None) ? (0) : (ChannelsToKeep);
    Table->TimeBias = OldTable.TimeBias;
    Table->TimeScale = OldTable.TimeScale;
    Table->Values.Empty(NewEntryCount * NewStride);
    Table->Values.AddZeroed(NewEntryCount * NewStride);

    // Copy values over.
    for (int32 EntryIndex = 0; EntryIndex < NewEntryCount; ++EntryIndex)
    {
        const float* RESTRICT SrcValues = &OldTable.Values[EntryIndex * OldTable.EntryStride];
        float* RESTRICT DestValues = &Table->Values[EntryIndex * Table->EntryStride];
        for (int32 ValueIndex = 0; ValueIndex < ChannelsToKeep; ++ValueIndex)
        {
            DestValues[ValueIndex] = SrcValues[ValueIndex];
        }
        if (OldTable.SubEntryStride > 0)
        {
            SrcValues += OldTable.SubEntryStride;
            DestValues += Table->SubEntryStride;
            for (int32 ValueIndex = 0; ValueIndex < ChannelsToKeep; ++ValueIndex)
            {
                DestValues[ValueIndex] = SrcValues[ValueIndex];
            }
        }
    }
}

/**
 * Scales each value in the lookup table by a constant.
 * @param InTable - Table to be scaled.
 * @param Scale - The amount to scale by.
 */
void ScaleLookupTableByConstant(FDistributionLookupTable* Table, float Scale)
{
    assert(Table != nullptr);

    for (int32 ValueIndex = 0; ValueIndex < Table->Values.Num(); ++ValueIndex)
    {
        Table->Values[ValueIndex] *= Scale;
    }
}

/**
 * Scales each value in the lookup table by a constant.
 * @param InTable - Table to be scaled.
 * @param Scale - The amount to scale by.
 * @param ValueCount - The number of scale values.
 */
void ScaleLookupTableByConstants(FDistributionLookupTable* InTable, const float* Scale, int32 ValueCount)
{
    assert(InTable != nullptr);
    assert(ValueCount == InTable->GetValuesPerEntry());

    const int32 EntryCount = InTable->EntryCount;
    const int32 SubEntryCount = (InTable->SubEntryStride > 0) ? 2 : 1;
    const int32 Stride = InTable->EntryStride;
    const int32 SubEntryStride = InTable->SubEntryStride;
    float* RESTRICT Values = InTable->Values.GetData();

    for (int32 Index = 0; Index < EntryCount; ++Index)
    {
        float* RESTRICT EntryValues = Values;
        for (int32 SubEntryIndex = 0; SubEntryIndex < SubEntryCount; ++SubEntryIndex)
        {
            for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
            {
                EntryValues[ValueIndex] *= Scale[ValueIndex];
            }
            EntryValues += SubEntryStride;
        }
        Values += Stride;
    }
}

/**
 * Adds a constant to each value in the lookup table.
 * @param InTable - Table to be scaled.
 * @param Addend - The amount to add by.
 * @param ValueCount - The number of values per entry.
 */
void AddConstantToLookupTable(FDistributionLookupTable* InTable, const float* Addend, int32 ValueCount)
{
    assert(InTable != nullptr);
    assert(ValueCount == InTable->GetValuesPerEntry());

    const int32 EntryCount = InTable->EntryCount;
    const int32 SubEntryCount = (InTable->SubEntryStride > 0) ? 2 : 1;
    const int32 Stride = InTable->EntryStride;
    const int32 SubEntryStride = InTable->SubEntryStride;
    float* RESTRICT Values = InTable->Values.GetData();

    for (int32 Index = 0; Index < EntryCount; ++Index)
    {
        float* RESTRICT EntryValues = Values;
        for (int32 SubEntryIndex = 0; SubEntryIndex < SubEntryCount; ++SubEntryIndex)
        {
            for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
            {
                EntryValues[ValueIndex] += Addend[ValueIndex];
            }
            EntryValues += SubEntryStride;
        }
        Values += Stride;
    }
}

/**
 * Scales one lookup table by another.
 * @param Table - The table to scale.
 * @param OtherTable - The table to scale by. Must have one value per entry OR the same values per entry as Table.
 */
void ScaleLookupTableByLookupTable(FDistributionLookupTable* Table, const FDistributionLookupTable& OtherTable)
{
    assert(Table != nullptr);
    assert(OtherTable.GetValuesPerEntry() == 1 || OtherTable.GetValuesPerEntry() == Table->GetValuesPerEntry());

    // Copy the original table.
    FDistributionLookupTable InTable = *Table;

    // Compute the domain of the composed distribution.
    const float OneOverTimeScale = (InTable.TimeScale == 0.0f) ? 0.0f : 1.0f / InTable.TimeScale;
    const float OneOverOtherTimeScale = (OtherTable.TimeScale == 0.0f) ? 0.0f : 1.0f / OtherTable.TimeScale;
    const float MinIn = FMath::Min(InTable.TimeBias, OtherTable.TimeBias);
    const float MaxIn = FMath::Max(
        InTable.TimeBias + (InTable.EntryCount - 1) * OneOverTimeScale, OtherTable.TimeBias + (OtherTable.EntryCount - 1) * OneOverOtherTimeScale
    );

    const int32 ValuesPerEntry = InTable.GetValuesPerEntry();
    const int32 OtherValuesPerEntry = OtherTable.GetValuesPerEntry();
    const uint8 NewOp = (InTable.Op == RDO_None) ? OtherTable.Op : InTable.Op;
    constexpr int32 NewEntryCount = LOOKUP_TABLE_MAX_SAMPLES;
    const int32 NewStride = (NewOp == RDO_None) ? ValuesPerEntry : ValuesPerEntry * 2;
    const float NewTimeScale = (MaxIn - MinIn) / static_cast<float>(NewEntryCount - 1);

    // Now build the new lookup table.
    Table->Op = NewOp;
    Table->EntryCount = NewEntryCount;
    Table->EntryStride = NewStride;
    Table->SubEntryStride = (NewOp == RDO_None) ? 0 : ValuesPerEntry;
    Table->TimeScale = (NewTimeScale > 0.0f) ? 1.0f / NewTimeScale : 0.0f;
    Table->TimeBias = MinIn;
    Table->Values.Empty(NewEntryCount * NewStride);
    Table->Values.AddZeroed(NewEntryCount * NewStride);
    for (int32 SampleIndex = 0; SampleIndex < NewEntryCount; ++SampleIndex)
    {
        const float* InEntry1;
        const float* InEntry2;
        const float* OtherEntry1;
        const float* OtherEntry2;
        float InLerpAlpha;
        float OtherLerpAlpha;

        const float Time = MinIn + SampleIndex * NewTimeScale;
        InTable.GetEntry(Time, InEntry1, InEntry2, InLerpAlpha);
        OtherTable.GetEntry(Time, OtherEntry1, OtherEntry2, OtherLerpAlpha);

        // Scale sub-entry 1.
        for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
        {
            Table->Values[SampleIndex * NewStride + ValueIndex] =
                FMath::Lerp(InEntry1[ValueIndex], InEntry2[ValueIndex], InLerpAlpha) *
                FMath::Lerp(OtherEntry1[ValueIndex % OtherValuesPerEntry], OtherEntry2[ValueIndex % OtherValuesPerEntry], OtherLerpAlpha);
        }

        // Scale sub-entry 2 if needed. 
        if (NewOp != RDO_None)
        {
            InEntry1 += InTable.SubEntryStride;
            InEntry2 += InTable.SubEntryStride;
            OtherEntry1 += OtherTable.SubEntryStride;
            OtherEntry2 += OtherTable.SubEntryStride;

            for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
            {
                Table->Values[SampleIndex * NewStride + ValuesPerEntry + ValueIndex] =
                    FMath::Lerp(InEntry1[ValueIndex], InEntry2[ValueIndex], InLerpAlpha) *
                    FMath::Lerp(OtherEntry1[ValueIndex % OtherValuesPerEntry], OtherEntry2[ValueIndex % OtherValuesPerEntry], OtherLerpAlpha);
            }
        }
    }
}

/**
 * Adds the values in one lookup table by another.
 * @param Table - The table to which to add values.
 * @param OtherTable - The table from which to add values. Must have one value per entry OR the same values per entry as Table.
 */
void AddLookupTableToLookupTable(FDistributionLookupTable* Table, const FDistributionLookupTable& OtherTable)
{
    assert(Table != nullptr);
    assert(OtherTable.GetValuesPerEntry() == 1 || OtherTable.GetValuesPerEntry() == Table->GetValuesPerEntry());

    // Copy the original table.
    FDistributionLookupTable InTable = *Table;

    // Compute the domain of the composed distribution.
    const float OneOverTimeScale = (InTable.TimeScale == 0.0f) ? 0.0f : 1.0f / InTable.TimeScale;
    const float OneOverOtherTimeScale = (OtherTable.TimeScale == 0.0f) ? 0.0f : 1.0f / OtherTable.TimeScale;
    const float MinIn = FMath::Min(InTable.TimeBias, OtherTable.TimeBias);
    const float MaxIn = FMath::Max(
        InTable.TimeBias + (InTable.EntryCount - 1) * OneOverTimeScale, OtherTable.TimeBias + (OtherTable.EntryCount - 1) * OneOverOtherTimeScale
    );

    const int32 ValuesPerEntry = InTable.GetValuesPerEntry();
    const int32 OtherValuesPerEntry = OtherTable.GetValuesPerEntry();
    const uint8 NewOp = (InTable.Op == RDO_None) ? OtherTable.Op : InTable.Op;
    constexpr int32 NewEntryCount = LOOKUP_TABLE_MAX_SAMPLES;
    const int32 NewStride = (NewOp == RDO_None) ? ValuesPerEntry : ValuesPerEntry * 2;
    const float NewTimeScale = (MaxIn - MinIn) / static_cast<float>(NewEntryCount - 1);

    // Now build the new lookup table.
    Table->Op = NewOp;
    Table->EntryCount = NewEntryCount;
    Table->EntryStride = NewStride;
    Table->SubEntryStride = (NewOp == RDO_None) ? 0 : ValuesPerEntry;
    Table->TimeScale = (NewTimeScale > 0.0f) ? 1.0f / NewTimeScale : 0.0f;
    Table->TimeBias = MinIn;
    Table->Values.Empty(NewEntryCount * NewStride);
    Table->Values.AddZeroed(NewEntryCount * NewStride);
    for (int32 SampleIndex = 0; SampleIndex < NewEntryCount; ++SampleIndex)
    {
        const float* InEntry1;
        const float* InEntry2;
        const float* OtherEntry1;
        const float* OtherEntry2;
        float InLerpAlpha;
        float OtherLerpAlpha;

        const float Time = MinIn + SampleIndex * NewTimeScale;
        InTable.GetEntry(Time, InEntry1, InEntry2, InLerpAlpha);
        OtherTable.GetEntry(Time, OtherEntry1, OtherEntry2, OtherLerpAlpha);

        // Scale sub-entry 1.
        for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
        {
            Table->Values[SampleIndex * NewStride + ValueIndex] =
                FMath::Lerp(InEntry1[ValueIndex], InEntry2[ValueIndex], InLerpAlpha) +
                FMath::Lerp(OtherEntry1[ValueIndex % OtherValuesPerEntry], OtherEntry2[ValueIndex % OtherValuesPerEntry], OtherLerpAlpha);
        }

        // Scale sub-entry 2 if needed. 
        if (NewOp != RDO_None)
        {
            InEntry1 += InTable.SubEntryStride;
            InEntry2 += InTable.SubEntryStride;
            OtherEntry1 += OtherTable.SubEntryStride;
            OtherEntry2 += OtherTable.SubEntryStride;

            for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
            {
                Table->Values[SampleIndex * NewStride + ValuesPerEntry + ValueIndex] =
                    FMath::Lerp(InEntry1[ValueIndex], InEntry2[ValueIndex], InLerpAlpha) +
                    FMath::Lerp(OtherEntry1[ValueIndex % OtherValuesPerEntry], OtherEntry2[ValueIndex % OtherValuesPerEntry], OtherLerpAlpha);
            }
        }
    }
}

/**
 * Computes the L2 norm between the samples in ValueCount dimensional space.
 * @param Values1 - Array of ValueCount values.
 * @param Values2 - Array of ValueCount values.
 * @param ValueCount - The number of values in the two arrays.
 * @returns the L2 norm of the difference of the vectors represented by the two float arrays.
 */
float ComputeSampleDistance(const float* Values1, const float* Values2, int32 ValueCount)
{
    float Dist = 0.0f;
    for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
    {
        const float Diff = Values1[ValueIndex] - Values2[ValueIndex];
        Dist += (Diff * Diff);
    }
    return FMath::Sqrt(Dist);
}

/**
 * Computes the chordal distance between the curves represented by the two tables.
 * @param InTable1 - The first table to compare.
 * @param InTable2 - The second table to compare.
 * @param MinIn - The time at which to begin comparing.
 * @param MaxIn - The time at which to stop comparing.
 * @param SampleCount - The number of samples to use.
 * @returns the chordal distance representing the error introduced by substituting one table for the other.
 */
float ComputeLookupTableError(
    const FDistributionLookupTable& InTable1, const FDistributionLookupTable& InTable2, float MinIn, float MaxIn, int32 SampleCount
)
{
    assert(InTable1.EntryStride == InTable2.EntryStride);
    assert(InTable1.SubEntryStride == InTable2.SubEntryStride);
    assert(SampleCount > 0);

    const FDistributionLookupTable* Table1 = (InTable2.EntryCount > InTable1.EntryCount) ? &InTable2 : &InTable1;
    const FDistributionLookupTable* Table2 = (Table1 == &InTable1) ? &InTable2 : &InTable1;
    const int32 ValuesPerEntry = Table1->GetValuesPerEntry();
    const float TimeStep = (MaxIn - MinIn) / (SampleCount - 1);

    float Values1[4] = {0};
    float Values2[4] = {0};
    float Error = 0.0f;
    float Time = MinIn;
    for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex, Time += TimeStep)
    {
        const float* Table1Entry1 = nullptr;
        const float* Table1Entry2 = nullptr;
        float Table1LerpAlpha = 0.0f;
        const float* Table2Entry1 = nullptr;
        const float* Table2Entry2 = nullptr;
        float Table2LerpAlpha = 0.0f;

        Table1->GetEntry(Time, Table1Entry1, Table1Entry2, Table1LerpAlpha);
        Table2->GetEntry(Time, Table2Entry1, Table2Entry2, Table2LerpAlpha);
        for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
        {
            Values1[ValueIndex] = FMath::Lerp(Table1Entry1[ValueIndex], Table1Entry2[ValueIndex], Table1LerpAlpha);
            Values2[ValueIndex] = FMath::Lerp(Table2Entry1[ValueIndex], Table2Entry2[ValueIndex], Table2LerpAlpha);
        }
        Error = FMath::Max<float>(Error, ComputeSampleDistance(Values1, Values2, ValuesPerEntry));

        if (Table1->SubEntryStride > 0)
        {
            Table1Entry1 += Table1->SubEntryStride;
            Table1Entry2 += Table1->SubEntryStride;
            Table2Entry1 += Table2->SubEntryStride;
            Table2Entry2 += Table2->SubEntryStride;
            for (int32 ValueIndex = 0; ValueIndex < ValuesPerEntry; ++ValueIndex)
            {
                Values1[ValueIndex] = FMath::Lerp(Table1Entry1[ValueIndex], Table1Entry2[ValueIndex], Table1LerpAlpha);
                Values2[ValueIndex] = FMath::Lerp(Table2Entry1[ValueIndex], Table2Entry2[ValueIndex], Table2LerpAlpha);
            }
            Error = FMath::Max<float>(Error, ComputeSampleDistance(Values1, Values2, ValuesPerEntry));
        }
    }
    return Error;
}

/**
 * Resamples a lookup table.
 * @param OutTable - The resampled table.
 * @param InTable - The table to be resampled.
 * @param MinIn - The time at which to begin resampling.
 * @param MaxIn - The time at which to stop resampling.
 * @param SampleCount - The number of samples to use.
 */
void ResampleLookupTable(
    FDistributionLookupTable* OutTable, const FDistributionLookupTable& InTable, float MinIn, float MaxIn, int32 SampleCount
)
{
    const int32 Stride = InTable.EntryStride;
    const float OneOverTimeScale = (InTable.TimeScale == 0.0f) ? 0.0f : 1.0f / InTable.TimeScale;
    const float TimeScale = (SampleCount > 1) ? ((MaxIn - MinIn) / static_cast<float>(SampleCount - 1)) : 0.0f;

    // Build the resampled table.
    OutTable->Op = InTable.Op;
    OutTable->EntryCount = SampleCount;
    OutTable->EntryStride = InTable.EntryStride;
    OutTable->SubEntryStride = InTable.SubEntryStride;
    OutTable->TimeBias = MinIn;
    OutTable->TimeScale = (TimeScale > 0.0f) ? (1.0f / TimeScale) : 0.0f;
    OutTable->Values.Empty(SampleCount * InTable.EntryStride);
    OutTable->Values.AddZeroed(SampleCount * InTable.EntryStride);

    // Resample entries in the table.
    for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
    {
        const float* Entry1 = nullptr;
        const float* Entry2 = nullptr;
        float LerpAlpha = 0.0f;
        const float Time = MinIn + TimeScale * SampleIndex;
        InTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
        for (int32 ValueIndex = 0; ValueIndex < Stride; ++ValueIndex)
        {
            OutTable->Values[SampleIndex * Stride + ValueIndex] =
                FMath::Lerp(Entry1[ValueIndex], Entry2[ValueIndex], LerpAlpha);
        }
    }
}

/**
 * Optimizes a lookup table using the minimum number of samples required to represent the distribution.
 * @param Table - The lookup table to optimize.
 * @param ErrorThreshold - Threshold at which the lookup table is considered good enough.
 */
void OptimizeLookupTable(FDistributionLookupTable* Table, float ErrorThreshold)
{
    assert(Table != nullptr);
    assert((Table->EntryCount & (Table->EntryCount-1)) == 0);

    // Domain for the table.
    const float OneOverTimeScale = (Table->TimeScale == 0.0f) ? 0.0f : 1.0f / Table->TimeScale;
    const float MinIn = Table->TimeBias;
    const float MaxIn = Table->TimeBias + (Table->EntryCount - 1) * OneOverTimeScale;

    // Duplicate the table.
    FDistributionLookupTable OriginalTable = *Table;

    // Resample the lookup table until error is reduced to an acceptable level.
    constexpr int32 MinSampleCount = 1;
    constexpr int32 MaxSampleCount = LOOKUP_TABLE_MAX_SAMPLES;
    for (int32 SampleCount = MinSampleCount; SampleCount < MaxSampleCount; SampleCount <<= 1)
    {
        ResampleLookupTable(Table, OriginalTable, MinIn, MaxIn, SampleCount);
        if (ComputeLookupTableError(*Table, OriginalTable, MinIn, MaxIn, LOOKUP_TABLE_MAX_SAMPLES) < ErrorThreshold)
        {
            return;
        }
    }

    // The original table is optimal.
    *Table = OriginalTable;
}
}


void FRawDistribution::GetValue(float Time, float* Value, int32 NumCoords, int32 Extreme, FRandomStream* InRandomStream) const
{
    assert(NumCoords == 3 || NumCoords == 1);

    switch (LookupTable.Op)
    {
    case RDO_None:
        if (NumCoords == 1)
        {
            GetValue1None(Time, Value);
        }
        else
        {
            GetValue3None(Time, Value);
        }
        break;
    case RDO_Extreme:
        if (NumCoords == 1)
        {
            GetValue1Extreme(Time, Value, Extreme, InRandomStream);
        }
        else
        {
            GetValue3Extreme(Time, Value, Extreme, InRandomStream);
        }
        break;
    case RDO_Random:
        if (NumCoords == 1)
        {
            GetValue1Random(Time, Value, InRandomStream);
        }
        else
        {
            GetValue3Random(Time, Value, InRandomStream);
        }
        break;
    }
}

void FRawDistribution::GetValue1(float Time, float* Value, int32 Extreme, FRandomStream* InRandomStream) const
{
    switch (LookupTable.Op)
    {
    case RDO_None:
        GetValue1None(Time, Value);
        break;
    case RDO_Extreme:
        GetValue1Extreme(Time, Value, Extreme, InRandomStream);
        break;
    case RDO_Random:
        GetValue1Random(Time, Value, InRandomStream);
        break;
    default: // compiler complains
        assert(0);
        *Value = 0.0f;
        break;
    }
}

void FRawDistribution::GetValue3(float Time, float* Value, int32 Extreme, FRandomStream* InRandomStream) const
{
    switch (LookupTable.Op)
    {
    case RDO_None:
        GetValue3None(Time, Value);
        break;
    case RDO_Extreme:
        GetValue3Extreme(Time, Value, Extreme, InRandomStream);
        break;
    case RDO_Random:
        GetValue3Random(Time, Value, InRandomStream);
        break;
    }
}

void FRawDistribution::GetValue1Extreme(float Time, float* InValue, int32 Extreme, FRandomStream* InRandomStream) const
{
    float* RESTRICT Value = InValue;
    const float* Entry1;
    const float* Entry2;
    float LerpAlpha = 0.0f;
    const float RandValue = DIST_GET_RANDOM_VALUE(InRandomStream);
    LookupTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
    const float* RESTRICT NewEntry1 = Entry1;
    const float* RESTRICT NewEntry2 = Entry2;
    const int32 InitialElement = ((Extreme > 0) || ((Extreme == 0) && (RandValue > 0.5f)));
    Value[0] = FMath::Lerp(NewEntry1[InitialElement + 0], NewEntry2[InitialElement + 0], LerpAlpha);
}

void FRawDistribution::GetValue3Extreme(float Time, float* InValue, int32 Extreme, FRandomStream* InRandomStream) const
{
    float* RESTRICT Value = InValue;
    const float* Entry1;
    const float* Entry2;
    float LerpAlpha = 0.0f;
    const float RandValue = DIST_GET_RANDOM_VALUE(InRandomStream);
    LookupTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
    const float* RESTRICT NewEntry1 = Entry1;
    const float* RESTRICT NewEntry2 = Entry2;
    int32 InitialElement = ((Extreme > 0) || ((Extreme == 0) && (RandValue > 0.5f)));
    InitialElement *= 3;
    const float T0 = FMath::Lerp(NewEntry1[InitialElement + 0], NewEntry2[InitialElement + 0], LerpAlpha);
    const float T1 = FMath::Lerp(NewEntry1[InitialElement + 1], NewEntry2[InitialElement + 1], LerpAlpha);
    const float T2 = FMath::Lerp(NewEntry1[InitialElement + 2], NewEntry2[InitialElement + 2], LerpAlpha);
    Value[0] = T0;
    Value[1] = T1;
    Value[2] = T2;
}

void FRawDistribution::GetValue1Random(float Time, float* InValue, FRandomStream* InRandomStream) const
{
    float* RESTRICT Value = InValue;
    const float* Entry1;
    const float* Entry2;
    float LerpAlpha = 0.0f;
    const float RandValue = DIST_GET_RANDOM_VALUE(InRandomStream);
    LookupTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
    const float* RESTRICT NewEntry1 = Entry1;
    const float* RESTRICT NewEntry2 = Entry2;
    const float Value1 = FMath::Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
    const float Value2 = FMath::Lerp(NewEntry1[1 + 0], NewEntry2[1 + 0], LerpAlpha);
    Value[0] = Value1 + (Value2 - Value1) * RandValue;
}

void FRawDistribution::GetValue3Random(float Time, float* InValue, FRandomStream* InRandomStream) const
{
    float* RESTRICT Value = InValue;
    const float* Entry1;
    const float* Entry2;
    float LerpAlpha = 0.0f;
    FVector RandValues;

    RandValues[0] = DIST_GET_RANDOM_VALUE(InRandomStream);
    RandValues[1] = DIST_GET_RANDOM_VALUE(InRandomStream);
    RandValues[2] = DIST_GET_RANDOM_VALUE(InRandomStream);
    switch (static_cast<EDistributionVectorLockFlags>(LookupTable.LockFlag))
    {
    case EDistributionVectorLockFlags::XY:
        RandValues.Y = RandValues.X;
        break;
    case EDistributionVectorLockFlags::XZ:
        RandValues.Z = RandValues.X;
        break;
    case EDistributionVectorLockFlags::YZ:
        RandValues.Z = RandValues.Y;
        break;
    case EDistributionVectorLockFlags::XYZ:
        RandValues.Y = RandValues.X;
        RandValues.Z = RandValues.X;
        break;
    default:
        break;
    }

    LookupTable.GetEntry(Time, Entry1, Entry2, LerpAlpha);
    const float* RESTRICT NewEntry1 = Entry1;
    const float* RESTRICT NewEntry2 = Entry2;
    const float X0 = FMath::Lerp(NewEntry1[0], NewEntry2[0], LerpAlpha);
    const float Y0 = FMath::Lerp(NewEntry1[1], NewEntry2[1], LerpAlpha);
    const float Z0 = FMath::Lerp(NewEntry1[2], NewEntry2[2], LerpAlpha);
    const float X1 = FMath::Lerp(NewEntry1[3 + 0], NewEntry2[3 + 0], LerpAlpha);
    const float Y1 = FMath::Lerp(NewEntry1[3 + 1], NewEntry2[3 + 1], LerpAlpha);
    const float Z1 = FMath::Lerp(NewEntry1[3 + 2], NewEntry2[3 + 2], LerpAlpha);
    Value[0] = X0 + (X1 - X0) * RandValues[0];
    Value[1] = Y0 + (Y1 - Y0) * RandValues[1];
    Value[2] = Z0 + (Z1 - Z0) * RandValues[2];
}

UObject* FRawDistribution::TryGetDistributionObjectFromRawDistributionProperty(FStructProperty* Property, uint8* Data)
{
    // // if the struct in this property is of type FRawDistributionFloat
    // if (Property->Struct->GetFName() == NAME_RawDistributionFloat)
    // {
    //     // then return the UDistribution pointed to by the FRawDistributionFloat
    //     return Property->ContainerPtrToValuePtr<FRawDistributionFloat>(Data)->Distribution;
    // }
    // // if the struct in this property is of type FRawDistributionVector
    // else if (Property->Struct->GetFName() == NAME_RawDistributionVector)
    // {
    //     // then return the UDistribution pointed to by the FRawDistributionVector
    //     return Property->ContainerPtrToValuePtr<FRawDistributionVector>(Data)->Distribution;
    // }

    // if this wasn't a FRawDistribution*, return nullptr
    return nullptr;
}

bool FRawDistributionVector::IsCreated()
{
    return HasLookupTable(/*bInitializeIfNeeded=*/ false) || (Distribution != nullptr);
}

#if WITH_EDITOR
FRawDistributionVector::~FRawDistributionVector()
{
    if (IsValid(Distribution))
    {
        Distribution->MarkAsGarbage();
    }
}

void FRawDistributionVector::Initialize()
{
    // Nothing to do if we don't have a distribution.
    if (Distribution == nullptr)
    {
        return;
    }

    // fill out our min/max
    Distribution->GetOutRange(MinValue, MaxValue);
    Distribution->GetRange(MinValueVec, MaxValueVec);

    // does this FRawDist need updating? (if UDist is dirty or somehow the distribution wasn't dirty, but we have no data)
    bool bNeedsUpdating = false;
    if (Distribution->bIsDirty || (LookupTable.IsEmpty() && Distribution->CanBeBaked()))
    {
        if (!Distribution->bIsDirty)
        {
            UE_LOG(
                ELogLevel::Display, TEXT("Somehow Distribution %s wasn't dirty, but its FRawDistribution wasn't ever initialized!"),
                *Distribution->GetName()
            );
        }
        bNeedsUpdating = true;
    }

    // only initialize if we need to
    if (!bNeedsUpdating)
    {
        return;
    }
    // assert(IsInGameThread() || IsInAsyncLoadingThread());

    // always empty out the lookup table
    LookupTable.Empty();

    // distribution is no longer dirty (if it was)
    // template objects aren't marked as dirty, because any UDists that uses this as an archetype, 
    // aren't the default values, and has already been saved, needs to know to build the FDist
    // if (!Distribution->IsTemplate())
    // {
    //     Distribution->bIsDirty = false;
    // }

    // if the distribution can't be baked out, then we do nothing here
    if (!Distribution->CanBeBaked())
    {
        return;
    }

    // Build and optimize the lookup table.
    BuildLookupTable(&LookupTable, Distribution);
    const float MinIn = LookupTable.TimeBias;
    const float MaxIn = MinIn + (LookupTable.EntryCount - 1) * (LookupTable.TimeScale == 0.0f ? 0.0f : (1.0f / LookupTable.TimeScale));
    OptimizeLookupTable(&LookupTable, LOOKUP_TABLE_ERROR_THRESHOLD);
}
#endif

const FRawDistribution* FRawDistributionVector::GetFastRawDistribution()
{
    if (!IsSimple() || !HasLookupTable())
    {
        return nullptr;
    }

    // if we get here, we better have been initialized!
    assert(!LookupTable.IsEmpty());

    return this;
}

FVector FRawDistributionVector::GetValue(float F, UObject* Data, int32 Extreme, struct FRandomStream* InRandomStream)
{
    if (!HasLookupTable())
    {
        if (!Distribution)
        {
            return FVector::ZeroVector;
        }
        return Distribution->GetValue(F, Data, Extreme, InRandomStream);
    }

    // if we get here, we better have been initialized!
    assert(!LookupTable.IsEmpty());

    FVector Value;
    GetValue3(F, &Value.X, Extreme, InRandomStream);
    return Value;
}

void FRawDistributionVector::GetOutRange(float& MinOut, float& MaxOut)
{
    if (!HasLookupTable() && Distribution)
    {
        assert(Distribution);
        Distribution->GetOutRange(MinOut, MaxOut);
    }
    else
    {
        MinOut = MinValue;
        MaxOut = MaxValue;
    }
}

void FRawDistributionVector::GetRange(FVector& MinOut, FVector& MaxOut) const
{
    if (Distribution)
    {
        assert(Distribution);
        Distribution->GetRange(MinOut, MaxOut);
    }
    else
    {
        MinOut = MinValueVec;
        MaxOut = MaxValueVec;
    }
}

void FRawDistributionVector::InitLookupTable()
{
#if WITH_EDITOR
    // make sure it's up to date
    // if( GIsEditor || (Distribution && Distribution->bIsDirty) )
    if (Distribution && Distribution->bIsDirty)
    {
        Initialize();
    }
#endif
}


FVector UDistributionVector::GetVectorValue(float F)
{
    return GetValue(F);
}

uint32 UDistributionVector::InitializeRawEntry(float Time, float* Values) const
{
    FVector Value = GetValue(Time);
    Values[0] = Value.X;
    Values[1] = Value.Y;
    Values[2] = Value.Z;
    return 3;
}

FVector UDistributionVector::GetValue(float F, UObject* Data, int32 Extreme, struct FRandomStream* InRandomStream) const
{
    return FVector::ZeroVector;
}

void UDistributionVector::GetInRange(float& MinIn, float& MaxIn) const
{
    MinIn = 0.0f;
    MaxIn = 0.0f;
}

void UDistributionVector::GetOutRange(float& MinOut, float& MaxOut) const
{
    MinOut = 0.0f;
    MaxOut = 0.0f;
}

void UDistributionVector::GetRange(FVector& OutMin, FVector& OutMax) const
{
    OutMin = FVector::ZeroVector;
    OutMax = FVector::ZeroVector;
}

void UDistributionVector::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    bIsDirty = true;
}

void UDistributionVectorUniform::PostInitProperties()
{
    Super::PostInitProperties();
    if (GetOuter()->IsA<UParticleModule>())
    {
        // Set to a bogus value for distributions created before VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS
        // to be able to restore to the previous default value.
        Min = FVector(UDistribution::DefaultValue);
        Max = FVector(UDistribution::DefaultValue);
    }
}

FVector UDistributionVectorUniform::GetValue(float F, UObject* Data, int32 Extreme, struct FRandomStream* InRandomStream) const
{
    FVector LocalMax = Max;
    FVector LocalMin = Min;

    // LocalMin.X = (MirrorFlags[0] == EDVMF_Different) ? LocalMin.X : ((MirrorFlags[0] == EDVMF_Mirror) ? -LocalMax.X : LocalMax.X);
    // LocalMin.Y = (MirrorFlags[1] == EDVMF_Different) ? LocalMin.Y : ((MirrorFlags[1] == EDVMF_Mirror) ? -LocalMax.Y : LocalMax.Y);
    // LocalMin.Z = (MirrorFlags[2] == EDVMF_Different) ? LocalMin.Z : ((MirrorFlags[2] == EDVMF_Mirror) ? -LocalMax.Z : LocalMax.Z);

    float fX;
    float fY;
    float fZ;

    bool bMin = true;
    if (bUseExtremes)
    {
        if (Extreme == 0)
        {
            if (DIST_GET_RANDOM_VALUE(InRandomStream) > 0.5f)
            {
                bMin = false;
            }
        }
        else if (Extreme > 0)
        {
            bMin = false;
        }
    }

    switch (LockedAxes)
    {
    case EDistributionVectorLockFlags::XY:
        if (bUseExtremes)
        {
            if (bMin)
            {
                fX = LocalMin.X;
                fZ = LocalMin.Z;
            }
            else
            {
                fX = LocalMax.X;
                fZ = LocalMax.Z;
            }
        }
        else
        {
            fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
            fZ = LocalMax.Z + (LocalMin.Z - LocalMax.Z) * DIST_GET_RANDOM_VALUE(InRandomStream);
        }
        fY = fX;
        break;
    case EDistributionVectorLockFlags::XZ:
        if (bUseExtremes)
        {
            if (bMin)
            {
                fX = LocalMin.X;
                fY = LocalMin.Y;
            }
            else
            {
                fX = LocalMax.X;
                fY = LocalMax.Y;
            }
        }
        else
        {
            fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
            fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
        }
        fZ = fX;
        break;
    case EDistributionVectorLockFlags::YZ:
        if (bUseExtremes)
        {
            if (bMin)
            {
                fX = LocalMin.X;
                fY = LocalMin.Y;
            }
            else
            {
                fX = LocalMax.X;
                fY = LocalMax.Y;
            }
        }
        else
        {
            fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
            fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
        }
        fZ = fY;
        break;
    case EDistributionVectorLockFlags::XYZ:
        if (bUseExtremes)
        {
            if (bMin)
            {
                fX = LocalMin.X;
            }
            else
            {
                fX = LocalMax.X;
            }
        }
        else
        {
            fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
        }
        fY = fX;
        fZ = fX;
        break;
    case EDistributionVectorLockFlags::None:
    default:
        if (bUseExtremes)
        {
            if (bMin)
            {
                fX = LocalMin.X;
                fY = LocalMin.Y;
                fZ = LocalMin.Z;
            }
            else
            {
                fX = LocalMax.X;
                fY = LocalMax.Y;
                fZ = LocalMax.Z;
            }
        }
        else
        {
            fX = LocalMax.X + (LocalMin.X - LocalMax.X) * DIST_GET_RANDOM_VALUE(InRandomStream);
            fY = LocalMax.Y + (LocalMin.Y - LocalMax.Y) * DIST_GET_RANDOM_VALUE(InRandomStream);
            fZ = LocalMax.Z + (LocalMin.Z - LocalMax.Z) * DIST_GET_RANDOM_VALUE(InRandomStream);
        }
        break;
    }

    return FVector{fX, fY, fZ};
}

ERawDistributionOperation UDistributionVectorUniform::GetOperation() const
{
    if (Min == Max)
    {
        // This may as well be a constant - don't bother doing the FMath::SRand scaling on it.
        return RDO_None;
    }
    // override the operation to use
    return bUseExtremes ? RDO_Extreme : RDO_Random;
}

uint8 UDistributionVectorUniform::GetLockFlag() const
{
    return static_cast<uint8>(LockedAxes);
}


uint32 UDistributionVectorUniform::InitializeRawEntry(float Time, float* Values) const
{
    // get the locked/mirrored min and max
    FVector ValueMin = GetMinValue();
    FVector ValueMax = GetMaxValue();
    Values[0] = ValueMin.X;
    Values[1] = ValueMin.Y;
    Values[2] = ValueMin.Z;
    Values[3] = ValueMax.X;
    Values[4] = ValueMax.Y;
    Values[5] = ValueMax.Z;

    // six elements per value
    return 6;
}

void UDistributionVectorUniform::GetRange(FVector& OutMin, FVector& OutMax) const
{
    OutMin = Min;
    OutMax = Max;
}


FVector UDistributionVectorUniform::GetMinValue() const
{
    FVector LocalMax = Max;
    FVector LocalMin = Min;

    // for (int32 i = 0; i < 3; i++)
    // {
    //     switch (MirrorFlags[i])
    //     {
    //     case EDVMF_Same:	LocalMin[i] =  LocalMax[i];		break;
    //     case EDVMF_Mirror:	LocalMin[i] = -LocalMax[i];		break;
    //     }
    // }

    float fX;
    float fY;
    float fZ;

    switch (LockedAxes)
    {
    case EDistributionVectorLockFlags::XY:
        fX = LocalMin.X;
        fY = LocalMin.X;
        fZ = LocalMin.Z;
        break;
    case EDistributionVectorLockFlags::XZ:
        fX = LocalMin.X;
        fY = LocalMin.Y;
        fZ = fX;
        break;
    case EDistributionVectorLockFlags::YZ:
        fX = LocalMin.X;
        fY = LocalMin.Y;
        fZ = fY;
        break;
    case EDistributionVectorLockFlags::XYZ:
        fX = LocalMin.X;
        fY = fX;
        fZ = fX;
        break;
    case EDistributionVectorLockFlags::None:
    default:
        fX = LocalMin.X;
        fY = LocalMin.Y;
        fZ = LocalMin.Z;
        break;
    }

    return FVector{fX, fY, fZ};
}

FVector UDistributionVectorUniform::GetMaxValue() const
{
    FVector LocalMax = Max;

    float fX;
    float fY;
    float fZ;

    switch (LockedAxes)
    {
    case EDistributionVectorLockFlags::XY:
        fX = LocalMax.X;
        fY = LocalMax.X;
        fZ = LocalMax.Z;
        break;
    case EDistributionVectorLockFlags::XZ:
        fX = LocalMax.X;
        fY = LocalMax.Y;
        fZ = fX;
        break;
    case EDistributionVectorLockFlags::YZ:
        fX = LocalMax.X;
        fY = LocalMax.Y;
        fZ = fY;
        break;
    case EDistributionVectorLockFlags::XYZ:
        fX = LocalMax.X;
        fY = fX;
        fZ = fX;
        break;
    case EDistributionVectorLockFlags::None:
    default:
        fX = LocalMax.X;
        fY = LocalMax.Y;
        fZ = LocalMax.Z;
        break;
    }

    return FVector{fX, fY, fZ};
}

int32 UDistributionVectorUniform::GetNumKeys() const
{
    return 1;
}

int32 UDistributionVectorUniform::GetNumSubCurves() const
{
    switch (LockedAxes)
    {
    case EDistributionVectorLockFlags::XY:
    case EDistributionVectorLockFlags::XZ:
    case EDistributionVectorLockFlags::YZ:
        return 4;
    case EDistributionVectorLockFlags::XYZ:
        return 2;
    }
    return 6;
}


FColor UDistributionVectorUniform::GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const
{
    const int32 SubCurves = GetNumSubCurves();

    // Check for array out of bounds because it will crash the program
    assert(SubCurveIndex >= 0);
    assert(SubCurveIndex < SubCurves);

    const bool bShouldGroupMinAndMax = ((SubCurves == 4) || (SubCurves == 6));
    FColor ButtonColor;

    switch (SubCurveIndex)
    {
    case 0:
        // Red
        ButtonColor = bIsSubCurveHidden ? FColor(32, 0, 0) : FColor::Red;
        break;
    case 1:
        if (bShouldGroupMinAndMax)
        {
            // Dark red
            ButtonColor = bIsSubCurveHidden ? FColor(28, 0, 0) : FColor(196, 0, 0);
        }
        else
        {
            // Green
            ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor::Green;
        }
        break;
    case 2:
        if (bShouldGroupMinAndMax)
        {
            // Green
            ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor::Green;
        }
        else
        {
            // Blue
            ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor::Blue;
        }
        break;
    case 3:
        // Dark green
        ButtonColor = bIsSubCurveHidden ? FColor(0, 28, 0) : FColor(0, 196, 0);
        break;
    case 4:
        // Blue
        ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 32) : FColor::Blue;
        break;
    case 5:
        // Dark blue
        ButtonColor = bIsSubCurveHidden ? FColor(0, 0, 28) : FColor(0, 0, 196);
        break;
    default:
        // A bad sub-curve index was given. 
        assert(false);
        break;
    }

    return ButtonColor;
}

float UDistributionVectorUniform::GetKeyIn(int32 KeyIndex)
{
    assert(KeyIndex == 0);
    return 0.f;
}

float UDistributionVectorUniform::GetKeyOut(int32 SubIndex, int32 KeyIndex)
{
    assert(SubIndex >= 0 && SubIndex < 6);
    assert(KeyIndex == 0);

    FVector LocalMax = Max;
    FVector LocalMin = Min;

    // for (int32 i = 0; i < 3; i++)
    // {
    //     switch (MirrorFlags[i])
    //     {
    //     case EDVMF_Same: LocalMin[i] = LocalMax[i];
    //         break;
    //     case EDVMF_Mirror: LocalMin[i] = -LocalMax[i];
    //         break;
    //     }
    // }

    switch (LockedAxes)
    {
    case EDistributionVectorLockFlags::XY:
        LocalMin.Y = LocalMin.X;
        break;
    case EDistributionVectorLockFlags::XZ:
        LocalMin.Z = LocalMin.X;
        break;
    case EDistributionVectorLockFlags::YZ:
        LocalMin.Z = LocalMin.Y;
        break;
    case EDistributionVectorLockFlags::XYZ:
        LocalMin.Y = LocalMin.X;
        LocalMin.Z = LocalMin.X;
        break;
    case EDistributionVectorLockFlags::None:
    default:
        break;
    }

    switch (SubIndex)
    {
    case 0: return LocalMin.X;
    case 1: return LocalMax.X;
    case 2: return LocalMin.Y;
    case 3: return LocalMax.Y;
    case 4: return LocalMin.Z;
    }
    return LocalMax.Z;
}

FColor UDistributionVectorUniform::GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor)
{
    assert(SubIndex >= 0 && SubIndex < 6);
    assert(KeyIndex == 0);

    if (SubIndex == 0)
    {
        return FColor{128, 0, 0};
    }
    else if (SubIndex == 1)
    {
        return FColor::Red;
    }
    else if (SubIndex == 2)
    {
        return FColor{0, 128, 0};
    }
    else if (SubIndex == 3)
    {
        return FColor::Green;
    }
    else if (SubIndex == 4)
    {
        return FColor{0, 0, 128};
    }
    else
    {
        return FColor::Blue;
    }
}

void UDistributionVectorUniform::GetInRange(float& MinIn, float& MaxIn) const
{
    MinIn = 0.f;
    MaxIn = 0.f;
}

void UDistributionVectorUniform::GetOutRange(float& MinOut, float& MaxOut) const
{
    FVector LocalMax = Max;
    FVector LocalMin = Min;

    // for (int32 i = 0; i < 3; i++)
    // {
    //     switch (MirrorFlags[i])
    //     {
    //     case EDVMF_Same: LocalMin[i] = LocalMax[i];
    //         break;
    //     case EDVMF_Mirror: LocalMin[i] = -LocalMax[i];
    //         break;
    //     }
    // }

    FVector LocalMin2;
    FVector LocalMax2;

    switch (LockedAxes)
    {
    case EDistributionVectorLockFlags::XY:
        LocalMin2 = FVector(LocalMin.X, LocalMin.X, LocalMin.Z);
        LocalMax2 = FVector(LocalMax.X, LocalMax.X, LocalMax.Z);
        break;
    case EDistributionVectorLockFlags::XZ:
        LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.X);
        LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.X);
        break;
    case EDistributionVectorLockFlags::YZ:
        LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.Y);
        LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.Y);
        break;
    case EDistributionVectorLockFlags::XYZ:
        LocalMin2 = FVector(LocalMin.X);
        LocalMax2 = FVector(LocalMax.X);
        break;
    case EDistributionVectorLockFlags::None:
        LocalMin2 = FVector(LocalMin.X, LocalMin.Y, LocalMin.Z);
        LocalMax2 = FVector(LocalMax.X, LocalMax.Y, LocalMax.Z);
        break;
    }

    MinOut = LocalMin2.GetMin();
    MaxOut = LocalMax2.GetMax();
}

// EInterpCurveMode UDistributionVectorUniform::GetKeyInterpMode(int32 KeyIndex) const
// {
//     assert(KeyIndex == 0);
//     return CIM_Constant;
// }

void UDistributionVectorUniform::GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const
{
    assert(SubIndex >= 0 && SubIndex < 6);
    assert(KeyIndex == 0);
    ArriveTangent = 0.f;
    LeaveTangent = 0.f;
}

float UDistributionVectorUniform::EvalSub(int32 SubIndex, float InVal)
{
    return GetKeyOut(SubIndex, 0);
}

int32 UDistributionVectorUniform::CreateNewKey(float KeyIn)
{
    return 0;
}

void UDistributionVectorUniform::DeleteKey(int32 KeyIndex)
{
    assert(KeyIndex == 0);
}

int32 UDistributionVectorUniform::SetKeyIn(int32 KeyIndex, float NewInVal)
{
    assert(KeyIndex == 0);
    return 0;
}

void UDistributionVectorUniform::SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal)
{
    assert(SubIndex >= 0 && SubIndex < 6);
    assert(KeyIndex == 0);

    if (SubIndex == 0)
        Min.X = FMath::Min<float>(NewOutVal, Max.X);
    else if (SubIndex == 1)
        Max.X = FMath::Max<float>(NewOutVal, Min.X);
    else if (SubIndex == 2)
        Min.Y = FMath::Min<float>(NewOutVal, Max.Y);
    else if (SubIndex == 3)
        Max.Y = FMath::Max<float>(NewOutVal, Min.Y);
    else if (SubIndex == 4)
        Min.Z = FMath::Min<float>(NewOutVal, Max.Z);
    else
        Max.Z = FMath::Max<float>(NewOutVal, Min.Z);

    bIsDirty = true;
}

// void UDistributionVectorUniform::SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode)
// {
//     assert(KeyIndex == 0);
// }

void UDistributionVectorUniform::SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent)
{
    assert(SubIndex >= 0 && SubIndex < 6);
    assert(KeyIndex == 0);
}

float UDistributionFloat::GetValue(float F, UObject* Data, struct FRandomStream* InRandomStream) const
{
    return 0.0;
}

FRawDistributionFloat::~FRawDistributionFloat()
{
    if (IsValid(Distribution))
    {
        Distribution->MarkAsGarbage();
    }
}

bool FRawDistributionFloat::IsCreated()
{
    return HasLookupTable(/*bInitializeIfNeeded=*/ false) || (Distribution != nullptr);
}

#if WITH_EDITOR
void FRawDistributionFloat::Initialize()
{
    // Nothing to do if we don't have a distribution.
    if (Distribution == nullptr)
    {
        return;
    }

    // does this FRawDist need updating? (if UDist is dirty or somehow the distribution wasn't dirty, but we have no data)
    bool bNeedsUpdating = false;
    if (Distribution->bIsDirty || (LookupTable.IsEmpty() && Distribution->CanBeBaked()))
    {
        if (!Distribution->bIsDirty)
        {
            UE_LOG(
                ELogLevel::Display, TEXT("Somehow Distribution %s wasn't dirty, but its FRawDistribution wasn't ever initialized!"),
                *Distribution->GetName()
            );
        }
        bNeedsUpdating = true;
    }
    // only initialize if we need to
    if (!bNeedsUpdating)
    {
        return;
    }

    // always empty out the lookup table
    LookupTable.Empty();

    // distribution is no longer dirty (if it was)
    // template objects aren't marked as dirty, because any UDists that uses this as an archetype, 
    // aren't the default values, and has already been saved, needs to know to build the FDist
    // if (!Distribution->IsTemplate())
    // {
    //     Distribution->bIsDirty = false;
    // }

    // if the distribution can't be baked out, then we do nothing here
    if (!Distribution->CanBeBaked())
    {
        return;
    }

    // Build and optimize the lookup table.
    BuildLookupTable(&LookupTable, Distribution);
    OptimizeLookupTable(&LookupTable, LOOKUP_TABLE_ERROR_THRESHOLD);

    // fill out our min/max
    Distribution->GetOutRange(MinValue, MaxValue);
}
#endif // WITH_EDITOR

const FRawDistribution* FRawDistributionFloat::GetFastRawDistribution()
{
    if (!IsSimple() || !HasLookupTable())
    {
        return nullptr;
    }

    // if we get here, we better have been initialized!
    assert(!LookupTable.IsEmpty());

    return this;
}

float FRawDistributionFloat::GetValue(float F, UObject* Data, struct FRandomStream* InRandomStream)
{
    if (!HasLookupTable())
    {
        if (!Distribution)
        {
            return 0.0f;
        }
        return Distribution->GetValue(F, Data, InRandomStream);
    }

    // if we get here, we better have been initialized!
    assert(!LookupTable.IsEmpty());

    float Value;
    FRawDistribution::GetValue1(F, &Value, 0, InRandomStream);
    return Value;
}

void FRawDistributionFloat::GetOutRange(float& MinOut, float& MaxOut)
{
    if (!HasLookupTable() && Distribution)
    {
        assert(Distribution);
        Distribution->GetOutRange(MinOut, MaxOut);
    }
    else
    {
        MinOut = MinValue;
        MaxOut = MaxValue;
    }
}

void FRawDistributionFloat::InitLookupTable()
{
#if WITH_EDITOR
    // make sure it's up to date
    if (Distribution)
    {
        if (Distribution->bIsDirty)
        {
            // Distribution->ConditionalPostLoad();
            Initialize();
        }
    }
#endif
}

float UDistributionFloat::GetFloatValue(float F)
{
    return GetValue(F);
}

void UDistributionFloat::GetInRange(float& MinIn, float& MaxIn) const
{
    MinIn = 0.0f;
    MaxIn = 0.0f;
}

void UDistributionFloat::GetOutRange(float& MinOut, float& MaxOut) const
{
    MinOut = 0.0f;
    MaxOut = 0.0f;
}

void UDistributionFloat::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    bIsDirty = true;
}

uint32 UDistributionFloat::InitializeRawEntry(float Time, float* Values) const
{
    Values[0] = GetValue(Time);
    return 1;
}

void UDistributionFloatUniform::PostInitProperties()
{
    Super::PostInitProperties();
    if (GetOuter()->IsA<UParticleModule>())
    {
        // Set to a bogus value for distributions created before VER_UE4_MOVE_DISTRIBUITONS_TO_POSTINITPROPS
        // to be able to restore to the previous default value.
        Min = UDistribution::DefaultValue;
        Max = UDistribution::DefaultValue;
    }
}

float UDistributionFloatUniform::GetValue(float F, UObject* Data, FRandomStream* InRandomStream) const
{
    return Max + (Min - Max) * DIST_GET_RANDOM_VALUE(InRandomStream);
}

ERawDistributionOperation UDistributionFloatUniform::GetOperation() const
{
    if (Min == Max)
    {
        // This may as well be a constant - don't bother doing the FMath::SRand scaling on it.
        return RDO_None;
    }
    return RDO_Random;
}

uint32 UDistributionFloatUniform::InitializeRawEntry(float Time, float* Values) const
{
    Values[0] = Min;
    Values[1] = Max;
    return 2;
}

int32 UDistributionFloatUniform::GetNumKeys() const
{
    return 1;
}

int32 UDistributionFloatUniform::GetNumSubCurves() const
{
    return 2;
}

FColor UDistributionFloatUniform::GetSubCurveButtonColor(int32 SubCurveIndex, bool bIsSubCurveHidden) const
{
    // Check for array out of bounds because it will crash the program
    assert(SubCurveIndex >= 0);
    assert(SubCurveIndex < GetNumSubCurves());

    FColor ButtonColor;

    switch (SubCurveIndex)
    {
    case 0:
        // Red
        ButtonColor = bIsSubCurveHidden ? FColor(32, 0, 0) : FColor::Red;
        break;
    case 1:
        // Green
        ButtonColor = bIsSubCurveHidden ? FColor(0, 32, 0) : FColor::Green;
        break;
    default:
        // A bad sub-curve index was given. 
        assert(false);
        break;
    }

    return ButtonColor;
}

float UDistributionFloatUniform::GetKeyIn(int32 KeyIndex)
{
    assert(KeyIndex == 0);
    return 0.f;
}

float UDistributionFloatUniform::GetKeyOut(int32 SubIndex, int32 KeyIndex)
{
    assert(SubIndex == 0 || SubIndex == 1);
    assert(KeyIndex == 0);
    return (SubIndex == 0) ? Min : Max;
}

FColor UDistributionFloatUniform::GetKeyColor(int32 SubIndex, int32 KeyIndex, const FColor& CurveColor)
{
    // There can only be as many as two sub-curves for this distribution.
    assert(SubIndex == 0 || SubIndex == 1);
    // There can be only be one key for this distribution.
    assert(KeyIndex == 0);

    FColor KeyColor;

    if (0 == SubIndex)
    {
        KeyColor = FColor::Red;
    }
    else
    {
        KeyColor = FColor::Green;
    }

    return KeyColor;
}

void UDistributionFloatUniform::GetInRange(float& MinIn, float& MaxIn) const
{
    MinIn = 0.f;
    MaxIn = 0.f;
}

void UDistributionFloatUniform::GetOutRange(float& MinOut, float& MaxOut) const
{
    MinOut = Min;
    MaxOut = Max;
}

// EInterpCurveMode UDistributionFloatUniform::GetKeyInterpMode(int32 KeyIndex) const
// {
//     assert( KeyIndex == 0 );
//     return CIM_Constant;
// }

void UDistributionFloatUniform::GetTangents(int32 SubIndex, int32 KeyIndex, float& ArriveTangent, float& LeaveTangent) const
{
    assert(SubIndex == 0 || SubIndex == 1);
    assert(KeyIndex == 0);
    ArriveTangent = 0.f;
    LeaveTangent = 0.f;
}

float UDistributionFloatUniform::EvalSub(int32 SubIndex, float InVal)
{
    assert(SubIndex == 0 || SubIndex == 1);
    return (SubIndex == 0) ? Min : Max;
}

int32 UDistributionFloatUniform::CreateNewKey(float KeyIn)
{
    return 0;
}

void UDistributionFloatUniform::DeleteKey(int32 KeyIndex)
{
    assert(KeyIndex == 0);
}

int32 UDistributionFloatUniform::SetKeyIn(int32 KeyIndex, float NewInVal)
{
    assert(KeyIndex == 0);
    return 0;
}

void UDistributionFloatUniform::SetKeyOut(int32 SubIndex, int32 KeyIndex, float NewOutVal)
{
    assert(SubIndex == 0 || SubIndex == 1);
    assert(KeyIndex == 0);

    // We ensure that we can't move the Min past the Max.
    if (SubIndex == 0)
    {
        Min = FMath::Min<float>(NewOutVal, Max);
    }
    else
    {
        Max = FMath::Max<float>(NewOutVal, Min);
    }

    bIsDirty = true;
}

// void UDistributionFloatUniform::SetKeyInterpMode(int32 KeyIndex, EInterpCurveMode NewMode)
// {
//     assert(KeyIndex == 0);
// }

void UDistributionFloatUniform::SetTangents(int32 SubIndex, int32 KeyIndex, float ArriveTangent, float LeaveTangent)
{
    assert(SubIndex == 0 || SubIndex == 1);
    assert(KeyIndex == 0);
}
