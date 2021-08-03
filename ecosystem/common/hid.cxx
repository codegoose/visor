#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <spdlog/spdlog.h>

#define DIRECTINPUT_VERSION 0x0800

#include <dinput.h>

#include "hid.h"

#include <array>
#include <memory>
#include <set>
#include <map>
#include <codecvt>

#include <windows.h>
#include <hidsdi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::CloseHandle)> ptr_closehandle;
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, decltype(&::FindVolumeClose)> ptr_findvolumeclose;
typedef std::unique_ptr<std::remove_pointer<HDEVINFO>::type, decltype(&::SetupDiDestroyDeviceInfoList)> ptr_destroydeviceinfolist;
typedef std::unique_ptr<std::remove_pointer<PHIDP_PREPARSED_DATA>::type, decltype(&::HidD_FreePreparsedData)> ptr_freepreparsedhidpdata;

constexpr auto IoControlDeviceType { 32769u };
constexpr auto IoControlGetBlacklist { CTL_CODE(IoControlDeviceType, 2050, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IoControlGetWhitelist { CTL_CODE(IoControlDeviceType, 2048, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IoControlSetBlacklist { CTL_CODE(IoControlDeviceType, 2051, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IoControlSetWhitelist { CTL_CODE(IoControlDeviceType, 2049, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IoControlGetActive { CTL_CODE(IoControlDeviceType, 2052, METHOD_BUFFERED, FILE_READ_DATA) };
constexpr auto IoControlSetActive { CTL_CODE(IoControlDeviceType, 2053, METHOD_BUFFERED, FILE_READ_DATA) };

static auto logger() {
    return spdlog::default_logger();
}

namespace sc::hid {

    std::optional<ptr_closehandle> get_device_handle() {
        const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        auto device_file_handle = CreateFileW(L"\\\\.\\HidHide", GENERIC_READ, share_mode, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (device_file_handle == INVALID_HANDLE_VALUE) return std::nullopt;
        return ptr_closehandle(device_file_handle, &CloseHandle);
    }

    std::vector<std::wstring> multi_string_to_string_list(std::vector<wchar_t> &multi_string) {
        std::vector<std::wstring> result;
        for (size_t index = 0, start = 0, size = multi_string.size(); index < size; index++) {
            if (multi_string.at(index) == 0) {
                std::wstring const string(&multi_string.at(start), 0, index - start);
                if (!string.empty()) result.emplace_back(string);
                start = index + 1;
            }
        }
        return std::move(result);
    }

    std::vector<std::filesystem::path> string_list_to_path_list(std::vector<std::wstring> &multi_string) {
        std::vector<std::filesystem::path> result;
        for (auto& string : multi_string) result.emplace_back(string);
        return std::move(result);
    }

    std::optional<std::vector<WCHAR>> string_list_to_multi_string(const std::vector<std::wstring> &string_list) {
        std::vector<WCHAR> result;
        for (auto const &value : string_list) {
            auto const old_size { result.size() };
            auto const append_size { value.size() + 1 };
            result.resize(old_size + append_size);
            if (wcsncpy_s(&result.at(old_size), append_size, value.c_str(), append_size) != 0) return std::nullopt;
        }
        result.push_back(L'\0');
        return result;
    }

    std::optional<std::set<std::filesystem::path>> find_volume_mount_points(std::wstring volume_name) {
        std::set<std::filesystem::path> result;
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        DWORD num_bytes_needed;
        if (GetVolumePathNamesForVolumeNameW(volume_name.data(), buffer.data(), static_cast<DWORD>(buffer.size()), &num_bytes_needed) == FALSE && GetLastError() != ERROR_MORE_DATA) return std::nullopt;
        auto list = multi_string_to_string_list(buffer);
        for (auto &i = std::begin(list); std::end(list) != i; i++) result.emplace(*i);
        return std::move(result);
    }

    std::optional<std::filesystem::path> find_volume_mount_point_for_path(std::filesystem::path file) {
        std::vector<WCHAR> volume_name(UNICODE_STRING_MAX_CHARS);
        auto handle = FindFirstVolumeW(volume_name.data(), static_cast<DWORD>(volume_name.size()));
        if (handle == INVALID_HANDLE_VALUE) return std::nullopt;
        auto ptr_find_volume = ptr_findvolumeclose(handle, &FindVolumeClose);
        std::wstring most_specific;
        for (;;) {
            if (auto volume_mount_points = find_volume_mount_points(volume_name.data()); volume_mount_points.has_value()) {
                for (auto &i : volume_mount_points.value()) {
                    auto volume_mount_point = i.native();
                    if (volume_mount_point.compare(0, std::wstring::npos, file.native(), 0, volume_mount_point.size()) == 0) {
                        if (volume_mount_point.size() > most_specific.size()) {
                            most_specific = volume_mount_point;
                        }
                    }
                }
            }
            if (FindNextVolumeW(ptr_find_volume.get(), volume_name.data(), static_cast<DWORD>(volume_name.size())) == FALSE) {
                if (GetLastError() != ERROR_NO_MORE_FILES) return std::nullopt;
                break;
            }
        }
        return most_specific;
    }

    std::optional<std::filesystem::path> get_dos_device_name_for_volume_name(std::wstring volume_name) {
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        if (QueryDosDeviceW(volume_name.substr(4, volume_name.size() - 5).data(), buffer.data(), static_cast<DWORD>(buffer.size())) == 0) return std::nullopt;
        return buffer.data();
    }

    std::optional<std::wstring> get_volume_name_for_volume_mount_point(std::filesystem::path volume_mount_point) {
        std::vector<WCHAR> buffer(UNICODE_STRING_MAX_CHARS);
        if (GetVolumeNameForVolumeMountPointW(volume_mount_point.native().c_str(), buffer.data(), static_cast<DWORD>(buffer.size())) == FALSE) return std::nullopt;
        return buffer.data();
    }

    std::optional<std::filesystem::path> convert_path_to_image_path(std::filesystem::path path) {
        auto volume_mount_point = find_volume_mount_point_for_path(path);
        if (!volume_mount_point.has_value()) return std::nullopt;
        auto volume_name = get_volume_name_for_volume_mount_point(volume_mount_point.value());
        if (!volume_mount_point.has_value()) return std::nullopt;
        auto dos_device_name = get_dos_device_name_for_volume_name(volume_name.value());
        auto name_without_mount_point = std::filesystem::path(path.native().substr(volume_mount_point.value().native().size()));
        return dos_device_name.value() / name_without_mount_point;
    }

    std::optional<std::wstring> convert_guid_to_string(const GUID &guid) {
        std::array<WCHAR, 39> buffer;
        if (StringFromGUID2(guid, buffer.data(), buffer.size()) == 0) return std::nullopt;
        return buffer.data();
    }

    std::optional<std::vector<std::wstring>> get_device_instance_paths_through_filter(const GUID &class_guid) {
        ULONG num_needed;
        const auto class_guid_as_string = convert_guid_to_string(class_guid);
        if (!class_guid_as_string.has_value()) return std::nullopt;
        if (auto res = CM_Get_Device_ID_List_SizeW(&num_needed, class_guid_as_string.value().data(), CM_GETIDLIST_FILTER_CLASS); res != CR_SUCCESS) return std::nullopt;
        std::vector<WCHAR> buffer(num_needed);
        if (auto res = CM_Get_Device_ID_ListW(class_guid_as_string.value().data(), buffer.data(), buffer.size(), CM_GETIDLIST_FILTER_CLASS); res != CR_SUCCESS) return std::nullopt;
        return multi_string_to_string_list(buffer);
    }

    std::optional<std::filesystem::path> determine_symbolic_link(const GUID &device_interface_guid, const std::wstring_view &device_instance_path) {
        auto const handle { ptr_destroydeviceinfolist(SetupDiGetClassDevsW(&device_interface_guid, device_instance_path.data(), nullptr, DIGCF_DEVICEINTERFACE), &SetupDiDestroyDeviceInfoList) };
        if (handle.get() == INVALID_HANDLE_VALUE) return std::nullopt;
        SP_DEVICE_INTERFACE_DATA deviceInterfaceData { };
        deviceInterfaceData.cbSize = sizeof(deviceInterfaceData);
        if (SetupDiEnumDeviceInterfaces(handle.get(), nullptr, &device_interface_guid, 0, &deviceInterfaceData) == FALSE) return std::nullopt;
        DWORD num_bytes_needed;
        if ((SetupDiGetDeviceInterfaceDetailW(handle.get(), &deviceInterfaceData, nullptr, 0, &num_bytes_needed, nullptr) == FALSE) && (ERROR_INSUFFICIENT_BUFFER != GetLastError())) return std::nullopt;
        std::vector<BYTE> buffer(num_bytes_needed);
        auto &device_interface_detail_data { *reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data()) };
        device_interface_detail_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (SetupDiGetDeviceInterfaceDetailW(handle.get(), &deviceInterfaceData, reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(buffer.data()), static_cast<DWORD>(buffer.size()), nullptr, nullptr) == FALSE) return std::nullopt;
        return device_interface_detail_data.DevicePath;
    }

    bool is_device_present(const std::wstring_view &device_instance_path) {
        DEVINST device_instance { };
        if (auto const result{ ::CM_Locate_DevNodeW(&device_instance, const_cast<DEVINSTID_W>(device_instance_path.data()), CM_LOCATE_DEVNODE_NORMAL) }; (CR_NO_SUCH_DEVNODE == result) || (CR_SUCCESS == result)) return (CR_SUCCESS == result);
        else return false;
    }

    std::optional<std::wstring> get_device_description(const std::wstring_view &device_instance_path) {
        DEVINST device_instance { };
        DEVPROPTYPE device_prop_type { };
        ULONG num_bytes_needed { };
        if (auto const result{ ::CM_Locate_DevNodeW(&device_instance, const_cast<DEVINSTID_W>(device_instance_path.data()), CM_LOCATE_DEVNODE_PHANTOM) }; (CR_SUCCESS != result)) return std::nullopt;
        if (auto const result{ ::CM_Get_DevNode_PropertyW(device_instance, &DEVPKEY_Device_DeviceDesc, &device_prop_type, nullptr, &num_bytes_needed, 0) }; (CR_BUFFER_SMALL != result)) return std::nullopt;
        if (DEVPROP_TYPE_STRING != device_prop_type) return std::nullopt;
        std::vector<WCHAR> buffer(num_bytes_needed);
        if (auto const result{ ::CM_Get_DevNode_PropertyW(device_instance, &DEVPKEY_Device_DeviceDesc, &device_prop_type, reinterpret_cast<PBYTE>(buffer.data()), &num_bytes_needed, 0) }; (CR_SUCCESS != result)) return std::nullopt;
        return (buffer.data());
    }

    struct device_model_information {
        std::optional<std::wstring> product, manufacturer, serial;
    };

    std::optional<device_model_information> query_device_model_information(const std::filesystem::path &symbolic_link) {
        auto const device_handle { ptr_closehandle(CreateFileW(symbolic_link.c_str(), GENERIC_READ, (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE), nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr), &CloseHandle) };
        if (device_handle.get() == INVALID_HANDLE_VALUE) return std::nullopt;
        PHIDP_PREPARSED_DATA pre_parsed_data;
        if (HidD_GetPreparsedData(device_handle.get(), &pre_parsed_data) == FALSE) return std::nullopt;
        auto const pre_parsed_data_ptr{ ptr_freepreparsedhidpdata(pre_parsed_data, &::HidD_FreePreparsedData) };
        HIDP_CAPS capabilities;
        if (HidP_GetCaps(pre_parsed_data, &capabilities) != HIDP_STATUS_SUCCESS) return std::nullopt;
        HIDD_ATTRIBUTES attributes;
        if (HidD_GetAttributes(device_handle.get(), &attributes) == FALSE) return std::nullopt;
        std::vector<WCHAR> buffer(127 * sizeof(WCHAR));
        device_model_information res;
        if (HidD_GetProductString(device_handle.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size())) == TRUE) res.product = buffer.data();
        if (HidD_GetManufacturerString(device_handle.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size())) == TRUE) res.manufacturer = buffer.data();
        if (HidD_GetSerialNumberString(device_handle.get(), buffer.data(), static_cast<ULONG>(sizeof(WCHAR) * buffer.size())) == TRUE) res.serial = buffer.data();
        return res;
    }

    static std::map<std::pair<std::string, std::string>, std::string> vid_pid_product_map;

    static BOOL CALLBACK di8_enum_device_callback(const DIDEVICEINSTANCE *di, void *user) {
        if (auto product_guid = sc::hid::convert_guid_to_string(di->guidProduct); product_guid.has_value() && product_guid->size() == 38) {
            auto guid_str = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(product_guid->data());
            auto pid = guid_str.substr(1, 4);
            auto vid = guid_str.substr(5, 4);
            if (vid_pid_product_map.find({ vid, pid }) == vid_pid_product_map.end()) {
                logger()->debug("New product mapping: VID [{}], PID [{}] -> NAME [{}] (GUID [{}])", vid, pid, di->tszProductName, guid_str);
                vid_pid_product_map[{ vid, pid }] = di->tszProductName;
            }
        } else logger()->warn("Failed to resolve VID/PID of device: {}", di->tszProductName);
        return DIENUM_CONTINUE;
    }

    bool populate_vid_pid_to_product_name_map() {
        IDirectInput8 *direct_input_8_context;
        if (SUCCEEDED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&direct_input_8_context, NULL))) {
            if (SUCCEEDED(IDirectInput8_EnumDevices(direct_input_8_context, DI8DEVCLASS_ALL, di8_enum_device_callback, NULL, DIEDFL_ALLDEVICES))) {
                logger()->debug("Enumerated DirectInput8 devices.");
                return true;
            } else logger()->warn("Failed to enumerate DirectInput8 devices.");
            direct_input_8_context->Release();
        } else logger()->warn("Failed to establish DirectInput8 context.");
        return false;
    }
}

std::optional<std::vector<std::string>> sc::hid::get_blacklist() {
    auto handle = get_device_handle();
    if (!handle.has_value()) return std::nullopt;
    DWORD num_bytes_needed { };
    if (DeviceIoControl(handle->get(), IoControlGetBlacklist, nullptr, 0, nullptr, 0, &num_bytes_needed, nullptr) == FALSE) return std::nullopt;
    std::vector<WCHAR> buffer(num_bytes_needed);
    if (DeviceIoControl(handle->get(), IoControlGetBlacklist, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &num_bytes_needed, nullptr) == FALSE) return std::nullopt;
    std::vector<std::string> res;
    for (auto &wstr : multi_string_to_string_list(buffer)) res.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(wstr));
    return res;
}

std::optional<std::vector<std::filesystem::path>> sc::hid::get_whitelist() {
    auto handle = get_device_handle();
    if (!handle.has_value()) return std::nullopt;
    DWORD num_bytes_needed { };
    if (DeviceIoControl(handle->get(), IoControlGetWhitelist, nullptr, 0, nullptr, 0, &num_bytes_needed, nullptr) == FALSE) return std::nullopt;
    std::vector<WCHAR> buffer(num_bytes_needed);
    if (DeviceIoControl(handle->get(), IoControlGetWhitelist, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(WCHAR)), &num_bytes_needed, nullptr) == FALSE) return std::nullopt;
    return string_list_to_path_list(multi_string_to_string_list(buffer));
}

bool sc::hid::set_blacklist(const std::vector<std::string> &new_list) {
    auto handle = get_device_handle();
    if (!handle.has_value()) return false;
    DWORD num_bytes_needed;
    std::vector<std::wstring> wide_string_list;
    for (auto &str : new_list) wide_string_list.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().from_bytes(str));
    auto buffer = string_list_to_multi_string(wide_string_list);
    if (!buffer.has_value()) return false;
    if (DeviceIoControl(handle->get(), IoControlSetBlacklist, buffer->data(), static_cast<DWORD>(buffer->size() * sizeof(WCHAR)), nullptr, 0, &num_bytes_needed, nullptr) == FALSE) return false;
    return true;
}

bool sc::hid::set_whitelist(const std::vector<std::filesystem::path> &new_list) {
    auto handle = get_device_handle();
    if (!handle.has_value()) return false;
    DWORD num_bytes_needed;
    std::vector<std::wstring> wide_string_list;
    for (auto &path : new_list) {
        auto image = convert_path_to_image_path(path);
        if (!image.has_value()) return false;
        wide_string_list.push_back(std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().from_bytes(image->string()));
    }
    auto buffer = string_list_to_multi_string(wide_string_list);
    if (!buffer.has_value()) return false;
    if (DeviceIoControl(handle->get(), IoControlSetWhitelist, buffer->data(), static_cast<DWORD>(buffer->size() * sizeof(WCHAR)), nullptr, 0, &num_bytes_needed, nullptr) == FALSE) return false;
    return true;
}

std::optional<std::vector<sc::hid::system_hid>> sc::hid::list_devices() {
    if (!populate_vid_pid_to_product_name_map()) return std::nullopt;
    GUID device_interface_guid { };
    HidD_GetHidGuid(&device_interface_guid);
    auto instances = get_device_instance_paths_through_filter(GUID_DEVCLASS_HIDCLASS);
    if (!instances.has_value()) return std::nullopt;
    std::vector<sc::hid::system_hid> res;
    for (auto &device_instance_path : instances.value()) {
        auto instance_path_chars = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(device_instance_path);
        auto vid_i = instance_path_chars.find("\\VID_");
        if (vid_i == std::string::npos) {
            logger()->debug("Unable to find vendor ID within instance path: {}", instance_path_chars);
            continue;
        }
        auto pid_i = instance_path_chars.find("&PID_");
        if (pid_i == std::string::npos) {
            logger()->debug("Unable to find product ID within instance path: {}", instance_path_chars);
            continue;
        }
        auto vid = instance_path_chars.substr(vid_i + 5, 4);
        auto pid = instance_path_chars.substr(pid_i + 5, 4);
        auto product_name_i = vid_pid_product_map.find({ vid, pid });
        if (product_name_i == vid_pid_product_map.end()) {
            logger()->debug("Unable to map VID/PID to product name: {}/{}", vid, pid);
            continue;
        } else logger()->debug("Mapped VID/PID to product name: {}/{} -> [{}]", vid, pid, product_name_i->second);
        res.push_back({ instance_path_chars, product_name_i->second });
    }
    return res;
}

bool sc::hid::present() {
    return get_device_handle().has_value();
}

bool sc::hid::is_enabled() {
    auto handle = get_device_handle();
    if (!handle.has_value()) return false;
    DWORD num_bytes_needed;
    auto buffer { std::vector<BOOLEAN>(1) };
    if (DeviceIoControl(handle->get(), IoControlGetActive, nullptr, 0, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), &num_bytes_needed, nullptr) == FALSE) {
        logger()->error("Unable to get enable state due to IO problem.");
        return false;
    }
    if (sizeof(BOOLEAN) != num_bytes_needed) {
        logger()->error("Unable to get enable state due to IO problem.");
        return false;
    }
    return buffer.at(0) != FALSE;
}

bool sc::hid::set_enabled(bool enabled) {
    auto handle = get_device_handle();
    if (!handle.has_value()) return false;
    DWORD num_bytes_needed;
    auto buffer { std::vector<BOOLEAN>(1) };
    buffer.at(0) = enabled ? TRUE : FALSE;
    if (DeviceIoControl(handle->get(), IoControlSetActive, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(BOOLEAN)), nullptr, 0, &num_bytes_needed, nullptr) == FALSE) {
        logger()->debug("Unable to set enable state due to IO problem.");
        return false;
    }
    return true;
}