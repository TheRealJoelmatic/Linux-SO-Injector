//
// config.h — tiny key/value persistence backed by ~/.config/so_injector/config
//

#pragma once
#include <string>

namespace config {
    std::string get(const std::string& key);

    void set(const std::string& key, const std::string& value);

}
