// "pragma once" is a way to make sure the computer only reads this file one time
#pragma once

// These are like dictionary books that we're using so we don't have to write everything from scratch.
// For example, cstddef is a dictionary for certain types of sizes and lengths.
#include <cstddef>

// Vector is like a magic box where you can store and organize a lot of the same kind of thing.
#include <vector>

// String is a fancy way of saying text. 
#include <string>

// Filesystem helps us work with folders and files in our computer, just like a digital filing cabinet.
#include <filesystem>

// Optional is like a box that might have a present (a value) in it, or it might be empty.
#include <optional>

// Tl/expected is a tool that tells us if a task was successful or gives us an error message if it failed.
#include <tl/expected.hpp>

// We're creating our own namespace (think of it like a special club) called sc::file
namespace sc::file {

    // This is a function called load. You give it a path (like an address of a file) and it tries to get all the data from that file.
    // If it can get the data, it gives you the data as a bunch of bytes (like tiny pieces of information). 
    // If it can't, it tells you why with a string (text).
    tl::expected<std::vector<std::byte>, std::string> load(const std::filesystem::path &path);  // end of load function declaration

    // This function is called save. You give it a path and some data (those tiny pieces of information), and it tries to save the data to that file.
    // If everything goes okay, it doesn't give you anything back. 
    // But if something goes wrong, it gives you a string (text) that tells you what happened.
    std::optional<std::string> save(const std::filesystem::path &path, const std::vector<std::byte> &data); // end of save function declaration
}  // end of sc::file namespace
