#include "serial.h"

#include <locale>
#include <codecvt>

#include <spdlog/spdlog.h>
#include <fmt/format.h>

#include "../winreg.hpp"

tl::expected<std::vector<std::string>, std::string> sc::serial::list_ports() {
    std::vector<std::string> list;
    winreg::RegKey key;
    if (const auto res = key.TryOpen(HKEY_LOCAL_MACHINE, L"HARDWARE\\DEVICEMAP\\SERIALCOMM", KEY_READ); !res) return tl::make_unexpected("Unable to open registry key.");
    for (const auto &value : key.EnumValues()) {
        try {
            const auto comm_port_name = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(key.GetStringValue(value.first));
            list.push_back(fmt::format("\\\\.\\{}", comm_port_name));
        } catch (...) {
            // ...
        }
    }
    return list;
}

std::optional<std::string> sc::serial::comm_instance::open(const std::string_view &port, std::optional<DWORD> baud_rate) {
    if (const auto err = close(); err) return err;
    this->port = port;
    io_handle = CreateFileA(port.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL);
    if (io_handle != INVALID_HANDLE_VALUE) {
        DCB params = { 0 };
        if (GetCommState(io_handle, &params)) {
            params.BaudRate = baud_rate ? *baud_rate : 115200;
            params.ByteSize = 8;
            params.StopBits = ONESTOPBIT;
            params.Parity = NOPARITY;
            params.fDtrControl = DTR_CONTROL_ENABLE;
            if (SetCommState(io_handle, &params)) {
                PurgeComm(io_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
                spdlog::debug("Opened COMM port: {}, {}", port, params.BaudRate);
                connected = true;
                return std::nullopt;
            } else {
                close();
                return fmt::format("Unable to get set serial parameters for COMM port: {}, {}", port, params.BaudRate);
            }
        } else {
            close();
            return fmt::format("Unable to get current serial parameters for COMM port: {}", port);
        }
    } else {
        return fmt::format("Unable to open COMM port: {}", port);
    }
}

std::optional<std::string> sc::serial::comm_instance::close() {
    if (io_handle != INVALID_HANDLE_VALUE) {
        if (!CloseHandle(io_handle)) spdlog::warn("Unable to close COMM port: {}", *port);
        else spdlog::debug("Closed COMM port: {}", *port);
        io_handle = INVALID_HANDLE_VALUE;
    }
    port = std::nullopt;
    connected = false;
    return std::nullopt;
}

tl::expected<std::vector<std::byte>, std::string> sc::serial::comm_instance::read() {
    if (!connected) return tl::make_unexpected("Not connected.");
    if (!ClearCommError(io_handle, &error, &status)) return tl::make_unexpected("Unable to get current status of COMM port.");
    if (status.cbInQue == 0) return { };
    std::vector<std::byte> buffer(status.cbInQue);
    DWORD num_bytes_read;
    if (!ReadFile(io_handle, buffer.data(), buffer.size(), &num_bytes_read, NULL)) return tl::make_unexpected("Unable to read from COMM port.");
    if (num_bytes_read != buffer.size()) buffer.resize(num_bytes_read);
    return buffer;
}

std::optional<std::string> sc::serial::comm_instance::write(const std::vector<std::byte> &input) {
    if (!connected) return "Not connected.";
    DWORD num_bytes_written;
    if (!WriteFile(io_handle, input.data(), input.size(), &num_bytes_written, NULL)) {
        ClearCommError(io_handle, &error, &status);
        return "Unable to write data.";
    }
    if (num_bytes_written != input.size()) return "Unable to write entire packet.";
    return std::nullopt;
}

sc::serial::comm_instance::~comm_instance() {
    close();    
}