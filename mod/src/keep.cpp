// Keeps your icon kit choices across restarts.
//
// GD loads its save while the game starts, before mods are loaded, and resets any selected icon
// or colour it thinks is locked (GameManager::checkUsedIcons). So with everything unlocked, every
// restart threw your choices away. We keep our own copy, written to disk the moment you change
// something, and put it back when the main menu first opens.

#include "photos.hpp"

#include <Geode/modify/CharacterColorPage.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/MenuLayer.hpp>

using namespace geode::prelude;

namespace {
    struct Choice {
        char const* key;
        int (*get)(GameManager*);
        void (*set)(GameManager*, int);
        bool (*unlocked)(GameManager*, int);
    };

    Choice const CHOICES[] = {
        {"cube", [](GameManager* gm) { return gm->getPlayerFrame(); }, [](GameManager* gm, int v) { gm->setPlayerFrame(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Cube); }},
        {"ship", [](GameManager* gm) { return gm->getPlayerShip(); }, [](GameManager* gm, int v) { gm->setPlayerShip(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Ship); }},
        {"ball", [](GameManager* gm) { return gm->getPlayerBall(); }, [](GameManager* gm, int v) { gm->setPlayerBall(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Ball); }},
        {"ufo", [](GameManager* gm) { return gm->getPlayerBird(); }, [](GameManager* gm, int v) { gm->setPlayerBird(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Ufo); }},
        {"wave", [](GameManager* gm) { return gm->getPlayerDart(); }, [](GameManager* gm, int v) { gm->setPlayerDart(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Wave); }},
        {"robot", [](GameManager* gm) { return gm->getPlayerRobot(); }, [](GameManager* gm, int v) { gm->setPlayerRobot(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Robot); }},
        {"spider", [](GameManager* gm) { return gm->getPlayerSpider(); }, [](GameManager* gm, int v) { gm->setPlayerSpider(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Spider); }},
        {"swing", [](GameManager* gm) { return gm->getPlayerSwing(); }, [](GameManager* gm, int v) { gm->setPlayerSwing(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Swing); }},
        {"jetpack", [](GameManager* gm) { return gm->getPlayerJetpack(); }, [](GameManager* gm, int v) { gm->setPlayerJetpack(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Jetpack); }},
        {"trail", [](GameManager* gm) { return gm->getPlayerStreak(); }, [](GameManager* gm, int v) { gm->setPlayerStreak(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::Special); }},
        {"ship-fire", [](GameManager* gm) { return gm->getPlayerShipFire(); }, [](GameManager* gm, int v) { gm->setPlayerShipStreak(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::ShipFire); }},
        {"death-effect", [](GameManager* gm) { return gm->getPlayerDeathEffect(); }, [](GameManager* gm, int v) { gm->setPlayerDeathEffect(v); },
            [](GameManager* gm, int v) { return gm->isIconUnlocked(v, IconType::DeathEffect); }},
        {"color-1", [](GameManager* gm) { return gm->getPlayerColor(); }, [](GameManager* gm, int v) { gm->setPlayerColor(v); },
            [](GameManager* gm, int v) { return gm->isColorUnlocked(v, UnlockType::Col1); }},
        {"color-2", [](GameManager* gm) { return gm->getPlayerColor2(); }, [](GameManager* gm, int v) { gm->setPlayerColor2(v); },
            [](GameManager* gm, int v) { return gm->isColorUnlocked(v, UnlockType::Col2); }},
        {"glow-color", [](GameManager* gm) { return gm->getPlayerGlowColor(); }, [](GameManager* gm, int v) { gm->setPlayerColor3(v); },
            [](GameManager* gm, int v) { return gm->isColorUnlocked(v, UnlockType::Col1) || gm->isColorUnlocked(v, UnlockType::Col2); }},
        {"glow", [](GameManager* gm) { return gm->getPlayerGlow() ? 1 : 0; }, [](GameManager* gm, int v) { gm->setPlayerGlow(v != 0); },
            [](GameManager*, int) { return true; }},
        {"gamemode", [](GameManager* gm) { return int(gm->m_playerIconType); },
            [](GameManager* gm, int v) { gm->m_playerIconType = IconType(v); }, [](GameManager*, int) { return true; }},
    };

    std::string savedKey(Choice const& choice) {
        return fmt::format("kept-{}", choice.key);
    }

    // Until our copy has been put back, GD's values may be the reset ones; don't save those over it.
    bool g_restored = false;

    void restoreChoices() {
        if (g_restored) return;
        g_restored = true;
        auto gm = GameManager::sharedState();
        auto mod = Mod::get();
        int restored = 0;
        for (auto const& choice : CHOICES) {
            int value = mod->getSavedValue<int>(savedKey(choice), -1);
            if (value < 0 || value == choice.get(gm) || !choice.unlocked(gm, value)) continue;
            choice.set(gm, value);
            ++restored;
        }
        if (restored) log::info("Put back {} icon kit choices that GD reset while starting", restored);
    }

    void keepChoices() {
        if (!g_restored) return;
        auto gm = GameManager::sharedState();
        auto mod = Mod::get();
        for (auto const& choice : CHOICES) {
            mod->setSavedValue<int>(savedKey(choice), choice.get(gm));
        }
        skins::saveNow();
    }
}

class $modify(KeepMenuLayer, MenuLayer) {
    bool init() {
        // before the menu builds, so its player icons already show your choices
        restoreChoices();
        return MenuLayer::init();
    }
};

class $modify(KeepGameManager, GameManager) {
    // whenever GD saves (including when you leave the app), our copy is saved too
    void encodeDataTo(DS_Dictionary* dict) {
        keepChoices();
        GameManager::encodeDataTo(dict);
    }
};

class $modify(KeepGarageLayer, GJGarageLayer) {
    void onSelect(CCObject* sender) {
        GJGarageLayer::onSelect(sender);
        keepChoices();
    }

    void onBack(CCObject* sender) {
        keepChoices();
        GJGarageLayer::onBack(sender);
    }

    void keyBackClicked() {
        keepChoices();
        GJGarageLayer::keyBackClicked();
    }
};

class $modify(KeepColorPage, CharacterColorPage) {
    void onPlayerColor(CCObject* sender) {
        CharacterColorPage::onPlayerColor(sender);
        keepChoices();
    }

    void toggleGlow(CCObject* sender) {
        CharacterColorPage::toggleGlow(sender);
        keepChoices();
    }

    void onClose(CCObject* sender) {
        keepChoices();
        CharacterColorPage::onClose(sender);
    }
};
