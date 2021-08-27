#include "vjoy-pub-2-1-9-1.h"

#define	VJOY_HWID_TMPLT TEXT("root\\VID_%04X&PID_%04X&REV_%04X")

#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include <spdlog/spdlog.h>

#define DIRECTINPUT_VERSION 0x0800

#include <tchar.h>
#include <dinput.h>
#include <newdev.h>

#include "hid.h"

#include <stdio.h>

#include <array>
#include <memory>
#include <set>
#include <map>
#include <codecvt>
#include <filesystem>

#include <windows.h>
#include <hidsdi.h>
#include <cfgmgr32.h>
#include <devguid.h>
#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>

#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

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

typedef BOOL (WINAPI *UpdateDriverForPlugAndPlayDevicesProto)(_In_opt_ HWND hwndParent, __in LPCTSTR HardwareId, __in LPCTSTR FullInfPath, __in DWORD InstallFlags, __out_opt PBOOL bRebootRequired);

#undef min
#undef max

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

    static bool di8_supports_xinput(const GUID* guid) {
        UINT i, count = 0;
        bool result = false;
        if (GetRawInputDeviceList(NULL, &count, sizeof(RAWINPUTDEVICELIST)) != 0) return false;
        std::vector<RAWINPUTDEVICELIST> ridls(count);
        if (GetRawInputDeviceList(ridls.data(), &count, sizeof(RAWINPUTDEVICELIST)) == (UINT) -1) return false;
        ridls.resize(count);
        for (i = 0;  i < count;  i++) {
            RID_DEVICE_INFO rdi;
            char name[256];
            UINT size;
            if (ridls[i].dwType != RIM_TYPEHID) continue;
            ZeroMemory(&rdi, sizeof(rdi));
            rdi.cbSize = sizeof(rdi);
            size = sizeof(rdi);
            if ((INT) GetRawInputDeviceInfoA(ridls[i].hDevice, RIDI_DEVICEINFO, &rdi, &size) == -1) continue;
            if (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) != (LONG) guid->Data1) continue;
            memset(name, 0, sizeof(name));
            size = sizeof(name);
            if ((INT) GetRawInputDeviceInfoA(ridls[i].hDevice, RIDI_DEVICENAME, name, &size) == -1) break;
            name[sizeof(name) - 1] = '\0';
            if (strstr(name, "IG_")) {
                result = true;
                break;
            }
        }
        return result;
    }

    static IDirectInput8 *direct_input_8_context = nullptr;

    static DIOBJECTDATAFORMAT direct_input_8_object_data_formats[] = {
        { &GUID_XAxis, DIJOFS_X, DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_YAxis, DIJOFS_Y, DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_ZAxis, DIJOFS_Z, DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_RxAxis, DIJOFS_RX, DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_RyAxis, DIJOFS_RY, DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_RzAxis, DIJOFS_RZ, DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_Slider, DIJOFS_SLIDER(0), DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_Slider, DIJOFS_SLIDER(1), DIDFT_AXIS | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, DIDOI_ASPECTPOSITION },
        { &GUID_POV, DIJOFS_POV(0), DIDFT_POV| DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { &GUID_POV, DIJOFS_POV(1), DIDFT_POV| DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { &GUID_POV, DIJOFS_POV(2), DIDFT_POV| DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { &GUID_POV, DIJOFS_POV(3), DIDFT_POV| DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(0), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(1), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(2), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(3), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(4), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(5), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(6), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(7), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(8), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(9), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(10), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(11), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(12), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(13), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(14), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(15), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(16), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(17), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(18), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(19), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(20), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(21), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(22), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(23), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(24), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(25), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(26), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(27), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(28), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(29), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(30), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
        { NULL, DIJOFS_BUTTON(31), DIDFT_BUTTON | DIDFT_OPTIONAL | DIDFT_ANYINSTANCE, 0 },
    };

    static const DIDATAFORMAT direct_input_8_data_format = {
        sizeof(DIDATAFORMAT),
        sizeof(DIOBJECTDATAFORMAT),
        DIDFT_ABSAXIS,
        sizeof(DIJOYSTATE),
        sizeof(direct_input_8_object_data_formats) / sizeof(DIOBJECTDATAFORMAT),
        direct_input_8_object_data_formats
    };

    enum class di8_object_type {
        SLIDER,
        AXIS,
        BUTTON,
        POV
    };

    struct di8_object {
        IDirectInputDevice8 *device = nullptr;
        int num_sliders = 0, num_buttons = 0, num_povs = 0, num_axes = 0;
    };

    static BOOL CALLBACK di8_enum_device_objects(const DIDEVICEOBJECTINSTANCE *doi, void *user) {
        auto object = reinterpret_cast<di8_object *>(user);
        std::optional<di8_object_type> obj_type;
        std::optional<int> obj_offset;
        if (DIDFT_GETTYPE(doi->dwType) & DIDFT_AXIS) {
            DIPROPRANGE dipr;
            if (memcmp(&doi->guidType, &GUID_Slider, sizeof(GUID)) == 0) obj_offset = DIJOFS_SLIDER(object->num_sliders);
            else if (memcmp(&doi->guidType, &GUID_XAxis, sizeof(GUID)) == 0) obj_offset = DIJOFS_X;
            else if (memcmp(&doi->guidType, &GUID_YAxis, sizeof(GUID)) == 0) obj_offset = DIJOFS_Y;
            else if (memcmp(&doi->guidType, &GUID_ZAxis, sizeof(GUID)) == 0) obj_offset = DIJOFS_Z;
            else if (memcmp(&doi->guidType, &GUID_RxAxis, sizeof(GUID)) == 0) obj_offset = DIJOFS_RX;
            else if (memcmp(&doi->guidType, &GUID_RyAxis, sizeof(GUID)) == 0) obj_offset = DIJOFS_RY;
            else if (memcmp(&doi->guidType, &GUID_RzAxis, sizeof(GUID)) == 0) obj_offset = DIJOFS_RZ;
            else return DIENUM_CONTINUE;
            ZeroMemory(&dipr, sizeof(dipr));
            dipr.diph.dwSize = sizeof(dipr);
            dipr.diph.dwHeaderSize = sizeof(dipr.diph);
            dipr.diph.dwObj = doi->dwType;
            dipr.diph.dwHow = DIPH_BYID;
            dipr.lMin = std::numeric_limits<short>::min();
            dipr.lMax =  std::numeric_limits<short>::max();
            if (FAILED(IDirectInputDevice8_SetProperty(object->device, DIPROP_RANGE, &dipr.diph))) return DIENUM_CONTINUE;
            if (memcmp(&doi->guidType, &GUID_Slider, sizeof(GUID)) == 0) {
                obj_type = di8_object_type::SLIDER;
                object->num_sliders++;
            } else {
                obj_type = di8_object_type::AXIS;
                object->num_axes++;
            }
        } else if (DIDFT_GETTYPE(doi->dwType) & DIDFT_BUTTON) {
            obj_offset = DIJOFS_BUTTON(object->num_buttons);
            obj_type = di8_object_type::BUTTON;
            object->num_buttons++;
        } else if (DIDFT_GETTYPE(doi->dwType) & DIDFT_POV) {
            obj_offset = DIJOFS_POV(object->num_povs);
            obj_type = di8_object_type::POV;
            object->num_povs++;
        }
        if (obj_type && obj_offset && *obj_type != di8_object_type::BUTTON) {
            logger()->critical(">> offset: {}, type: {}", *obj_offset, static_cast<int>(*obj_type));
        }
        return DIENUM_CONTINUE;
    }

    static BOOL CALLBACK di8_enum_device_callback(const DIDEVICEINSTANCE *di, void *user) {
        if (auto product_guid = sc::hid::convert_guid_to_string(di->guidProduct); product_guid.has_value() && product_guid->size() == 38) {
            auto guid_str = std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t>().to_bytes(product_guid->data());
            auto pid = guid_str.substr(1, 4);
            auto vid = guid_str.substr(5, 4);
            if (vid_pid_product_map.find({ vid, pid }) == vid_pid_product_map.end()) {
                logger()->debug("New product mapping: VID [{}], PID [{}] -> NAME [{}] (GUID [{}])", vid, pid, di->tszProductName, guid_str);
                vid_pid_product_map[{ vid, pid }] = di->tszProductName;
                /*
                logger()->critical("---");
                if (DIDFT_GETTYPE(di->dwDevType) & DIDFT_AXIS) {
                    logger()->critical("Device is axis: {}", di->tszProductName);
                }
                if (DIDFT_GETTYPE(di->dwDevType) & DIDFT_BUTTON) {
                    logger()->critical("Device is button: {}", di->tszProductName);
                }
                if (DIDFT_GETTYPE(di->dwDevType) & DIDFT_POV) {
                    logger()->critical("Device is hat: {}", di->tszProductName);
                }
                const bool supports_xinput = di8_supports_xinput(&di->guidProduct);
                if (!supports_xinput) {
                    logger()->info("Xinput: {}", supports_xinput);
                    if (IDirectInputDevice8 *device = nullptr; SUCCEEDED(IDirectInput8_CreateDevice(direct_input_8_context, di->guidInstance, &device, nullptr))) {
                        if (SUCCEEDED(IDirectInputDevice8_SetDataFormat(device, &direct_input_8_data_format))) {
                            DIDEVCAPS capabilities;
                            memset(&capabilities, 0, sizeof(DIDEVCAPS));
                            capabilities.dwSize = sizeof(DIDEVCAPS);
                            if (SUCCEEDED(IDirectInputDevice8_GetCapabilities(device, &capabilities))) {
                                DIPROPDWORD dipd;
                                memset(&dipd, 0, sizeof(DIPROPDWORD));
                                dipd.diph.dwSize = sizeof(dipd);
                                dipd.diph.dwHeaderSize = sizeof(dipd.diph);
                                dipd.diph.dwHow = DIPH_DEVICE;
                                dipd.dwData = DIPROPAXISMODE_ABS;
                                if (SUCCEEDED(IDirectInputDevice8_SetProperty(device, DIPROP_AXISMODE, &dipd.diph))) {
                                    logger()->critical("{} axes, {} buttons, {} hats", capabilities.dwAxes, capabilities.dwButtons, capabilities.dwPOVs);
                                    di8_object obj;
                                    obj.device = device;
                                    if (SUCCEEDED(IDirectInputDevice8_EnumObjects(device, di8_enum_device_objects, &obj, DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV))) {
                                        logger()->critical("Got device.");
                                    }
                                }
                            }
                        }
                        device->Release();
                    } else logger()->critical("Failed to create device.");
                } else logger()->critical("Xinput: {}", supports_xinput);
                logger()->critical("--- eod");
                */
            }
        } else logger()->warn("Failed to resolve VID/PID of device: {}", di->tszProductName);
        return DIENUM_CONTINUE;
    }

    bool populate_vid_pid_to_product_name_map() {
        if (SUCCEEDED(DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&direct_input_8_context, NULL))) {
            if (SUCCEEDED(IDirectInput8_EnumDevices(direct_input_8_context, DI8DEVCLASS_ALL, di8_enum_device_callback, NULL, DIEDFL_ALLDEVICES))) {
                logger()->debug("Enumerated DirectInput8 devices.");
                return true;
            } else logger()->warn("Failed to enumerate DirectInput8 devices.");
            direct_input_8_context->Release();
            direct_input_8_context = nullptr;
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

LPTSTR * GetMultiSzIndexArray(__in __drv_aliasesMem LPTSTR MultiSz) {
    LPTSTR scan;
    LPTSTR * array;
    int elements;

    for(scan = MultiSz, elements = 0; scan[0] ;elements++) {
        scan += lstrlen(scan)+1;
    }
    array = new LPTSTR[elements+2];
    if(!array) {
        return NULL;
    }
    array[0] = MultiSz;
    array++;
    if(elements) {
        for(scan = MultiSz, elements = 0; scan[0]; elements++) {
            array[elements] = scan;
            scan += lstrlen(scan)+1;
        }
    }
    array[elements] = NULL;
    return array;
}

LPTSTR * GetDevMultiSz(__in HDEVINFO Devs, __in PSP_DEVINFO_DATA DevInfo, __in DWORD Prop) {
    LPTSTR buffer = NULL;
    DWORD reqSize = 16000;
    DWORD dataType;
    LPTSTR * array;
    DWORD szChars;
	BOOL bRes;


	// Getting the size of required buffer
#if 0
	bRegProp = SetupDiGetDeviceRegistryProperty(Devs, DevInfo, Prop, NULL, NULL, 0, &reqSize);
	DWORD err = GetLastError();
	if (err != ERROR_INSUFFICIENT_BUFFER)
		return NULL;

#endif // 0


	// Allocate buffer according to required size
    buffer = new TCHAR[(reqSize /sizeof(TCHAR))+2];
    if(!buffer)
		return NULL;

	// Get the string into the buffer 
	bRes = SetupDiGetDeviceRegistryProperty(Devs, DevInfo, Prop, &dataType, (LPBYTE)buffer, reqSize, &reqSize);
	if (!bRes || ((dataType != REG_SZ) && (dataType != REG_MULTI_SZ)))
		return NULL;

	szChars = reqSize / sizeof(TCHAR);
	buffer[szChars] = TEXT('\0');
	buffer[szChars + 1] = TEXT('\0');
	array = GetMultiSzIndexArray(buffer);
    if(array)
        return array;

    if(buffer) {
        delete [] buffer;
    }
    return NULL;
}

void DelMultiSz(__in_opt __drv_freesMem(object) PZPWSTR Array) {
        if(Array) {
        Array--;
        if(Array[0]) {
            delete [] Array[0];
        }
        delete [] Array;
    }
}

static std::optional<bool> is_vjoy_installed() {
    TCHAR vjoy_instance_id[MAX_DEVICE_ID_LEN];
    TCHAR device_hwid[MAX_PATH];
    _stprintf_s(device_hwid, MAX_PATH, VJOY_HWID_TMPLT,VENDOR_N_ID, PRODUCT_N_ID, VERSION_N);
    HDEVINFO devs = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (devs != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfo;
        devInfo.cbSize = sizeof(devInfo);
        TCHAR prt[MAX_PATH];
        for (int devIndex=0; SetupDiEnumDeviceInfo(devs,devIndex,&devInfo); devIndex++) {
            LPTSTR *hwIds = NULL;
            if(CM_Get_Device_ID(devInfo.DevInst, vjoy_instance_id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) vjoy_instance_id[0] = TEXT('\0');
            hwIds = GetDevMultiSz(devs,&devInfo,SPDRP_HARDWAREID);
            if (!hwIds || !(*hwIds)) continue;
            int cmp = _tcsnicmp(*hwIds, device_hwid, _tcslen(device_hwid));
            DelMultiSz((PZPWSTR)hwIds);
            if (!cmp) return true;
        }
        return false;
    } 
    return std::nullopt;
}

static std::optional<bool> install_vjoy_device(const std::filesystem::path &inf_file) {
    const auto is_already_installed = is_vjoy_installed();
    if (!is_already_installed || *is_already_installed) return std::nullopt;
    logger()->info("vJoy device isn't present; Installing... ({})", std::filesystem::absolute(inf_file).string());
    GUID class_guid;
    TCHAR class_name[MAX_CLASS_NAME_LEN];
    if (!SetupDiGetINFClass(std::filesystem::absolute(inf_file).string().data(), &class_guid, class_name, sizeof(class_name) / sizeof(class_name[0]), 0)) {
        logger()->error("vJoy Installation: Failed to process INF file.");
        return std::nullopt;
    }
    logger()->info("vJoy Installation, Class Name: {}", class_name);
    HDEVINFO device_info_set = SetupDiCreateDeviceInfoList(&class_guid, NULL);
    if(device_info_set == INVALID_HANDLE_VALUE) {
        logger()->error("vJoy Installation: Failed to get device info list.");
        return std::nullopt;
    }
    SP_DEVINFO_DATA device_info_data;
    memset(&device_info_data, 0, sizeof(SP_DEVINFO_DATA));
    device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
    if (!SetupDiCreateDeviceInfo(device_info_set, class_name, &class_guid, NULL,  0, DICD_GENERATE_ID, &device_info_data)) {
        logger()->error("vJoy Installation: Failed to get device info.");
        return std::nullopt;
    }
    TCHAR *hwIdList = "root\\VID_1234&PID_BEAD&REV_0221";
    if(!SetupDiSetDeviceRegistryProperty(device_info_set, &device_info_data, SPDRP_HARDWAREID,(LPBYTE)hwIdList, (lstrlen(hwIdList)+1+1)*sizeof(TCHAR))) {
        logger()->error("vJoy Installation: Failed to set device registry property.");
        return std::nullopt;
    }
    if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, device_info_set, &device_info_data)) {
        logger()->error("vJoy Installation: Failed to call class installer.");
        return std::nullopt;
    }
    TCHAR instance_id[1000];
    if (!SetupDiGetDeviceInstanceId(device_info_set, &device_info_data, instance_id, 1000, NULL)) {
        logger()->error("vJoy Installation: Failed to get device instance ID.");
        return std::nullopt;
    }
    logger()->info("vJoy Installation, Device Instance ID: {}", instance_id);
    const auto wait_events = CMP_WaitNoPendingInstallEvents(30000);
    logger()->info("vJoy Installation, Wait Events: {}", wait_events);
    BOOL reboot = false;
    const auto newdev_mod = LoadLibrary(TEXT("newdev.dll"));
    logger()->info("vJoy Installation, NEWDEV module: {}", reinterpret_cast<void *>(newdev_mod));
    const auto update_pnp = (UpdateDriverForPlugAndPlayDevicesProto)GetProcAddress(newdev_mod, "UpdateDriverForPlugAndPlayDevicesA");
    logger()->info("vJoy Installation, PNP function: {}", reinterpret_cast<void *>(update_pnp));
    logger()->info("vJoy Installation, HWID: {}", hwIdList);
    const auto pnp_status = update_pnp(NULL, hwIdList, std::filesystem::absolute(inf_file).string().data(), INSTALLFLAG_FORCE, &reboot);
    logger()->info("vJoy Installation, PNP Status: {}", pnp_status);
    logger()->info("Progress good.");
    // SetupDiDestroyDeviceInfoList(DeviceInfoSet);
    return std::nullopt;
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
        logger()->debug("Instance path: {}", instance_path_chars);
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
    if (const auto installed = is_vjoy_installed(); installed) {
        spdlog::info("### VJOY INSTALLED ### : {}", *installed);
    }
    install_vjoy_device("C:\\Users\\Brandon\\Downloads\\vJoy-2.2.1.1\\x64\\Release\\vjoy.inf");
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