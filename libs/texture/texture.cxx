#include "texture.h"

#include "../resource/resource.h"

#include <glbinding/glbinding.h>
#include <glbinding/gl33core/gl.h>

using namespace gl;

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_resize.h>
#include <stb_image_write.h>
#include <rlottie_capi.h>

tl::expected<sc::texture::frame, std::string> sc::texture::load_from_memory(const std::vector<std::byte> &data) {
    int image_width, image_height;
    unsigned char *image_data = stbi_load_from_memory(reinterpret_cast<const stbi_uc *>(data.data()), data.size(), &image_width, &image_height, 0, STBI_rgb_alpha);
    if (image_data == 0) return tl::make_unexpected("Unrecognized image format or corrupt data.");
    std::vector<std::byte> rgba;
    rgba.resize(image_width * image_height * 4);
    memcpy(rgba.data(), image_data, rgba.size());
    stbi_image_free(image_data);
    frame res = { { image_width, image_height }, std::move(rgba) };
    return std::move(res);
}

tl::expected<sc::texture::frame_sequence, std::string> sc::texture::load_lottie_from_memory(const std::string_view &cache_key, const std::vector<std::byte> &data, const glm::ivec2 &size) {
    const auto data_str = std::string(reinterpret_cast<const char *>(data.data()), data.size());
    if (!nlohmann::json::accept(data_str)) return tl::make_unexpected("Unable to parse JSON data.");
    auto animation = lottie_animation_from_data(data_str.c_str(), cache_key.data(), "");
    if (!animation) return tl::make_unexpected("Unable to process file.");
    const auto frame_rate = lottie_animation_get_framerate(animation);
    const auto num_frames = lottie_animation_get_totalframe(animation);
    const auto duration = lottie_animation_get_duration(animation);
    std::vector<frame> frames;
    frames.reserve(num_frames);
    for (int i = 0; i < num_frames; i++) {
        std::vector<std::byte> rgba(size.x * size.y * 4);
        lottie_animation_render(animation, i, reinterpret_cast<uint32_t *>(rgba.data()), size.x, size.y, size.x * 4);
        frames.push_back({
            size,
            rgba
        });
    }
    lottie_animation_destroy(animation);
    const frame_sequence res {
        frame_rate,
        frames
    };
    return res;
}

tl::expected<sc::texture::frame, std::string> sc::texture::resize(const frame &reference, const glm::ivec2 &new_size) {
    std::vector<std::byte> scaled_rgba;
    scaled_rgba.resize(new_size.x * new_size.y * 4);
    auto status = stbir_resize_uint8(
        reinterpret_cast<const unsigned char *>(reference.content.data()),
        reference.size.x,
        reference.size.y,
        0,
        reinterpret_cast<unsigned char *>(scaled_rgba.data()),
        new_size.x,
        new_size.y,
        0, 4
    );
    if (status != 1) return tl::make_unexpected("The resizing operation failed.");
    frame res = { new_size, std::move(scaled_rgba) };
    return res;
}

std::optional<size_t> sc::texture::frame_sequence::plot_frame_index(const double &frame_rate, const size_t &num_frames, double &seconds, const bool &wrap) {
    if (!num_frames) return std::nullopt;
    const auto duration = (1.0 / frame_rate) * num_frames;
    if (!wrap && seconds >= duration) {
        seconds = duration;
        return num_frames - 1;
    }
    while (seconds >= duration) seconds -= duration;
    return static_cast<size_t>(((1.0 / duration) * seconds) * num_frames);
}

sc::texture::gpu_handle::gpu_handle(const uint32_t &handle, const glm::ivec2 &size, const std::optional<std::string> &description) : handle(handle), size(size), description(description) {
    spdlog::debug("Uploaded to GPU: {} ({}x{}, #{})", description ? *description : "<?>", size.x, size.y, handle);
}

sc::texture::gpu_handle::~gpu_handle() {
    glDeleteTextures(1, &handle);
    spdlog::debug("Deleted from GPU: {} ({}x{}, #{})", description ? *description : "<?>", size.x, size.y, handle);
}

tl::expected<std::shared_ptr<sc::texture::gpu_handle>, std::string> sc::texture::upload_to_gpu(const frame &reference, const glm::ivec2 &size, const std::optional<std::string> &description) {
    GLuint texture;
    glGenTextures(1, &texture);
    if (texture <= 0) return tl::make_unexpected("Unable to allocate a texture ID on the GPU.");
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (size != reference.size) {
        auto scaled_image = resize(reference, size);
        if (scaled_image.has_value()) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_image->size.x, scaled_image->size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled_image->content.data());
            return std::make_shared<gpu_handle>(texture, size, description);
        } else spdlog::warn("Unable to resize image data for texture: #{}", texture);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, reference.size.x, reference.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, reference.content.data());
    return std::make_shared<gpu_handle>(texture, size, description);
}