// Every icon, trail, death effect, ship fire and colour counts as unlocked while the setting is on.
// Nothing is written to your save: turn the setting off and the game's own unlocks are back.

#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>

using namespace geode::prelude;

class $modify(UnlockGameManager, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (Mod::get()->getSettingValue<bool>("unlock-all")) return true;
        return GameManager::isIconUnlocked(id, type);
    }

    bool isColorUnlocked(int id, UnlockType type) {
        if (Mod::get()->getSettingValue<bool>("unlock-all")) return true;
        return GameManager::isColorUnlocked(id, type);
    }
};
