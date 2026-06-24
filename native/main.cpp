#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/mman.h>
#include <android/log.h>
#include "zygisk.hpp"

#define LOG_TAG "LQ_Pro_Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// ================================================================
// CHỈ BUILD TOÀN BỘ LOGIC TRÊN ARM64 — armeabi-v7a dùng stub rỗng
// ================================================================
#ifdef __aarch64__

#include "shadowhook.h"

uintptr_t g_il2cpp_base = 0;
size_t g_il2cpp_size = 0;
void* g_clean_backup = nullptr;

bool GetLibInfo(const char* libName, uintptr_t& base, size_t& size) {
    char line[512];
    FILE* fp = fopen("/proc/self/maps", "rt");
    if (!fp) return false;
    uintptr_t start = 0, end = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, libName) && strstr(line, "r-xp")) {
            if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
                base = start;
                size = end - start;
                fclose(fp);
                return true;
            }
        }
    }
    fclose(fp);
    return false;
}

// ==================== BYPASS MEMORY SCAN ====================
ssize_t (*orig_pread64)(int fd, void* buf, size_t count, off64_t offset);
ssize_t hook_pread64(int fd, void* buf, size_t count, off64_t offset) {
    ssize_t result = orig_pread64(fd, buf, count, offset);
    if (g_il2cpp_base && g_clean_backup && result > 0) {
        if (offset >= 0 && (size_t)offset < g_il2cpp_size) {
            size_t available = g_il2cpp_size - (size_t)offset;
            size_t copy_size = (count < available) ? count : available;
            memcpy(buf, (void*)((uintptr_t)g_clean_backup + offset), copy_size);
        }
    }
    return result;
}

// ==================== HOOK LOGIC GAME ====================
// 1. Hack Cam
float (*orig_GetCameraHeightRateValue)(void* instance);
float hook_GetCameraHeightRateValue(void* instance) { return 2.5f; }

// 2. Hiện Ulti dòng 1
bool (*orig_ShowUlti1)(void* instance);
bool hook_ShowUlti1(void* instance) { return true; }

// 3. Hiện Ulti dòng 2
bool (*orig_ShowUlti2)(void* instance);
bool hook_ShowUlti2(void* instance) { return true; }

// 4. 60FPS
bool (*orig_Enable60FPS)(void* instance);
bool hook_Enable60FPS(void* instance) { return true; }

// ==================== LUỒNG CHÍNH ====================
void* S
