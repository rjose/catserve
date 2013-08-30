#ifndef ERRORS_H
#define ERRORS_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define mem_alloc_failure(file, line) \
{\
        syslog(LOG_ALERT, "%s:%d Memory allocation failure", file, line);\
        closelog();\
        sleep(1);\
        exit(-1);\
}

#define pthread_failure(file, line) \
{\
        syslog(LOG_ALERT, "%s:%d pthread failure", file, line);\
        closelog();\
        sleep(1);\
        exit(-1);\
}

#define lua_failure(file, line) \
{\
        syslog(LOG_ALERT, "%s:%d lua/C interop failure", file, line);\
        closelog();\
        sleep(1);\
        exit(-1);\
}

#define socket_failure(file, line) \
{\
        syslog(LOG_ALERT, "%s:%d socket failure: %s", file, line,\
                                                      strerror(errno));\
        closelog();\
        sleep(1);\
        exit(-1);\
}

#endif
