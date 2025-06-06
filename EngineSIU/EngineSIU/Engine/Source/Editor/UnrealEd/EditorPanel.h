#pragma once
#include <World/WorldType.h>

#ifndef __ICON_FONT_INDEX__

#define __ICON_FONT_INDEX__
#define DEFAULT_FONT        0
#define    FEATHER_FONT        1

#endif // !__ICON_FONT_INDEX__

enum EWindowType : uint8
{
    WT_Main,
    WT_ParticleSubWindow
};

class UEditorPanel
{
public:
    virtual ~UEditorPanel() = default;
    virtual void Render() = 0;
    virtual void OnResize(HWND hWnd) = 0;

    void  SetSupportedWorldTypes(EWorldTypeBitFlag mask) { SupportedMask = mask; }
    EWorldTypeBitFlag GetSupportedWorldTypes() const { return SupportedMask; }

    EWindowType WindowType = WT_Main;
private:
    EWorldTypeBitFlag SupportedMask = EWorldTypeBitFlag::None;
};
