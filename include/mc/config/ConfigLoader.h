#pragma once
//
// ConfigLoader — turns the converted JSON (data/*.json) into domain objects.
// This is the ONE place that knows about nlohmann::json; the domain core never
// sees it, so swapping in an MCU-friendly parser later (Phase 3) is a local edit.
//
// Loaders are split so each can be used independently (and tested):
//   loadPedalConfig*  -> PedalConfig (pedal command map)
//   loadSong*/Setlist -> Setlist/Song/Part
//   loadControllerState* -> ControllerState (+ channel table, button setup)
//
#include <functional>
#include <string>

#include "mc/domain/ControllerState.h"
#include "mc/domain/PedalConfig.h"
#include "mc/domain/SongPartSet.h"

namespace mc::config {

// --- pedal command maps ------------------------------------------------------
PedalConfig loadPedalConfigFromString(const std::string& jsonText);
PedalConfig loadPedalConfigFromFile(const std::string& path);

// --- songs / sets ------------------------------------------------------------
Song loadSongFromString(const std::string& jsonText, const std::string& fallbackName = "");
Song loadSongFromFile(const std::string& path);
// Loads a set file plus each referenced song from songsDir (files named "<song>.json").
Setlist loadSetlistFromFile(const std::string& setFilePath, const std::string& songsDir);
// Same, but songs are fetched by name through a callback (e.g. an IConfigStore).
Setlist loadSetlistFromString(const std::string& setJsonText,
                              const std::function<std::string(const std::string& songName)>& fetchSongJson);

// --- controller config -------------------------------------------------------
ControllerState loadControllerStateFromString(const std::string& jsonText);
ControllerState loadControllerStateFromFile(const std::string& path);

// Utility: read a whole file into a string (throws std::runtime_error if missing).
std::string readFile(const std::string& path);

}  // namespace mc::config
