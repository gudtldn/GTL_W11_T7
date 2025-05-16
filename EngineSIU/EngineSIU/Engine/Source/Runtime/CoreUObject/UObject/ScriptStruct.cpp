#include "ScriptStruct.h"


UScriptStruct::UScriptStruct(
    const char* InName,
    uint32 InStructSize,
    uint32 InAlignment,
    UScriptStruct* InSuperScriptStruct
)
    : UStruct(InName, InStructSize, InAlignment, InSuperScriptStruct)
{
}
