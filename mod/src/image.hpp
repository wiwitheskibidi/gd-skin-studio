#pragma once

// Turning a photo into a square icon: decode, crop/zoom/rotate, mask, outline, save as PNG.

#include <Geode/Geode.hpp>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace skins {
    // RGBA, straight alpha, top row first.
    struct Pixels {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> rgba;
    };

    enum class Shape { Square = 0, Rounded = 1, Circle = 2 };

    struct IconEdit {
        int rotation = 0;   // quarter turns clockwise
        bool flip = false;  // mirror left/right
        float zoom = 1.f;   // 1 = the largest square that fits the photo
        float panX = 0.f;   // -1..1, left to right across the room left by zooming
        float panY = 0.f;   // -1..1, bottom to top
        Shape shape = Shape::Rounded;
        bool outline = true;
    };

    // Size of a saved icon in pixels (GD's high-quality icons are about this big).
    constexpr int ICON_SIZE = 128;

    // Reads a PNG, JPEG, BMP or GIF and shrinks it to at most `maxSide` pixels on its long side.
    geode::Result<Pixels> decodeImage(std::filesystem::path const& path, int maxSide = 768);
    Pixels renderIcon(Pixels const& src, IconEdit const& edit, int size = ICON_SIZE);
    geode::Result<> savePng(Pixels const& img, std::filesystem::path const& path);

    // A texture built from our own pixels. On Android the engine rebuilds textures from this
    // pixel pointer after the GL context is lost, so the pixels live exactly as long as the texture.
    class PixelTexture : public cocos2d::CCTexture2D {
        std::vector<uint8_t> m_pixels;

    public:
        static PixelTexture* create(Pixels px);
    };
}
