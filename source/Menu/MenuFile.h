#pragma once
#include "Menu/MenuModel.h"
#include <string>

namespace cm {

// Parse already-decoded menu-file text into a model. Robust to malformed lines;
// unknown keys ignored; missing theme keys keep Theme{} defaults.
MenuModel ParseMenu(const std::wstring& text);

// "R,G,B" or "R,G,B,A" (0-255, whitespace tolerant). Returns false on any bad component.
bool ParseColor(const std::wstring& value, Color& out);

} // namespace cm
