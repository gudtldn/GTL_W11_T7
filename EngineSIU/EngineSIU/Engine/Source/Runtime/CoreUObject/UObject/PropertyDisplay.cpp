#include "Property.h"

#include "Class.h"
#include "PropertyEvent.h"
#include "ScriptStruct.h"
#include "UObjectHash.h"
#include "Editor/UnrealEd/ImGuiWidget.h"
#include "Math/NumericLimits.h"
#include "Template/SubclassOf.h"

#include "ImGui/imgui.h"
#include "Misc/Optional.h"

template <typename Type, typename... Types>
concept TIsAnyOf = (std::same_as<Type, Types> || ...);

template <typename T>
concept NumericType = TIsAnyOf<T, int8, int16, int32, int64, uint8, uint16, uint32, uint64, float, double>;

static constexpr int32 IMGUI_FSTRING_BUFFER_SIZE = 2048;


struct FPropertyUIHelper
{
    template <NumericType NumType>
    static TOptional<EPropertyChangeType> DisplayNumericDragN(
        const char* PropertyLabel, void* InData, int Components, float Speed = 1.0f, const char* Format = nullptr
    )
    {
        NumType* Data = static_cast<NumType*>(InData);
        constexpr NumType Min = TNumericLimits<NumType>::Lowest();
        constexpr NumType Max = TNumericLimits<NumType>::Max();

        ImGuiDataType DataType;
        if constexpr (std::same_as<NumType, int8>)        { DataType = ImGuiDataType_S8;     }
        else if constexpr (std::same_as<NumType, int16>)  { DataType = ImGuiDataType_S16;    }
        else if constexpr (std::same_as<NumType, int32>)  { DataType = ImGuiDataType_S32;    }
        else if constexpr (std::same_as<NumType, int64>)  { DataType = ImGuiDataType_S64;    }
        else if constexpr (std::same_as<NumType, uint8>)  { DataType = ImGuiDataType_U8;     }
        else if constexpr (std::same_as<NumType, uint16>) { DataType = ImGuiDataType_U16;    }
        else if constexpr (std::same_as<NumType, uint32>) { DataType = ImGuiDataType_U32;    }
        else if constexpr (std::same_as<NumType, uint64>) { DataType = ImGuiDataType_U64;    }
        else if constexpr (std::same_as<NumType, float>)  { DataType = ImGuiDataType_Float;  }
        else if constexpr (std::same_as<NumType, double>) { DataType = ImGuiDataType_Double; }
        else { static_assert(TAlwaysFalse<NumType>); }

        ImGui::Text("%s", PropertyLabel);
        ImGui::SameLine();
        const std::string FormatStr = std::format("##{}", PropertyLabel);
        const bool bIsInteractive = ImGui::DragScalarN(FormatStr.c_str(), DataType, Data, Components, Speed, &Min, &Max, Format);

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            return EPropertyChangeType::ValueSet;
        }

        if (bIsInteractive)
        {
            return EPropertyChangeType::Interactive;
        }

        return {};
    }
};


void FProperty::DisplayInImGui(UObject* Object) const
{
    void* Data = GetPropertyData(Object);
    DisplayRawDataInImGui(Name, Data, Object);
}

void FProperty::DisplayRawDataInImGui(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    if (!HasAnyFlags(Flags, EPropertyFlags::EditAnywhere | EPropertyFlags::VisibleAnywhere))
    {
        return;
    }

    DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);
}

void FProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
}

void FDisplayMembersRecursiveTrait::DisplayMembersRecursive(
    const UStruct* CurrentStructToDisplay, void* StructInstanceDataPtr, UObject* TopLevelOwnerObject
) const
{
    if (!CurrentStructToDisplay)
    {
        return;
    }

    // 부모 구조체의 멤버 먼저 그리기
    if (CurrentStructToDisplay->GetSuperStruct())
    {
        DisplayMembersRecursive(CurrentStructToDisplay->GetSuperStruct(), StructInstanceDataPtr, TopLevelOwnerObject);
    }

    for (const FProperty* MemberProp : CurrentStructToDisplay->GetProperties())
    {
        void* MemberDataPtr = static_cast<std::byte*>(StructInstanceDataPtr) + MemberProp->Offset;
        MemberProp->DisplayRawDataInImGui(MemberProp->Name, MemberDataPtr, TopLevelOwnerObject);
    }
}

void FNumericProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FInt8Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<int8>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FInt8Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FInt16Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<int16>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FInt16Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FInt32Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<int32>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FInt32Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FInt64Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<int64>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FInt64Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FUInt8Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<uint8>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FUInt8Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FUInt16Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<uint16>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FUInt16Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FUInt32Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<uint32>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FUInt32Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FUInt64Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<uint64>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FUInt64Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FFloatProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<float>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FFloatProperty*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FDoubleProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FNumericProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<double>(PropertyLabel, DataPtr, 1);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FDoubleProperty*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FBoolProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FBoolProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    if (ImGui::Checkbox(PropertyLabel, static_cast<bool*>(DataPtr)))
    {
        if (IsValid(OwnerObject))
        {
            FPropertyChangedEvent Event{const_cast<FBoolProperty*>(this), OwnerObject, EPropertyChangeType::ValueSet};
            OwnerObject->PostEditChangeProperty(Event);
        }
    }
}

void FStrProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FStrProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    FString* Data = static_cast<FString*>(DataPtr);

    char Buffer[IMGUI_FSTRING_BUFFER_SIZE];
    FCStringAnsi::Strncpy(Buffer, Data->ToAnsiString().c_str(), IMGUI_FSTRING_BUFFER_SIZE);
    Buffer[IMGUI_FSTRING_BUFFER_SIZE - 1] = '\0'; // 항상 널 종료 보장

    bool bChanged = false;

    ImGui::Text("%s", PropertyLabel);
    ImGui::SameLine();
    if (ImGui::InputText(std::format("##{}", PropertyLabel).c_str(), Buffer, IMGUI_FSTRING_BUFFER_SIZE, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        bChanged = true;
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) // 포커스 아웃 등으로 편집 완료
    {
        // InputText 내부에서 이미 Buffer가 변경되었을 수 있음
        // 실제 Data와 Buffer를 비교하여 변경되었는지 확인 후 bChanged 설정 가능
        if (*Data != Buffer)
        {
            bChanged = true;
        }
    }

    if (bChanged)
    {
        *Data = Buffer; // 실제 데이터 업데이트
        if (IsValid(OwnerObject))
        {
            FPropertyChangedEvent Event(const_cast<FStrProperty*>(this), OwnerObject, EPropertyChangeType::ValueSet); // ValueSet으로 명시
            OwnerObject->PostEditChangeProperty(Event);
        }
    }
}

void FNameProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const FName* Data = static_cast<FName*>(DataPtr);
    std::string NameStr = Data->ToString().ToAnsiString();

    // ReadOnly Mode
    ImGui::BeginDisabled(true);
    {
        ImGui::Text("%s", PropertyLabel);
        ImGui::SameLine();
        ImGui::InputText(std::format("##{}", PropertyLabel).c_str(), NameStr.data(), NameStr.size(), ImGuiInputTextFlags_ReadOnly);
    }
    ImGui::EndDisabled();
}

void FVector2DProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FVector2DProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<float>(PropertyLabel, DataPtr, 2);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FVector2DProperty*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FVectorProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FVectorProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<float>(PropertyLabel, DataPtr, 3);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FVectorProperty*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FVector4Property::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FVector4Property::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<float>(PropertyLabel, DataPtr, 4);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FVector4Property*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FRotatorProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FRotatorProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<float>(PropertyLabel, DataPtr, 3);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FRotatorProperty*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FQuatProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FQuatProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    const TOptional<EPropertyChangeType> ChangeResult = FPropertyUIHelper::DisplayNumericDragN<float>(PropertyLabel, DataPtr, 4);
    if (!ChangeResult.IsSet()) return;

    FPropertyChangedEvent Event{const_cast<FQuatProperty*>(this), OwnerObject, *ChangeResult};
    OwnerObject->PostEditChangeProperty(Event);
}

void FTransformProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    if (ImGui::TreeNode(PropertyLabel))
    {
        ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
        {
            FTransform* Data = static_cast<FTransform*>(DataPtr);
            FRotator Rotation = Data->Rotator();

            bool bChangedThisFrame = false;
            bChangedThisFrame |= FImGuiWidget::DrawVec3Control("Location", Data->Translation);
            bChangedThisFrame |= FImGuiWidget::DrawRot3Control("Rotation", Rotation);
            bChangedThisFrame |= FImGuiWidget::DrawVec3Control("Scale", Data->Scale3D, 1.0f);

            if (bChangedThisFrame)
            {
                Data->Rotation = Rotation.Quaternion();

                if (IsValid(OwnerObject))
                {
                    FPropertyChangedEvent Event{const_cast<FTransformProperty*>(this), OwnerObject};
                    OwnerObject->PostEditChangeProperty(Event);
                }
            }
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }
}

void FMatrixProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    // TODO: 짐벌락 현상 있음
    if (ImGui::TreeNode(PropertyLabel))
    {
        FMatrix* Data = static_cast<FMatrix*>(DataPtr);

        ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
        {
            FTransform Transform = FTransform(*Data);
            FRotator Rotation = Transform.Rotator();

            bool bChanged = false;
            bChanged |= FImGuiWidget::DrawVec3Control("Location", Transform.Translation);
            bChanged |= FImGuiWidget::DrawRot3Control("Rotation", Rotation);
            bChanged |= FImGuiWidget::DrawVec3Control("Scale", Transform.Scale3D, 1.0f);

            if (bChanged)
            {
                *Data =
                    FMatrix::CreateScaleMatrix(Transform.Scale3D)
                    * FMatrix::CreateRotationMatrix(Rotation.Quaternion())
                    * FMatrix::CreateTranslationMatrix(Transform.Translation);

                if (IsValid(OwnerObject))
                {
                    FPropertyChangedEvent Event{const_cast<FMatrixProperty*>(this), OwnerObject};
                    OwnerObject->PostEditChangeProperty(Event);
                }
            }
        }
        ImGui::EndDisabled();

        if (ImGui::TreeNode("Advanced"))
        {
            ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
            {
                bool bChanged = false;
                bChanged |= ImGui::DragFloat4("##1", Data->M[0], 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
                bChanged |= ImGui::DragFloat4("##2", Data->M[1], 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
                bChanged |= ImGui::DragFloat4("##3", Data->M[2], 0.01f, -FLT_MAX, FLT_MAX, "%.3f");
                bChanged |= ImGui::DragFloat4("##4", Data->M[3], 0.01f, -FLT_MAX, FLT_MAX, "%.3f");

                if (bChanged && IsValid(OwnerObject))
                {
                    FPropertyChangedEvent Event{const_cast<FMatrixProperty*>(this), OwnerObject};
                    OwnerObject->PostEditChangeProperty(Event);
                }
            }
            ImGui::EndDisabled();
            ImGui::TreePop();
        }

        ImGui::TreePop();
    }
}

void FColorProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FColorProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    FColor* Data = static_cast<FColor*>(DataPtr);
    FLinearColor LinearColorForUI = FLinearColor(*Data);

    constexpr ImGuiColorEditFlags Flags =
        ImGuiColorEditFlags_DisplayRGB
        | ImGuiColorEditFlags_AlphaBar
        | ImGuiColorEditFlags_AlphaPreview
        | ImGuiColorEditFlags_AlphaPreviewHalf;

    ImGui::Text("%s", PropertyLabel);
    ImGui::SameLine();
    if (ImGui::ColorEdit4(std::format("##{}", PropertyLabel).c_str(), reinterpret_cast<float*>(&LinearColorForUI), Flags))
    {
        *Data = LinearColorForUI.ToColorRawRGB8();
        if (OwnerObject)
        {
            FPropertyChangedEvent Event(const_cast<FColorProperty*>(this), OwnerObject, EPropertyChangeType::ValueSet);
            OwnerObject->PostEditChangeProperty(Event);
        }
    }

}

void FLinearColorProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FLinearColorProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    FLinearColor* Data = static_cast<FLinearColor*>(DataPtr);

    constexpr ImGuiColorEditFlags Flags =
        ImGuiColorEditFlags_Float
        | ImGuiColorEditFlags_AlphaBar
        | ImGuiColorEditFlags_AlphaPreview
        | ImGuiColorEditFlags_AlphaPreviewHalf;

    ImGui::Text("%s", PropertyLabel);
    ImGui::SameLine();
    if (ImGui::ColorEdit4(std::format("##{}", PropertyLabel).c_str(), reinterpret_cast<float*>(Data), Flags))
    {
        if (OwnerObject)
        {
            FPropertyChangedEvent Event(const_cast<FLinearColorProperty*>(this), OwnerObject, EPropertyChangeType::ValueSet);
            OwnerObject->PostEditChangeProperty(Event);
        }
    }
}

void FSubclassOfProperty::DisplayInImGui(UObject* Object) const
{
    ImGui::BeginDisabled(HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere));
    {
        FProperty::DisplayInImGui(Object);
    }
    ImGui::EndDisabled();
}

void FSubclassOfProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    TSubclassOf<UObject>* Data = static_cast<TSubclassOf<UObject>*>(DataPtr);
    UClass* CurrentClass = GetSpecificClass();
    if (CurrentClass == nullptr)
    {
        return;
    }

    TArray<UClass*> ChildClasses;
    GetChildOfClass(CurrentClass, ChildClasses);

    bool bChanged = false;
    const std::string CurrentClassName = (*Data) ? (*Data)->GetName().ToAnsiString() : "None";
    ImGui::Text("%s", PropertyLabel);
    ImGui::SameLine();
    if (ImGui::BeginCombo(std::format("##{}", PropertyLabel).c_str(), CurrentClassName.c_str()))
    {
        if (ImGui::Selectable("None", !(*Data)))
        {
            *Data = nullptr;
            bChanged = true;
        }

        for (UClass* ChildClass : ChildClasses)
        {
            const std::string ChildClassName = ChildClass->GetName().ToAnsiString();
            const bool bIsSelected = (*Data) && (*Data) == ChildClass;
            if (ImGui::Selectable(ChildClassName.c_str(), bIsSelected))
            {
                *Data = ChildClass;
                bChanged = true;
            }
            if (bIsSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (bChanged)
    {
        if (IsValid(OwnerObject))
        {
            FPropertyChangedEvent Event(const_cast<FSubclassOfProperty*>(this), OwnerObject, EPropertyChangeType::ValueSet);
            OwnerObject->PostEditChangeProperty(Event);
        }
    }
}

void FObjectProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    if (ImGui::TreeNodeEx(PropertyLabel, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        UObject** Object = static_cast<UObject**>(DataPtr);
        const UClass* ObjectClass = IsValid(*Object) ? (*Object)->GetClass() : nullptr;

        // 포인터가 가리키는 객체를 수정, 여기서는 UObject의 인스턴스
        if (HasAnyFlags(Flags, EPropertyFlags::EditAnywhere))
        {
            const UClass* SpecificClass = GetSpecificClass();
            assert(SpecificClass);

            TArray<UObject*> ChildObjects;
            GetObjectsOfClass(SpecificClass, ChildObjects, true);

            std::string PreviewName = "None";
            if (IsValid(*Object))
            {
                PreviewName = (*Object)->GetName().ToAnsiString();
            }

            if (ImGui::BeginCombo(std::format("##{}", PropertyLabel).c_str(), PreviewName.c_str()))
            {
                for (UObject* ChildObject : ChildObjects)
                {
                    const std::string ObjectName = ChildObject->GetName().ToAnsiString();
                    const bool bIsSelected = ChildObject == *Object;
                    if (ImGui::Selectable(ObjectName.c_str(), bIsSelected))
                    {
                        // OwnerObject: 나중에 수정, 지금은 목록만 보여주고, 설정은 안함
                        *Object = ChildObject;
                        FPropertyChangedEvent Event(const_cast<FObjectProperty*>(this), OwnerObject, EPropertyChangeType::ValueSet);
                        OwnerObject->PostEditChangeProperty(Event);
                    }
                    if (bIsSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                    ImGui::Separator();
                }
                ImGui::EndCombo();
            }

            if (HasAnyFlags(Flags, EPropertyFlags::EditInline))
            {
                DisplayMembersRecursive(ObjectClass, *Object, *Object);
            }
        }
        else if (HasAnyFlags(Flags, EPropertyFlags::VisibleAnywhere))
        {
            DisplayMembersRecursive(ObjectClass, *Object, *Object);
        }
        ImGui::TreePop();
    }
}

void FStructProperty::DisplayRawDataInImGui_Implement(const char* PropertyLabel, void* DataPtr, UObject* OwnerObject) const
{
    FProperty::DisplayRawDataInImGui_Implement(PropertyLabel, DataPtr, OwnerObject);

    if (UScriptStruct* const* StructType = std::get_if<UScriptStruct*>(&TypeSpecificData))
    {
        if (ImGui::TreeNodeEx(PropertyLabel, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
        {
            const UStruct* ActualStruct = *StructType;
            DisplayMembersRecursive(ActualStruct, DataPtr, OwnerObject);
            ImGui::TreePop();
        }
    }
}

void FUnresolvedPtrProperty::DisplayInImGui(UObject* Object) const
{
    if (Type == EPropertyType::Unknown)
    {
        return;
    }
    ResolvedProperty->DisplayInImGui(Object);
}
