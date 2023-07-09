// Adapted from https://github.com/cmaughan/zep_imgui/blob/main.

#pragma once

#include <zep.h>

// Helpers to create zep editor
Zep::ZepEditor& zep_get_editor();
void zep_init(const Zep::NVec2f& pixelScale);
void zep_update();
void zep_show(const Zep::NVec2i& displaySize);
void zep_destroy();
void zep_load(const std::filesystem::path& file);