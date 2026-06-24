#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>      // Đã thêm thư viện liên kết động cho dlopen
#include <sys/mman.h>
#include <android/log.h>
#include "zygisk.hpp"
#include "shadowhook.h"

#define LOG_TAG "LQ_Pro_Mod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

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

// ==================== BYPASS MEMORY SCAN (FAKE MAPS) ====================
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

// ==================== KHU VỰC HOOK LOGIC GAME ====================
// 1. Hack Cam (Kiểu số thực Single/Float)
float (*orig_GetCameraHeightRateValue)(void* instance);
float hook_GetCameraHeightRateValue(void* instance) {
    return 2.5f; 
}

// 2. Hiện Ulti dòng 1 (Kiểu Boolean/Bool)
bool (*orig_ShowUlti1)(void* instance);
bool hook_ShowUlti1(void* instance) {
    return true; 
}

// Hiện Ulti dòng 2 (Kiểu Boolean/Bool)
bool (*orig_ShowUlti2)(void* instance);
bool hook_ShowUlti2(void* instance) {
    return true; 
}

// 3. Kích hoạt 60FPS (Kiểu Boolean/Bool)
bool (*orig_Enable60FPS)(void* instance);
bool hook_Enable60FPS(void* instance) {
    return true; 
}

// ==================== LUỒNG KHỞI CHẠY CHÍNH ====================
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
        LOGI("Đã tạo phân vùng bộ nhớ sạch phục vụ ẩn danh!");
    }

    void* pread64_addr = (void*)pread64;
    
    // Đã sửa hàm hook và kiểu dữ liệu chuẩn của ShadowHook
    void* bypass_hook = shadowhook_hook_func_addr(
        pread64_addr, (void*)hook_pread64, (void**)&orig_pread64
    );
    if (bypass_hook) LOGI("✅ Đã bật tường lửa Bypass Memory Scan thành công!");

    // Tiến hành Hook các tính năng (Sử dụng shadowhook_hook_func_addr)
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x8D61178), (void*)hook_GetCameraHeightRateValue, (void**)&orig_GetCameraHeightRateValue);
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x6BD7BA0), (void*)hook_ShowUlti1, (void**)&orig_ShowUlti1);
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x85A87AC), (void*)hook_ShowUlti2, (void**)&orig_ShowUlti2);
    shadowhook_hook_func_addr((void*)(g_il2cpp_base + 0x7372080), (void*)hook_Enable60FPS, (void**)&orig_Enable60FPS);

    LOGI("✅ Toàn bộ tính năng đã được Hook ẩn danh bằng ShadowHook!");
    return nullptr;
}

// ==================== KHỞI TẠO ZYGISK MODULE ====================
class MyZygiskModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        dlopen("libshadowhook.so", RTLD_NOW); // Ép hệ thống nạp file .so đi kèm trước
        shadowhook_init();
    }
    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* process = env->GetStringUTFChars(args->nice_name, nullptr);
        if (process && strcmp(process, "com.garena.game.kgvn") == 0) {
            LOGI("Mục tiêu: Liên Quân Mobile VN. Khởi động luồng siêu bảo mật...");
            pthread_t t;
            pthread_create(&t, nullptr, SuperSafeThread, nullptr);
        }
        env->ReleaseStringUTFChars(args->nice_name, process);
    }
};

REGISTER_ZYGISK_MODULE(MyZygiskModule)
