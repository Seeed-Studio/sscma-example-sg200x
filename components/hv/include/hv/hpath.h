#ifndef HV_PATH_H_
#define HV_PATH_H_

#include <string> // for std::string

#include "hexport.h"

class HV_EXPORT HPath {
public:
    static bool exists(const char* path);
    static bool isdir(const char* path);
    static bool isfile(const char* path);
    static bool islink(const char* path);


    static std::string basename(const std::string& filepath);
    static std::string dirname(const std::string& filepath);
    static std::string filename(const std::string& filepath);
    static std::string suffixname(const std::string& filepath);

    static std::string join(const std::string& dir, const std::string& filename);
};

#endif // HV_PATH_H_
