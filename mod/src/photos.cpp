#include "photos.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

using namespace geode::prelude;

namespace skins {
    namespace {
        struct ModeInfo {
            IconType type;
            char const* name;
            char const* key;
            char const* frame;
        };

        constexpr ModeInfo MODES[] = {
            {IconType::Cube, "Cube", "cube", "player_{:02}_001.png"},
            {IconType::Ship, "Ship", "ship", "ship_{:02}_001.png"},
            {IconType::Ball, "Ball", "ball", "player_ball_{:02}_001.png"},
            {IconType::Ufo, "UFO", "ufo", "bird_{:02}_001.png"},
            {IconType::Wave, "Wave", "wave", "dart_{:02}_001.png"},
            {IconType::Swing, "Swing", "swing", "swing_{:02}_001.png"},
            {IconType::Jetpack, "Jetpack", "jetpack", "jetpack_{:02}_001.png"},
        };

        ModeInfo const* infoFor(IconType type) {
            for (auto& mode : MODES) {
                if (mode.type == type) return &mode;
            }
            return nullptr;
        }

        std::string savedKey(ModeInfo const& mode) {
            return fmt::format("photo-{}", mode.key);
        }

        // Loaded photo textures; a null entry means the file couldn't be read.
        std::unordered_map<std::string, Ref<CCTexture2D>> g_textures;
    }

    std::vector<IconType> const& photoModes() {
        static std::vector<IconType> const modes = [] {
            std::vector<IconType> out;
            for (auto& mode : MODES) out.push_back(mode.type);
            return out;
        }();
        return modes;
    }

    char const* modeName(IconType mode) {
        auto info = infoFor(mode);
        return info ? info->name : "?";
    }

    std::string mainFrameName(IconType mode, int id) {
        auto info = infoFor(mode);
        return info ? fmt::format(fmt::runtime(info->frame), id) : std::string();
    }

    std::filesystem::path photoDir() {
        auto dir = Mod::get()->getSaveDir() / "photo-icons";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    std::vector<std::string> listPhotos() {
        std::vector<std::pair<std::filesystem::file_time_type, std::string>> found;
        std::error_code ec;
        for (auto const& entry : std::filesystem::directory_iterator(photoDir(), ec)) {
            if (!entry.is_regular_file(ec) || entry.path().extension() != ".png") continue;
            found.emplace_back(entry.last_write_time(ec), entry.path().stem().string());
        }
        std::sort(found.begin(), found.end(), [](auto const& a, auto const& b) { return a.first > b.first; });

        std::vector<std::string> ids;
        for (auto& [time, id] : found) ids.push_back(std::move(id));
        return ids;
    }

    Result<std::string> addPhoto(Pixels const& icon, std::string const& name) {
        // keep file names simple so they work on every platform
        std::string clean;
        for (char c : name) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_') clean += c;
        }
        while (!clean.empty() && clean.back() == ' ') clean.pop_back();
        while (!clean.empty() && clean.front() == ' ') clean.erase(clean.begin());
        if (clean.size() > 24) clean.resize(24);
        if (clean.empty()) clean = "Photo";

        auto dir = photoDir();
        std::string id = clean;
        std::error_code ec;
        for (int i = 2; std::filesystem::exists(dir / (id + ".png"), ec); ++i) {
            id = fmt::format("{} {}", clean, i);
        }

        auto saved = savePng(icon, dir / (id + ".png"));
        if (saved.isErr()) return Err(saved.unwrapErr());
        g_textures.erase(id);
        return Ok(id);
    }

    void deletePhoto(std::string const& id) {
        std::error_code ec;
        std::filesystem::remove(photoDir() / (id + ".png"), ec);
        g_textures.erase(id);
        for (auto& mode : MODES) {
            if (assignedPhoto(mode.type) == id) assignPhoto(mode.type, "");
        }
    }

    std::string assignedPhoto(IconType mode) {
        auto info = infoFor(mode);
        if (!info) return "";
        return Mod::get()->getSavedValue<std::string>(savedKey(*info), "");
    }

    void assignPhoto(IconType mode, std::string const& id) {
        auto info = infoFor(mode);
        if (!info) return;
        Mod::get()->setSavedValue<std::string>(savedKey(*info), id);
        saveNow();
    }

    void saveNow() {
        if (auto saved = Mod::get()->saveData(); saved.isErr()) {
            log::warn("Couldn't save Skin Studio choices: {}", saved.unwrapErr());
        }
    }

    std::string activePhoto(IconType mode) {
        if (!Mod::get()->getSettingValue<bool>("show-photos")) return "";
        return assignedPhoto(mode);
    }

    CCTexture2D* photoTexture(std::string const& id) {
        if (id.empty()) return nullptr;
        if (auto it = g_textures.find(id); it != g_textures.end()) return it->second;

        CCTexture2D* tex = nullptr;
        auto img = decodeImage(photoDir() / (id + ".png"), ICON_SIZE * 2);
        if (img.isOk()) {
            tex = PixelTexture::create(std::move(img).unwrap());
        }
        else {
            log::warn("Photo icon \"{}\" couldn't be loaded: {}", id, img.unwrapErr());
        }
        g_textures[id] = tex;
        return tex;
    }
}
