#pragma once

// Saved photo icons (PNG files in the mod's save folder) and which gamemode uses which.

#include "image.hpp"
#include <string>
#include <vector>

class SimplePlayer;

namespace skins {
    // Gamemodes a photo can stand in for. Robot and spider are animated multi-part
    // sprites, so they keep their normal icons.
    std::vector<IconType> const& photoModes();
    char const* modeName(IconType mode);
    // Sprite frame GD shows on the main layer of an icon, e.g. "player_12_001.png".
    std::string mainFrameName(IconType mode, int id);

    std::filesystem::path photoDir();
    // File names (without .png) of saved photo icons, newest first.
    std::vector<std::string> listPhotos();
    geode::Result<std::string> addPhoto(Pixels const& icon, std::string const& name);
    void deletePhoto(std::string const& id);

    std::string assignedPhoto(IconType mode);
    void assignPhoto(IconType mode, std::string const& id);
    // Photo to draw for this gamemode right now, or "" for the normal icon.
    std::string activePhoto(IconType mode);

    cocos2d::CCTexture2D* photoTexture(std::string const& id);

    // Writes the mod's saved choices to disk right away, so they survive the app being closed
    // before GD gets to save.
    void saveNow();

    // The icon kit preview is the only SimplePlayer that shows photos (others show other players).
    void showPhotosOn(SimplePlayer* player);
    void refreshPhoto(SimplePlayer* player);
}
