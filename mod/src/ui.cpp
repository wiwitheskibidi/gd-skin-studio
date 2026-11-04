// The "Photos" button in the icon kit, the photo icon manager, and the photo editor.

#include "photos.hpp"

#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <algorithm>
#include <functional>

using namespace geode::prelude;

class PhotoEditorPopup : public Popup {
protected:
    skins::Pixels m_source;
    skins::IconEdit m_edit;
    IconType m_mode = IconType::Cube;
    std::function<void()> m_onSaved;
    CCSprite* m_preview = nullptr;
    TextInput* m_name = nullptr;
    ButtonSprite* m_shapeLabel = nullptr;
    ButtonSprite* m_outlineLabel = nullptr;
    bool m_dirty = true;

    static char const* shapeName(skins::Shape shape) {
        switch (shape) {
            case skins::Shape::Square: return "Square";
            case skins::Shape::Circle: return "Circle";
            default: return "Rounded";
        }
    }

    static ButtonSprite* smallButton(char const* text) {
        return ButtonSprite::create(text, 90, true, "goldFont.fnt", "GJ_button_04.png", 28.f, .55f);
    }

    void addButton(ButtonSprite* spr, CCPoint offset, geode::Function<void(CCMenuItemSpriteExtra*)> callback) {
        m_buttonMenu->addChildAtPosition(CCMenuItemExt::createSpriteExtra(spr, std::move(callback)), Anchor::Center, offset);
    }

    void addSlider(char const* label, CCPoint offset, SEL_MenuHandler callback, float value) {
        auto text = CCLabelBMFont::create(label, "goldFont.fnt");
        text->setScale(.45f);
        m_mainLayer->addChildAtPosition(text, Anchor::Center, offset + CCPoint(0.f, 15.f));
        auto slider = ::Slider::create(this, callback, .65f);
        slider->setValue(value);
        m_mainLayer->addChildAtPosition(slider, Anchor::Center, offset);
    }

    bool init(skins::Pixels source, IconType mode, std::string const& name, std::function<void()> onSaved) {
        if (!Popup::init(420.f, 270.f)) return false;
        m_source = std::move(source);
        m_mode = mode;
        m_onSaved = std::move(onSaved);
        this->setTitle(fmt::format("New {} photo icon", skins::modeName(mode)));

        // preview on the left
        auto backdrop = CCScale9Sprite::create("square02b_001.png", {0.f, 0.f, 80.f, 80.f});
        backdrop->setContentSize({136.f, 136.f});
        backdrop->setColor({0, 0, 0});
        backdrop->setOpacity(90);
        m_mainLayer->addChildAtPosition(backdrop, Anchor::Left, {95.f, 12.f});

        m_name = TextInput::create(150.f, "Name", "bigFont.fnt");
        m_name->setMaxCharCount(24);
        m_name->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
        m_name->setString(name);
        m_mainLayer->addChildAtPosition(m_name, Anchor::Left, {95.f, -82.f});

        // controls on the right
        this->addSlider("Zoom", {85.f, 72.f}, menu_selector(PhotoEditorPopup::onZoom), 0.f);
        this->addSlider("Move left / right", {85.f, 34.f}, menu_selector(PhotoEditorPopup::onPanX), .5f);
        this->addSlider("Move down / up", {85.f, -4.f}, menu_selector(PhotoEditorPopup::onPanY), .5f);

        this->addButton(smallButton("Rotate"), {35.f, -40.f}, [this](CCMenuItemSpriteExtra*) {
            m_edit.rotation = (m_edit.rotation + 1) % 4;
            m_dirty = true;
        });
        this->addButton(smallButton("Flip"), {135.f, -40.f}, [this](CCMenuItemSpriteExtra*) {
            m_edit.flip = !m_edit.flip;
            m_dirty = true;
        });

        m_shapeLabel = smallButton(shapeName(m_edit.shape));
        this->addButton(m_shapeLabel, {35.f, -74.f}, [this](CCMenuItemSpriteExtra*) {
            m_edit.shape = skins::Shape((int(m_edit.shape) + 1) % 3);
            m_shapeLabel->setString(shapeName(m_edit.shape));
            m_dirty = true;
        });
        m_outlineLabel = smallButton(m_edit.outline ? "Outline" : "No outline");
        this->addButton(m_outlineLabel, {135.f, -74.f}, [this](CCMenuItemSpriteExtra*) {
            m_edit.outline = !m_edit.outline;
            m_outlineLabel->setString(m_edit.outline ? "Outline" : "No outline");
            m_dirty = true;
        });

        auto save = CCMenuItemExt::createSpriteExtra(
            ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", .8f),
            [this](CCMenuItemSpriteExtra*) { this->onSave(); }
        );
        m_buttonMenu->addChildAtPosition(save, Anchor::Bottom, {85.f, 22.f});

        // the preview redraws at most once per frame, however fast the sliders move
        this->scheduleUpdate();
        return true;
    }

    void update(float) override {
        if (m_dirty) this->render();
    }

    void render() {
        m_dirty = false;
        auto tex = skins::PixelTexture::create(skins::renderIcon(m_source, m_edit));
        if (!tex) return;
        if (m_preview) m_preview->removeFromParent();
        m_preview = CCSprite::createWithTexture(tex);
        m_preview->setScale(120.f / m_preview->getContentSize().width);
        m_mainLayer->addChildAtPosition(m_preview, Anchor::Left, {95.f, 12.f});
    }

    void onZoom(CCObject* sender) {
        m_edit.zoom = 1.f + static_cast<::SliderThumb*>(sender)->getValue() * 4.f;
        m_dirty = true;
    }

    void onPanX(CCObject* sender) {
        m_edit.panX = static_cast<::SliderThumb*>(sender)->getValue() * 2.f - 1.f;
        m_dirty = true;
    }

    void onPanY(CCObject* sender) {
        m_edit.panY = static_cast<::SliderThumb*>(sender)->getValue() * 2.f - 1.f;
        m_dirty = true;
    }

    void onSave() {
        auto saved = skins::addPhoto(skins::renderIcon(m_source, m_edit), std::string(m_name->getString()));
        if (saved.isErr()) {
            Notification::create(fmt::format("Couldn't save the photo: {}", saved.unwrapErr()), NotificationIcon::Error)->show();
            return;
        }
        auto id = std::move(saved).unwrap();
        skins::assignPhoto(m_mode, id);
        Notification::create(
            fmt::format("\"{}\" is now your {}", id, skins::modeName(m_mode)), NotificationIcon::Success
        )->show();
        if (m_onSaved) m_onSaved();
        this->onClose(nullptr);
    }

public:
    static PhotoEditorPopup* create(
        skins::Pixels source, IconType mode, std::string const& name, std::function<void()> onSaved
    ) {
        auto ret = new PhotoEditorPopup();
        if (ret->init(std::move(source), mode, name, std::move(onSaved))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class PhotoIconsPopup : public Popup {
protected:
    Ref<SimplePlayer> m_garagePlayer;
    IconType m_mode = IconType::Cube;
    int m_page = 0;
    bool m_deleteMode = false;
    CCMenu* m_content = nullptr;  // tabs, photos and buttons; rebuilt after every change
    CCNode* m_labels = nullptr;
    async::TaskHolder<file::PickResult> m_pick;

    static constexpr int COLUMNS = 5;
    static constexpr int PER_PAGE = 10;

    bool init(SimplePlayer* garagePlayer, IconType mode) {
        if (!Popup::init(420.f, 270.f)) return false;
        m_garagePlayer = garagePlayer;
        auto const& modes = skins::photoModes();
        m_mode = std::find(modes.begin(), modes.end(), mode) != modes.end() ? mode : IconType::Cube;
        this->setTitle("Photo Icons");

        m_content = CCMenu::create();
        m_content->ignoreAnchorPointForPosition(false);
        m_content->setContentSize(m_size);
        m_mainLayer->addChildAtPosition(m_content, Anchor::Center);

        m_labels = CCNode::create();
        m_labels->setContentSize(m_size);
        m_labels->setAnchorPoint({.5f, .5f});
        m_mainLayer->addChildAtPosition(m_labels, Anchor::Center);

        this->refresh();
        return true;
    }

    void addButton(CCNode* spr, Anchor anchor, CCPoint offset, geode::Function<void(CCMenuItemSpriteExtra*)> callback) {
        m_content->addChildAtPosition(CCMenuItemExt::createSpriteExtra(spr, std::move(callback)), anchor, offset);
    }

    void addLabel(std::string const& text, char const* font, float width, float scale, Anchor anchor, CCPoint offset) {
        auto label = CCLabelBMFont::create(text.c_str(), font);
        label->setAlignment(kCCTextAlignmentCenter);
        label->limitLabelWidth(width, scale, .2f);
        m_labels->addChildAtPosition(label, anchor, offset);
    }

    void refresh() {
        m_content->removeAllChildren();
        m_labels->removeAllChildren();
        auto const& modes = skins::photoModes();
        auto current = skins::assignedPhoto(m_mode);

        // one tab per gamemode; gold text = that gamemode already uses a photo
        float tabGap = 57.f;
        float firstX = -tabGap * float(modes.size() - 1) / 2.f;
        for (size_t i = 0; i < modes.size(); ++i) {
            auto mode = modes[i];
            auto spr = ButtonSprite::create(
                skins::modeName(mode), 50, true, skins::assignedPhoto(mode).empty() ? "bigFont.fnt" : "goldFont.fnt",
                mode == m_mode ? "GJ_button_01.png" : "GJ_button_04.png", 24.f, .45f
            );
            this->addButton(spr, Anchor::Top, {firstX + tabGap * float(i), -50.f}, [this, mode](CCMenuItemSpriteExtra*) {
                m_mode = mode;
                m_deleteMode = false;
                this->refreshSoon();
            });
        }

        std::string status;
        if (m_deleteMode) {
            status = "Tap a photo to delete it";
        }
        else if (current.empty()) {
            status = fmt::format("{}: your normal icon. Tap a photo to use it.", skins::modeName(m_mode));
        }
        else {
            status = fmt::format("{}: {}", skins::modeName(m_mode), current);
        }
        this->addLabel(status, "chatFont.fnt", 380.f, .7f, Anchor::Top, {0.f, -76.f});

        auto photos = skins::listPhotos();
        int pages = std::max(1, int((photos.size() + PER_PAGE - 1) / PER_PAGE));
        m_page = std::clamp(m_page, 0, pages - 1);

        if (photos.empty()) {
            this->addLabel(
                "No photos yet.\nTap Import Photo to pick one.\nOnly you can see photo icons.",
                "bigFont.fnt", 340.f, .45f, Anchor::Center, {0.f, 0.f}
            );
        }
        for (int i = 0; i < PER_PAGE; ++i) {
            size_t index = size_t(m_page) * PER_PAGE + size_t(i);
            if (index >= photos.size()) break;
            int col = i % COLUMNS, row = i / COLUMNS;
            CCPoint offset{(col - (COLUMNS - 1) / 2.f) * 66.f, 22.f - row * 66.f};
            m_content->addChildAtPosition(this->photoButton(photos[index], photos[index] == current), Anchor::Center, offset);
        }

        if (pages > 1) {
            auto left = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
            left->setScale(.6f);
            this->addButton(left, Anchor::Left, {22.f, -8.f}, [this, pages](CCMenuItemSpriteExtra*) {
                m_page = (m_page + pages - 1) % pages;
                this->refreshSoon();
            });
            auto right = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
            right->setFlipX(true);
            right->setScale(.6f);
            this->addButton(right, Anchor::Right, {-22.f, -8.f}, [this, pages](CCMenuItemSpriteExtra*) {
                m_page = (m_page + 1) % pages;
                this->refreshSoon();
            });
            this->addLabel(fmt::format("{}/{}", m_page + 1, pages), "chatFont.fnt", 40.f, .6f, Anchor::Right, {-22.f, -38.f});
        }

        this->addButton(
            ButtonSprite::create("Import Photo", "goldFont.fnt", "GJ_button_01.png", .55f), Anchor::Bottom, {-125.f, 22.f},
            [this](CCMenuItemSpriteExtra*) { this->onImport(); }
        );
        if (!current.empty()) {
            this->addButton(
                ButtonSprite::create("Normal Icon", "goldFont.fnt", "GJ_button_04.png", .55f), Anchor::Bottom, {20.f, 22.f},
                [this](CCMenuItemSpriteExtra*) {
                    skins::assignPhoto(m_mode, "");
                    this->changed();
                }
            );
        }
        if (!photos.empty()) {
            this->addButton(
                ButtonSprite::create(
                    m_deleteMode ? "Done" : "Delete", "goldFont.fnt", m_deleteMode ? "GJ_button_06.png" : "GJ_button_04.png", .55f
                ),
                Anchor::Bottom, {140.f, 22.f},
                [this](CCMenuItemSpriteExtra*) {
                    m_deleteMode = !m_deleteMode;
                    this->refreshSoon();
                }
            );
        }
    }

    CCNode* photoButton(std::string const& id, bool selected) {
        // an empty sprite is the container: menu items expect a node that has colour and opacity
        auto cell = CCSprite::create();
        cell->setContentSize({54.f, 54.f});

        auto frame = CCScale9Sprite::create("square02b_001.png", {0.f, 0.f, 80.f, 80.f});
        frame->setContentSize({54.f, 54.f});
        frame->setColor(
            m_deleteMode ? ccColor3B{255, 60, 60} : selected ? ccColor3B{90, 255, 90} : ccColor3B{0, 0, 0}
        );
        frame->setOpacity(selected || m_deleteMode ? 150 : 70);
        frame->setPosition({27.f, 27.f});
        cell->addChild(frame);

        if (auto tex = skins::photoTexture(id)) {
            auto spr = CCSprite::createWithTexture(tex);
            spr->setScale(44.f / spr->getContentSize().width);
            spr->setPosition({27.f, 27.f});
            cell->addChild(spr);
        }

        auto name = CCLabelBMFont::create(id.c_str(), "chatFont.fnt");
        name->limitLabelWidth(60.f, .5f, .2f);
        name->setPosition({27.f, -7.f});
        cell->addChild(name);

        return CCMenuItemExt::createSpriteExtra(cell, [this, id](CCMenuItemSpriteExtra*) { this->onPhoto(id); });
    }

    void onPhoto(std::string const& id) {
        if (!m_deleteMode) {
            skins::assignPhoto(m_mode, id);
            this->changed();
            return;
        }
        Ref<PhotoIconsPopup> self = this;
        createQuickPopup(
            "Delete photo",
            fmt::format("Delete <cy>{}</c>? Gamemodes using it go back to your normal icon.", id),
            "Cancel", "Delete",
            [self, id](FLAlertLayer*, bool yes) {
                if (!yes) return;
                skins::deletePhoto(id);
                self->changed();
            }
        );
    }

    void onImport() {
        if (m_pick.isPending()) return;
        file::FilePickOptions options;
        options.filters.push_back({"Images", {"*.png", "*.jpg", "*.jpeg", "*.bmp", "*.gif"}});
        m_pick.spawn(file::pick(file::PickMode::OpenFile, std::move(options)), [this](file::PickResult result) {
            if (result.isErr()) {
                Notification::create(
                    fmt::format("Couldn't open the file picker: {}", result.unwrapErr()), NotificationIcon::Error
                )->show();
                return;
            }
            auto path = std::move(result).unwrap();
            if (!path) return;  // cancelled

            auto image = skins::decodeImage(*path);
            if (image.isErr()) {
                FLAlertLayer::create("Can't use this photo", image.unwrapErr(), "OK")->show();
                return;
            }
            Ref<PhotoIconsPopup> self = this;
            auto editor = PhotoEditorPopup::create(
                std::move(image).unwrap(), m_mode, utils::string::pathToString(path->stem()), [self] { self->changed(); }
            );
            if (editor) editor->show();
        });
    }

    // a photo or assignment changed: redraw the icon kit preview and this list
    void changed() {
        if (m_garagePlayer) skins::refreshPhoto(m_garagePlayer);
        this->refreshSoon();
    }

    // rebuilding removes the button that was just tapped, so wait until its tap has finished
    void refreshSoon() {
        this->scheduleOnce(schedule_selector(PhotoIconsPopup::onRefresh), 0.f);
    }

    void onRefresh(float) {
        this->refresh();
    }

public:
    static PhotoIconsPopup* create(SimplePlayer* garagePlayer, IconType mode) {
        auto ret = new PhotoIconsPopup();
        if (ret->init(garagePlayer, mode)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(PhotoGarageLayer, GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init()) return false;
        skins::showPhotosOn(m_playerObject);

        auto spr = ButtonSprite::create("Photos", "goldFont.fnt", "GJ_button_01.png", .65f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(PhotoGarageLayer::onPhotoIcons));
        btn->setID("photo-icons-button"_spr);
        // our own menu, so the button stays put whatever other mods do to the icon kit's menus
        auto menu = CCMenu::create();
        menu->setID("photo-icons-menu"_spr);
        menu->addChild(btn);
        menu->setPosition({spr->getScaledContentSize().width / 2.f + 8.f, 22.f});
        this->addChild(menu, 10);
        return true;
    }

    void onPhotoIcons(CCObject*) {
        if (auto popup = PhotoIconsPopup::create(m_playerObject, m_iconType)) popup->show();
    }
};
