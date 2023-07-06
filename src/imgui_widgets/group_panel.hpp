// from https://github.com/ocornut/imgui/issues/1496#issuecomment-655048353
// Many thanks!
#pragma once

#include <imgui.h>

namespace ImGui
{
    void BeginGroupPanel(const char* name, const ImVec2& size = ImVec2(0.0f, 0.0f));
    void EndGroupPanel();
}