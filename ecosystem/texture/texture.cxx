#include "texture.h"

#include <glbinding/glbinding.h>
#include <glbinding/gl33core/gl.h>

using namespace gl;

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stb_image_resize.h>
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
    try {
        const auto doc = nlohmann::json::parse(std::string_view(reinterpret_cast<const char *>(data.data()), data.size()));
        const auto str = doc.dump();
        auto animation = lottie_animation_from_data(str.data(), cache_key.data(), "");
        if (!animation) return tl::make_unexpected("Unable to recognize data as Lottie format, or corrupt data.");
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
                std::move(rgba)
            });
        }
        lottie_animation_destroy(animation);
        const frame_sequence res {
            frame_rate,
            duration,
            std::move(frames)
        };
        return std::move(res);
    } catch (const nlohmann::json::exception &err) {
        return tl::make_unexpected("Unable to parse file as JSON format.");
    }
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

sc::texture::gpu_handle::gpu_handle(const uint32_t &handle, const glm::ivec2 &size) : handle(handle), size(size) {
    // pass
}

sc::texture::gpu_handle::~gpu_handle() {
    glDeleteTextures(1, &handle);
    spdlog::debug("Destroyed texture: #{}", handle);
}

tl::expected<std::shared_ptr<sc::texture::gpu_handle>, std::string> sc::texture::upload_to_gpu(const frame &reference, const glm::ivec2 &size) {
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
            spdlog::debug("Made new texture: #{} ({}x{} -> {}x{})", texture, reference.size.x, reference.size.y, scaled_image->size.x, scaled_image->size.y);
            return std::make_shared<gpu_handle>(texture, size);
        } else spdlog::warn("Unable to resize image data for texture: #{}", texture);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, reference.size.x, reference.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, reference.content.data());
    spdlog::debug("Made new texture: #{} ({} by {})", texture, size.x, size.y);
    return std::make_shared<gpu_handle>(texture, size);
}