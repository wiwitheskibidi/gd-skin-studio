#include "image.hpp"

#include <algorithm>
#include <cmath>

// stb is compiled into this file only, with every function static so nothing clashes with
// another mod that bundles its own copy.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include <stb_image.h>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include <stb_image_write.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

using namespace geode::prelude;

namespace skins {
    namespace {
        // Box-filter shrink so the long side is at most maxSide; colours are weighted by alpha.
        Pixels shrink(uint8_t const* src, int w, int h, int maxSide) {
            int longest = std::max(w, h);
            if (longest <= maxSide) {
                return Pixels{w, h, std::vector<uint8_t>(src, src + size_t(w) * h * 4)};
            }
            double scale = double(maxSide) / longest;
            int nw = std::max(1, int(std::lround(w * scale)));
            int nh = std::max(1, int(std::lround(h * scale)));
            Pixels out{nw, nh, std::vector<uint8_t>(size_t(nw) * nh * 4)};

            for (int y = 0; y < nh; ++y) {
                int y0 = int(int64_t(y) * h / nh);
                int y1 = std::max(y0 + 1, int(int64_t(y + 1) * h / nh));
                for (int x = 0; x < nw; ++x) {
                    int x0 = int(int64_t(x) * w / nw);
                    int x1 = std::max(x0 + 1, int(int64_t(x + 1) * w / nw));
                    uint64_t r = 0, g = 0, b = 0, a = 0, n = 0;
                    for (int sy = y0; sy < y1; ++sy) {
                        auto p = src + (size_t(sy) * w + x0) * 4;
                        for (int sx = x0; sx < x1; ++sx, p += 4) {
                            r += p[0] * p[3];
                            g += p[1] * p[3];
                            b += p[2] * p[3];
                            a += p[3];
                            ++n;
                        }
                    }
                    auto dst = &out.rgba[(size_t(y) * nw + x) * 4];
                    if (a > 0) {
                        dst[0] = uint8_t(r / a);
                        dst[1] = uint8_t(g / a);
                        dst[2] = uint8_t(b / a);
                    }
                    dst[3] = uint8_t(a / n);
                }
            }
            return out;
        }

        // Premultiplied running total of samples.
        struct Sample {
            float r = 0.f, g = 0.f, b = 0.f, a = 0.f;
        };

        // Bilinear sample at a point in pixel units (pixel centres at i + 0.5), clamped to the edges.
        void addSample(Pixels const& src, float x, float y, Sample& acc) {
            float fx = std::clamp(x - 0.5f, 0.f, float(src.width - 1));
            float fy = std::clamp(y - 0.5f, 0.f, float(src.height - 1));
            int x0 = int(fx), y0 = int(fy);
            int x1 = std::min(x0 + 1, src.width - 1);
            int y1 = std::min(y0 + 1, src.height - 1);
            float tx = fx - x0, ty = fy - y0;

            auto add = [&](int px, int py, float weight) {
                auto p = &src.rgba[(size_t(py) * src.width + px) * 4];
                float a = p[3] / 255.f * weight;
                acc.r += p[0] / 255.f * a;
                acc.g += p[1] / 255.f * a;
                acc.b += p[2] / 255.f * a;
                acc.a += a;
            };
            add(x0, y0, (1.f - tx) * (1.f - ty));
            add(x1, y0, tx * (1.f - ty));
            add(x0, y1, (1.f - tx) * ty);
            add(x1, y1, tx * ty);
        }

        // Distance from a point (relative to the centre) to the edge of the shape, positive inside.
        float insideDistance(Shape shape, float px, float py, float half) {
            switch (shape) {
                case Shape::Circle:
                    return half - std::sqrt(px * px + py * py);
                case Shape::Rounded: {
                    float radius = half * 0.44f;
                    float qx = std::abs(px) - (half - radius);
                    float qy = std::abs(py) - (half - radius);
                    float ox = std::max(qx, 0.f), oy = std::max(qy, 0.f);
                    float outside = std::sqrt(ox * ox + oy * oy);
                    float inside = std::min(std::max(qx, qy), 0.f);
                    return radius - outside - inside;
                }
                default:
                    return half - std::max(std::abs(px), std::abs(py));
            }
        }
    }

    Result<Pixels> decodeImage(std::filesystem::path const& path, int maxSide) {
        auto read = file::readBinary(path);
        if (read.isErr()) {
            return Err(fmt::format("Couldn't open the file: {}", read.unwrapErr()));
        }
        auto bytes = std::move(read).unwrap();
        if (bytes.empty() || bytes.size() > size_t(INT32_MAX)) {
            return Err(std::string("The file is empty or too big"));
        }

        int w = 0, h = 0, channels = 0;
        stbi_uc* data = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 4);
        if (!data) {
            return Err(fmt::format("Can't read this picture ({}). Try a PNG or JPG.", stbi_failure_reason()));
        }
        auto px = shrink(data, w, h, maxSide);
        stbi_image_free(data);
        return Ok(std::move(px));
    }

    Pixels renderIcon(Pixels const& src, IconEdit const& edit, int size) {
        Pixels out{size, size, std::vector<uint8_t>(size_t(size) * size * 4, 0)};
        if (src.width <= 0 || src.height <= 0 || size <= 0) return out;

        int rot = ((edit.rotation % 4) + 4) % 4;
        float W = float(src.width), H = float(src.height);
        // photo size after rotating
        float rw = rot % 2 ? H : W;
        float rh = rot % 2 ? W : H;
        float side = std::min(rw, rh) / std::clamp(edit.zoom, 1.f, 8.f);
        float left = (rw - side) / 2.f * (1.f + std::clamp(edit.panX, -1.f, 1.f));
        float top = (rh - side) / 2.f * (1.f - std::clamp(edit.panY, -1.f, 1.f));

        // enough samples per output pixel to average the photo pixels it covers
        int taps = std::clamp(int(std::ceil(side / size)), 1, 6);
        float half = size / 2.f;
        float border = edit.outline ? size * 0.055f : 0.f;

        for (int y = 0; y < size; ++y) {
            for (int x = 0; x < size; ++x) {
                float d = insideDistance(edit.shape, x + 0.5f - half, y + 0.5f - half, half);
                float cover = std::clamp(d + 0.5f, 0.f, 1.f);
                if (cover <= 0.f) continue;

                Sample s;
                for (int ty = 0; ty < taps; ++ty) {
                    for (int tx = 0; tx < taps; ++tx) {
                        float ox = x + (tx + 0.5f) / taps;
                        if (edit.flip) ox = size - ox;
                        float oy = y + (ty + 0.5f) / taps;
                        // point in the rotated photo, then back to the stored photo
                        float qx = left + ox / size * side;
                        float qy = top + oy / size * side;
                        float sx, sy;
                        switch (rot) {
                            case 1: sx = qy; sy = H - qx; break;
                            case 2: sx = W - qx; sy = H - qy; break;
                            case 3: sx = W - qy; sy = qx; break;
                            default: sx = qx; sy = qy; break;
                        }
                        addSample(src, sx, sy, s);
                    }
                }
                float n = float(taps * taps);
                float r = s.r / n, g = s.g / n, b = s.b / n, a = s.a / n;

                if (border > 0.f) {
                    // black ring just inside the edge, like GD's own icons
                    float t = std::clamp(border - d + 0.5f, 0.f, 1.f);
                    r *= 1.f - t;
                    g *= 1.f - t;
                    b *= 1.f - t;
                    a = a * (1.f - t) + t;
                }
                r *= cover;
                g *= cover;
                b *= cover;
                a *= cover;

                auto dst = &out.rgba[(size_t(y) * size + x) * 4];
                if (a > 0.f) {
                    dst[0] = uint8_t(std::clamp(r / a, 0.f, 1.f) * 255.f + 0.5f);
                    dst[1] = uint8_t(std::clamp(g / a, 0.f, 1.f) * 255.f + 0.5f);
                    dst[2] = uint8_t(std::clamp(b / a, 0.f, 1.f) * 255.f + 0.5f);
                }
                dst[3] = uint8_t(std::clamp(a, 0.f, 1.f) * 255.f + 0.5f);
            }
        }
        return out;
    }

    Result<> savePng(Pixels const& img, std::filesystem::path const& path) {
        std::vector<uint8_t> png;
        auto write = [](void* ctx, void* data, int size) {
            auto out = static_cast<std::vector<uint8_t>*>(ctx);
            auto bytes = static_cast<uint8_t*>(data);
            out->insert(out->end(), bytes, bytes + size);
        };
        if (!stbi_write_png_to_func(write, &png, img.width, img.height, 4, img.rgba.data(), img.width * 4)) {
            return Err(std::string("Couldn't encode the PNG"));
        }
        return file::writeBinary(path, png);
    }

    PixelTexture* PixelTexture::create(Pixels px) {
        if (px.width <= 0 || px.height <= 0 || px.rgba.size() < size_t(px.width) * px.height * 4) {
            return nullptr;
        }
        auto ret = new PixelTexture();
        ret->m_pixels = std::move(px.rgba);
        if (!ret->initWithData(
            ret->m_pixels.data(), kCCTexture2DPixelFormat_RGBA8888,
            unsigned(px.width), unsigned(px.height), CCSize(float(px.width), float(px.height))
        )) {
            delete ret;
            return nullptr;
        }
        // initWithData already sets smooth (linear) filtering
        ret->autorelease();
        return ret;
    }
}
