//
// Created by Joel on 6/24/26.
//

#ifndef SO_INJECTOR_MESSAGEBOX_H
#define SO_INJECTOR_MESSAGEBOX_H

#include <string>

namespace msgBox
{
    void show(const std::string& title,
              const std::string& text,
              int autoCloseMs = 0);

    void showBlocking(const std::string& title, const std::string& text);
    void closeBlocking();

    void update();

    bool isOpen();
}

#endif //SO_INJECTOR_MESSAGEBOX_H
