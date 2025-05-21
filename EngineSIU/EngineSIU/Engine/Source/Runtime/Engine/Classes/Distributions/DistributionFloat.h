#pragma once
#include "Distribution.h"
#include "Distributions.h"

class UDistributionFloat;


struct FRawDistributionFloat : public FRawDistribution
{
    DECLARE_STRUCT(FRawDistributionFloat)

private:
    UPROPERTY(
        float, MinValue
    )

    UPROPERTY(
        float, MaxValue
    )

public:
    UPROPERTY_WITH_FLAGS(
        EditAnywhere | EditInline,
        UDistributionFloat*, Distribution
    )


    FRawDistributionFloat()
        : MinValue(0)
        , MaxValue(0)
        , Distribution(nullptr)
    {
    }

    virtual ~FRawDistributionFloat();

    /** Whether the distribution data has been cooked or the object itself is available */
    bool IsCreated();

#if WITH_EDITOR
    /**`
     * Initialize a raw distribution from the original Unreal distribution
     */
    void Initialize();
#endif
    /**
     * Gets a pointer to the raw distribution if you can just call FRawDistribution::GetValue1 on it, otherwise NULL 
     */
    const FRawDistribution* GetFastRawDistribution();

    /**
     * Get the value at the specified F
     */
    float GetValue(float F = 0.0f, UObject* Data = nullptr, struct FRandomStream* InRandomStream = nullptr);

    /**
     * Get the min and max values
     */
    void GetOutRange(float& MinOut, float& MaxOut);

    /**
     * Is this distribution a uniform type? (ie, does it have two values per entry?)
     */
    inline bool IsUniform() const { return LookupTable.SubEntryStride != 0; }

    void InitLookupTable();

    FORCEINLINE bool HasLookupTable(bool bInitializeIfNeeded = true)
    {
#if WITH_EDITOR
        if (bInitializeIfNeeded)
        {
            InitLookupTable();
        }
#endif
        return !LookupTable.IsEmpty();
    }

    FORCEINLINE bool OkForParallel()
    {
        HasLookupTable(); // initialize if required
        return true;      // even if they stay distributions, this should probably be ok as long as nobody is changing them at runtime
        //return !Distribution || HasLookupTable();
    }
};

class UDistributionFloat : public UDistribution
{
    DECLARE_CLASS(UDistributionFloat, UObject)

public:
    // UPROPERTY(EditAnywhere, Category=Baked)
    // uint8 bCanBeBaked:1;

    /** Can this variable be baked out to a FRawDistribution? Should be true 99% of the time*/
    UPROPERTY_WITH_FLAGS(
        EditAnywhere,
        bool, bCanBeBaked, = true;
        // uint8 bCanBeBaked : 1;
    )

    /** Set internally when the distribution is updated so that that FRawDistribution can know to update itself*/
    UPROPERTY_WITH_BITFIELD(
        BitField,
        uint8, bIsDirty, : 1;
    )

protected:
    UPROPERTY_WITH_BITFIELD(
        BitField,
        uint8, bBakedDataSuccessfully, : 1; //It's possible that even though we want to bake we are not able to because of content or code.
    )

public:
    virtual float GetFloatValue(float F = 0);

    UDistributionFloat()
        : bCanBeBaked(true)
        , bIsDirty(true)
    {
    }

    virtual ~UDistributionFloat() override = default;


    //@todo.CONSOLE: Currently, consoles need this? At least until we have some sort of cooking/packaging step!
    /**
     * Return the operation used at runtime to calculate the final value
     */
    virtual ERawDistributionOperation GetOperation() const { return RDO_None; }

    /**
     *  Returns the lock axes flag used at runtime to swizzle random stream values. Not used for distributions derived from UDistributionFloat.
     */
    virtual uint8 GetLockFlag() const { return 0; }

    /**
     * Fill out an array of floats and return the number of elements in the entry
     *
     * @param Time The time to evaluate the distribution
     * @param Values An array of values to be filled out, guaranteed to be big enough for 4 values
     * @return The number of elements (values) set in the array
     */
    virtual uint32 InitializeRawEntry(float Time, float* Values) const;

    /** @todo document */
    virtual float GetValue(float F = 0.f, UObject* Data = nullptr, FRandomStream* InRandomStream = nullptr) const;

    //~ Begin FCurveEdInterface Interface
    virtual void GetInRange(float& MinIn, float& MaxIn) const override;
    virtual void GetOutRange(float& MinOut, float& MaxOut) const override;
    //~ End FCurveEdInterface Interface

    /** @return true of this distribution can be baked into a FRawDistribution lookup table, otherwise false */
    virtual bool CanBeBaked() const
    {
        return bCanBeBaked;
    }

    bool HasBakedSuccessfully() const
    {
        return bBakedDataSuccessfully;
    }

    /**
     * Returns the number of values in the distribution. 1 for float.
     */
    int32 GetValueCount() const
    {
        return 1;
    }

    /** Begin UObject interface */
#if	WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	// WITH_EDITOR
    // virtual bool NeedsLoadForClient() const override;
    // virtual bool NeedsLoadForServer() const override;
    // virtual bool NeedsLoadForEditorGame() const override;
    // virtual void Serialize(FStructuredArchive::FRecord Record) override;
    /** End UObject interface */
};
