#pragma once
#include "Menu/MenuModel.h"
#include <string>

namespace cm {

// Parse already-decoded menu-file text into a model. Robust to malformed lines;
// unknown keys ignored; missing theme keys keep Theme{} defaults.
MenuModel ParseMenu(const std::wstring& text);

// "R,G,B" or "R,G,B,A" (0-255, whitespace tolerant). Returns false on any bad component.
bool ParseColor(const std::wstring& value, Color& out);

// Decode raw file bytes to wstring: UTF-16LE (FF FE BOM), or UTF-8 (with/without EF BB BF BOM).
std::wstring DecodeBytes(const std::string& bytes);

// Read a menu file from disk and decode it. Returns false if the file cannot be opened.
bool ReadMenuFile(const std::wstring& path, std::wstring& out);

} // namespace cm
