#include "../include/android_sandbox.h"
#include <iostream>
#include <map>
#include <string>

/**
 * @file service_manager.cpp
 * @brief Implementation of the Android Service Manager (servicemanager) for AfriOS.
 */

class ServiceManager {
public:
    afros_status_t add_service(const std::string& name, uint32_t handle) {
        std::cout << "[SERVICE-MANAGER] Adding service: " << name << " (handle: " << handle << ")" << std::endl;
        services[name] = handle;
        return AFROS_SUCCESS;
    }

    uint32_t get_service(const std::string& name) {
        if (services.find(name) != services.end()) {
            std::cout << "[SERVICE-MANAGER] Found service: " << name << std::endl;
            return services[name];
        }
        std::cout << "[SERVICE-MANAGER] Service not found: " << name << std::endl;
        return 0;
    }

private:
    std::map<std::string, uint32_t> services;
};

static ServiceManager g_sm;

extern "C" {

afros_status_t android_sandbox_init(void) {
    std::cout << "[ANDROID-SANDBOX] Initializing Android runtime environment..." << std::endl;
    binder_init();
    
    // Default services
    g_sm.add_service("activity", 1);
    g_sm.add_service("package", 2);
    g_sm.add_service("window", 3);
    
    return AFROS_SUCCESS;
}

}
