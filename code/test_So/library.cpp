#include "library.h"

#include <iostream>
#include <syslog.h>

void notify(const char* msg) {
    openlog("Test_SO_Injector", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "%s", msg);
    closelog();
}

extern "C" void entry() {
    notify("entry() called, injection successful!");
}