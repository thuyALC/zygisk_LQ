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

float (*orig_GetCameraHeightRateValue)(void* instance);
float hook_GetCameraHeightRateValue(void* instance) { return 2.5f; }

bool (*orig_ShowUlti1)(void* instance);
bool hook_ShowUlti1(void* instance) { return true; }

bool (*orig_ShowUlti2)(void* instance);
bool hook_ShowUlti2(void* instance) { return true; }

bool (*orig_Enable60FPS)(void* instance);
bool hook_Enable60FPS(void* instance) { return true; }

void* SuperSafeThread(void*) {
    LOGI("Module ShadowHook đã kích hoạt. Đợi game nạp thư viện...");
    while (g_il2cpp_base == 0) {
        GetLibInfo("libil2cpp.so", g_il2cpp_base, g_il2cpp_size);
        usleep(500000);
    }
    LOGI("Phát hiện libil2cpp.so tại: %p [Size: %zu]", (void*)g_il2cpp_base, g_il2cpp_size);
    g_clean_backup = malloc(g_il2cpp_size);
    if (g_clean_backup) {
        memcpy(g_clean_backup, (void*)g_il2cpp_base, g_il2cpp_size);
        LOGI("Đã tạo phân vùng bộ nhớ sạch!");
    }
    void* bypass_hook = shadowhook_hook_func_addr(
        (void*)pread64, (void*)hook_pread64, (void**)&orig_pread64
    );
    if (bypass_hook) LOGI("Bypass Memory Scan OK!");
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x8D61178), (void*)hook_GetCameraHeightRateValue, (void**)&orig_GetCameraHeightRateValue);
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x6BD7BA0), (void*)hook_ShowUlti1, (void**)&orig_ShowUlti1);
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x85A87AC), (void*)hook_ShowUlti2, (void**)&orig_ShowUlti2);
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x7372080), (void*)hook_Enable60FPS, (void**)&orig_Enable60FPS);
    LOGI("Toàn bộ tính năng đã Hook xong!");
    return nullptr;
}

class MyZygiskModule : public zygisk::ModuleBase {
public:
    zygisk::Api* api;
    JNIEnv* env;

    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        dlopen("libshadowhook.so", RTLD_NOW);
        shadowhook_init(SHADOWHOOK_MODE_UNIQUE, false);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* process = this->env->GetStringUTFChars(args->nice_name, nullptr);
        if (process && strcmp(process, "com.tren.ne") == 
