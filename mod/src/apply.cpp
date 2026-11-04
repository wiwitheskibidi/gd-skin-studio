// Draws photo icons on your own player (in levels and the editor) and on the icon kit preview.
//
// GD keeps moving, rotating, scaling and fading its own icon sprite, so the photo rides on that
// sprite as a child while the sprite itself draws nothing. Everything we change is recorded and put
// back before GD redraws the icon, so switching icons or gamemodes always starts from GD's own state.

#include "photos.hpp"

#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/SimplePlayer.hpp>
#include <algorithm>
#include <unordered_map>

using namespace geode::prelude;

namespace {
    struct PhotoState {
        Ref<CCSprite> target;
        Ref<CCSpriteFrame> frame;
        Ref<CCSprite> photo;
        bool cascadeColor = false;
        bool cascadeOpacity = false;
        std::vector<std::pair<Ref<CCNode>, bool>> hidden;

        bool active() const {
            return target.data() != nullptr;
        }

        // Hides a node, remembering the visibility GD wanted for it.
        void hide(CCNode* node) {
            if (!node) return;
            auto it = std::find_if(hidden.begin(), hidden.end(), [&](auto const& entry) { return entry.first == node; });
            if (it != hidden.end()) {
                it->second = node->isVisible();
            }
            else {
                hidden.emplace_back(node, node->isVisible());
            }
            node->setVisible(false);
        }

        void apply(CCSprite* sprite, std::initializer_list<CCNode*> extras, CCTexture2D* tex) {
            if (!sprite || !tex || sprite->getBatchNode()) return;
            auto size = sprite->getContentSize();
            float side = std::max(size.width, size.height);
            if (side <= 0.f) return;

            target = sprite;
            frame = sprite->displayFrame();
            cascadeColor = sprite->isCascadeColorEnabled();
            cascadeOpacity = sprite->isCascadeOpacityEnabled();

            // colour layers, glow, details: everything that belongs to the normal icon
            if (auto children = sprite->getChildren()) {
                for (auto child : CCArrayExt<CCNode*>(children)) this->hide(child);
            }
            for (auto node : extras) this->hide(node);

            sprite->setTextureRect(CCRect(0.f, 0.f, 0.f, 0.f));
            // keep the photo's real colours, but let it fade with the player
            sprite->setCascadeColorEnabled(false);
            sprite->setCascadeOpacityEnabled(true);

            auto spr = CCSprite::createWithTexture(tex);
            spr->setScale(side / spr->getContentSize().width);
            spr->setID("photo-icon"_spr);
            sprite->addChild(spr, 10);
            spr->updateDisplayedOpacity(sprite->getDisplayedOpacity());
            photo = spr;
        }

        void undo() {
            if (photo) photo->removeFromParent();
            if (auto sprite = target.data()) {
                if (frame) sprite->setDisplayFrame(frame);
                sprite->setCascadeColorEnabled(cascadeColor);
                sprite->setCascadeOpacityEnabled(cascadeOpacity);
            }
            for (auto& [node, visible] : hidden) node->setVisible(visible);

            photo = nullptr;
            target = nullptr;
            frame = nullptr;
            hidden.clear();
        }
    };

    CCSpriteFrame* findFrame(IconType mode, int id) {
        auto name = skins::mainFrameName(mode, id);
        if (name.empty()) return nullptr;
        return CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(name.c_str());
    }
}

class $modify(PhotoPlayerObject, PlayerObject) {
    struct Fields {
        PhotoState icon;     // on m_iconSprite (cube, ball, wave, swing; the cube inside ships and UFOs)
        PhotoState vehicle;  // on m_vehicleSprite (ship, UFO, jetpack)
        std::unordered_map<int, int> ids;  // last icon GD drew for each gamemode
        int depth = 0;
        bool ready = false;
    };

    bool isLocalPlayer() {
        auto layer = m_gameLayer;
        if (!layer) return false;
        if (layer->m_player1 == this || layer->m_player2 == this) return true;
        // the level's own players draw themselves before the level has stored them
        return !layer->m_player1 || !layer->m_player2;
    }

    void beginDraw() {
        auto f = m_fields.self();
        if (f->depth++ == 0) {
            f->icon.undo();
            f->vehicle.undo();
        }
    }

    void endDraw() {
        auto f = m_fields.self();
        if (--f->depth == 0) this->applyPhotos();
    }

    void applyPhotos() {
        auto f = m_fields.self();
        if (!f->ready || !this->isLocalPlayer()) return;

        for (auto mode : skins::photoModes()) {
            auto id = f->ids.find(int(mode));
            if (id == f->ids.end()) continue;
            auto photo = skins::activePhoto(mode);
            if (photo.empty()) continue;
            auto frame = findFrame(mode, id->second);
            if (!frame) continue;
            auto tex = skins::photoTexture(photo);
            if (!tex) continue;

            if (!f->icon.active() && m_iconSprite && m_iconSprite->isFrameDisplayed(frame)) {
                f->icon.apply(m_iconSprite, {m_iconSpriteSecondary, m_iconSpriteWhitener, m_iconGlow}, tex);
            }
            else if (!f->vehicle.active() && m_vehicleSprite && m_vehicleSprite->isFrameDisplayed(frame)) {
                f->vehicle.apply(
                    m_vehicleSprite, {m_vehicleSpriteSecondary, m_birdVehicle, m_vehicleSpriteWhitener, m_vehicleGlow}, tex
                );
            }
        }
    }

    bool init(int player, int ship, GJBaseGameLayer* gameLayer, CCLayer* layer, bool playLayer) {
        if (!PlayerObject::init(player, ship, gameLayer, layer, playLayer)) return false;
        m_fields->ready = true;
        this->beginDraw();
        this->endDraw();
        return true;
    }

    void updatePlayerFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Cube)] = frame;
        PlayerObject::updatePlayerFrame(frame);
        this->endDraw();
    }

    void updatePlayerShipFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Ship)] = frame;
        PlayerObject::updatePlayerShipFrame(frame);
        this->endDraw();
    }

    void updatePlayerRollFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Ball)] = frame;
        PlayerObject::updatePlayerRollFrame(frame);
        this->endDraw();
    }

    void updatePlayerBirdFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Ufo)] = frame;
        PlayerObject::updatePlayerBirdFrame(frame);
        this->endDraw();
    }

    void updatePlayerDartFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Wave)] = frame;
        PlayerObject::updatePlayerDartFrame(frame);
        this->endDraw();
    }

    void updatePlayerSwingFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Swing)] = frame;
        PlayerObject::updatePlayerSwingFrame(frame);
        this->endDraw();
    }

    void updatePlayerJetpackFrame(int frame) {
        this->beginDraw();
        m_fields->ids[int(IconType::Jetpack)] = frame;
        PlayerObject::updatePlayerJetpackFrame(frame);
        this->endDraw();
    }

    // GD can switch the glow back on without redrawing the icon; keep it off under a photo
    void updatePlayerGlow() {
        PlayerObject::updatePlayerGlow();
        auto f = m_fields.self();
        if (f->icon.active()) f->icon.hide(m_iconGlow);
        if (f->vehicle.active()) f->vehicle.hide(m_vehicleGlow);
    }
};

class $modify(PhotoSimplePlayer, SimplePlayer) {
    struct Fields {
        bool enabled = false;
        PhotoState state;
        IconType type = IconType::Cube;
        int id = -1;
        int retries = 0;
    };

    void refreshPhoto() {
        auto f = m_fields.self();
        f->state.undo();
        if (!f->enabled || f->id < 0) return;
        auto photo = skins::activePhoto(f->type);
        if (photo.empty() || !m_firstLayer || !m_firstLayer->isVisible()) return;

        auto frame = findFrame(f->type, f->id);
        if (!frame || !m_firstLayer->isFrameDisplayed(frame)) {
            // GD loads icon kit icons in the background; try again once this one has arrived
            if (f->retries++ < 30) {
                this->scheduleOnce(schedule_selector(PhotoSimplePlayer::onRetryPhoto), 0.1f);
            }
            return;
        }
        f->retries = 0;
        f->state.apply(m_firstLayer, {m_secondLayer, m_birdDome, m_outlineSprite, m_detailSprite}, skins::photoTexture(photo));
    }

    void onRetryPhoto(float) {
        this->refreshPhoto();
    }

    void updatePlayerFrame(int id, IconType type) {
        auto f = m_fields.self();
        f->state.undo();
        SimplePlayer::updatePlayerFrame(id, type);
        f->id = id;
        f->type = type;
        f->retries = 0;
        this->refreshPhoto();
    }
};

namespace skins {
    void showPhotosOn(SimplePlayer* player) {
        if (!player) return;
        auto photoPlayer = static_cast<PhotoSimplePlayer*>(player);
        auto f = photoPlayer->m_fields.self();
        f->enabled = true;

        if (f->id < 0 && player->m_firstLayer) {
            // drawn before we were watching: work out which of your icons it shows
            auto gm = GameManager::sharedState();
            for (auto mode : photoModes()) {
                int id = gm->activeIconForType(mode);
                auto frame = findFrame(mode, id);
                if (frame && player->m_firstLayer->isFrameDisplayed(frame)) {
                    f->type = mode;
                    f->id = id;
                    break;
                }
            }
        }
        photoPlayer->refreshPhoto();
    }

    void refreshPhoto(SimplePlayer* player) {
        if (player) static_cast<PhotoSimplePlayer*>(player)->refreshPhoto();
    }
}
