#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nect {
inline constexpr std::size_t raster_source_limit=8*1024*1024;
inline constexpr std::size_t raster_png_limit=16*1024*1024;
inline constexpr std::uint32_t raster_dimension_limit=8192;
inline constexpr std::uint64_t raster_pixel_limit=16777216;

// Straight-alpha, oriented sRGB RGBA8; rows are tightly packed, top to bottom.
struct RasterPixels {
    std::uint32_t width=0,height=0;
    std::vector<unsigned char> rgba;
    bool operator==(const RasterPixels&) const=default;
};

class RasterPayload;
using Raster=std::shared_ptr<const RasterPayload>;
Raster make_raster(std::vector<unsigned char> bytes);

// Only the validating memory decoder can construct an accepted payload. No
// derivative pixels, file handles, paths or mutable caches are authored here.
class RasterPayload final {
public:
    RasterPayload(const RasterPayload&)=delete;
    RasterPayload& operator=(const RasterPayload&)=delete;
    const std::vector<unsigned char>& bytes() const noexcept {return bytes_;}
    const std::string& sha256() const noexcept {return sha256_;}
    const std::string& mime() const noexcept {return mime_;}
    std::uint32_t width() const noexcept {return width_;}
    std::uint32_t height() const noexcept {return height_;}
    unsigned orientation() const noexcept {return orientation_;}
    // assumed_srgb, declared_srgb, or embedded_icc_to_srgb.
    const std::string& color_interpretation() const noexcept {return color_;}
    unsigned interpretation_version() const noexcept {return 1;}
    bool operator==(const RasterPayload&) const;
private:
    friend Raster make_raster(std::vector<unsigned char>);
    RasterPayload(std::vector<unsigned char>,std::string hash,std::string mime,
        std::uint32_t width,std::uint32_t height,unsigned orientation,std::string color);
    std::vector<unsigned char> bytes_;
    std::string sha256_,mime_;
    std::uint32_t width_,height_;
    unsigned orientation_;
    std::string color_;
};

RasterPixels decode_raster(const RasterPayload&);
// Lossless normalized PNG without source metadata, for deterministic display
// semantics at the SVG boundary. This does not replace the authored bytes.
std::vector<unsigned char> encode_raster_png(const RasterPixels&);
}
