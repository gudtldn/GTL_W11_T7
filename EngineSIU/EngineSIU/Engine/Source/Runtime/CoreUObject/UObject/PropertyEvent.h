#pragma once
#include "HAL/PlatformType.h"

class UObject;
struct FProperty;


enum class EPropertyChangeType : uint8
{
    Interactive, // 사용자가 드래그하는 등 실시간으로 여러 번 발생 가능
    ValueSet,    // 최종적으로 값이 설정되었을 때 (예: 입력 필드에서 Enter, 드래그 완료)
    ArrayAdd,    // 배열에 요소 추가
    ArrayRemove, // 배열에서 요소 제거
    ArrayClear,  // 배열 비우기
};

struct FPropertyChangedEvent
{
    // 변경된 프로퍼티의 FProperty 객체 (가장 직접적인 변경)
    FProperty* Property = nullptr;

    // 만약 Property가 구조체나 배열이고, 그 내부 멤버가 변경된 경우 해당 멤버 FProperty
    // FProperty* MemberProperty = nullptr;

    // (예: Transform.Location.X -> Property는 Transform, MemberProperty는 Location의 X)
    // (구현 복잡도에 따라 이 부분은 단순화하거나 확장 가능)

    // 프로퍼티 변경이 발생한 최상위 UObject
    UObject* ObjectThatChanged = nullptr;

    EPropertyChangeType ChangeType = EPropertyChangeType::ValueSet;

    // int32 ArrayIndices[2] = {-1, -1}; // 배열 변경 시 인덱스 정보 (예: 추가된 위치, 삭제된 위치 범위)

public:
    FPropertyChangedEvent(FProperty* InProperty, UObject* InObjectThatChanged, EPropertyChangeType InChangeType = EPropertyChangeType::ValueSet)
        : Property(InProperty)
        , ObjectThatChanged(InObjectThatChanged)
        , ChangeType(InChangeType)
    {
    }
};
