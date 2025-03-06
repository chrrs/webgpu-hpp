#pragma once

#include <webgpu/webgpu.hpp>
#include <webgpu/wgpu.h>

namespace wgpu::platform {

// -- ENUMS --
enum class LogLevel : uint32_t {
    Off = 0,
    Error = 1,
    Warn = 2,
    Info = 3,
    Debug = 4,
    Trace = 5,
};

// -- CALLBACKS --
typedef void (*LogCallback)(LogLevel, StringView, void*);

// -- FUNCTIONS --
inline void setLogLevel(LogLevel logLevel) {
    wgpuSetLogLevel(static_cast<WGPULogLevel>(logLevel));
}

inline void setLogCallback(LogCallback callback, void* userdata) {
    wgpuSetLogCallback(reinterpret_cast<WGPULogCallback>(callback), userdata);
}

inline void tickDevice(Device device) {
    wgpuDevicePoll(device, false, nullptr);
}

};