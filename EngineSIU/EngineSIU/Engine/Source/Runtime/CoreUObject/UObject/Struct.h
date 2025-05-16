#pragma once
#include "UObject/Object.h"

struct FProperty;


/**
 * UClass와 UScriptStruct의 기본 클래스입니다.
 * 구조체 또는 클래스의 멤버 변수(프로퍼티) 목록, 크기, 정렬 등의 메타데이터를 관리합니다.
 */
class UStruct : public UObject
{
public:
    UStruct(
        const FName& InName,
        uint32 InStructSize,
        uint32 InAlignment,
        UStruct* InSuperStruct
    );

    virtual ~UStruct() override;

    UStruct(const UStruct&) = delete;
    UStruct& operator=(const UStruct&) = delete;
    UStruct(UStruct&&) = delete;
    UStruct& operator=(UStruct&&) = delete;

public:
    [[nodiscard]] uint32 GetStructSize() const { return StructSize; }
    [[nodiscard]] uint32 GetStructAlignment() const { return StructAlignment; }
    [[nodiscard]] UStruct* GetSuperStruct() const { return SuperStruct; }

    /** SomeBase의 자식 Struct인지 확인합니다. */
    bool IsChildOf(const UStruct* SomeBase) const;

    template <typename T>
    requires
        std::derived_from<T, UObject>
    [[nodiscard]] bool IsChildOf() const;

    [[nodiscard]] const TArray<FProperty*>& GetProperties() const { return Properties; }

    /**
     * UStruct에 Property를 추가합니다.
     * @param Prop 추가할 Property
     */
    void AddProperty(FProperty* Prop);

    /** 이름으로 Property를 찾습니다. 부모 Struct의 Property도 검색합니다. */
    [[nodiscard]] FProperty* FindPropertyByName(const FName& InName) const;

    /**
     * 이 Struct 타입의 인스턴스 데이터를 직렬화합니다.
     * @param Ar 직렬화 아카이브
     * @param Data 직렬화할 Struct 데이터의 시작 포인터
     */
    virtual void SerializeBin(FArchive& Ar, void* Data);

    /** 컴파일 타임에 알 수 없는 프로퍼티 타입을 런타임에 검사합니다.*/
    static void ResolvePendingProperties();

private:
    /** 컴파일 타임에 알 수 없는 프로퍼티 목록들*/
    static TArray<FProperty*>& GetUnresolvedProperties();

protected:
    uint32 StructSize;
    uint32 StructAlignment;
    UStruct* SuperStruct;

    // 이 Struct/Class에 직접 정의된 프로퍼티들
    TArray<FProperty*> Properties;

    // 이름으로 프로퍼티를 빠르게 찾기 위한 맵
    TMap<FName, FProperty*> PropertyMap;
};

template <typename T>
requires
    std::derived_from<T, UObject>
bool UStruct::IsChildOf() const
{
    return IsChildOf(T::StaticClass());
}
