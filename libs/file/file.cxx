#include "file.h"  // We are including the file.h file we wrote earlier.

#include <fstream>  // This is a library that lets us work with files.

// Now we're defining how the 'load' function works.
tl::expected<std::vector<std::byte>, std::string> sc::file::load(const std::filesystem::path &path) {

    // We're trying to open the file at the 'path' we were given. We open it in binary mode (which means we're reading the raw data, not text), 
    // and we're starting at the end of the file.
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);

    // If we couldn't open the file, we return an error message.
    if (!ifs) return tl::make_unexpected("Unable to open file.");

    // We check where we are in the file (which is the end, because we opened it at the end).
    const auto end = ifs.tellg();

    // We then go back to the beginning of the file.
    ifs.seekg(0, std::ios::beg);

    // Now we find out how big the file is by subtracting our current position (the beginning) from the end.
    const auto size = end - ifs.tellg();

    // If the size is zero, that means we couldn't figure out the file size, so we return an error message.
    if (size == 0) return tl::make_unexpected("Unable to determine file size.");

    // We make a buffer (think of it as a temporary storage area) that is big enough to hold all the data from the file.
    std::vector<std::byte> buffer(size);

    // We try to read the file into our buffer. If we can't, we return an error message.
    if (!ifs.read(reinterpret_cast<char *>(buffer.data()), buffer.size())) return tl::make_unexpected(std::strerror(errno));

    // If we got this far, everything worked! We return the buffer, which now contains all the data from the file.
    return std::move(buffer);
}

// Now we're defining how the 'save' function works.
std::optional<std::string> sc::file::save(const std::filesystem::path &path, const std::vector<std::byte> &data) {

    // We're trying to open the file at the 'path' we were given. We open it in binary mode, and 'trunc' means we delete anything that was in the file before.
    std::ofstream ofs(path, std::ios::binary | std::ios::trunc);

    // If we couldn't open the file, we return an error message.
    if (!ofs) return "Unable to open file.";

    // We try to write our data to the file.
    ofs.write(reinterpret_cast<const char *>(data.data()), data.size());

    // If we couldn't write to the file, we return an error message.
    if (!ofs) return "Unable to write data.";

    // If we got this far, everything worked! We don't return anything, which is what 'nullopt' means.
    return std::nullopt;
}
