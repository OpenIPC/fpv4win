/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "util.h"

#if defined(_WIN32)
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <shlwapi.h>
#include <thread>
#pragma comment(lib, "shlwapi.lib")
extern "C" const IMAGE_DOS_HEADER __ImageBase;
#endif // defined(_WIN32)

#if defined(__MACH__) || defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */

int uv_exepath(char *buffer, int *size) {
    /* realpath(exepath) may be > PATH_MAX so double it to be on the safe side. */
    char abspath[PATH_MAX * 2 + 1];
    char exepath[PATH_MAX + 1];
    uint32_t exepath_size;
    size_t abspath_size;

    if (buffer == nullptr || size == nullptr || *size == 0)
        return -EINVAL;

    exepath_size = sizeof(exepath);
    if (_NSGetExecutablePath(exepath, &exepath_size))
        return -EIO;

    if (realpath(exepath, abspath) != abspath)
        return -errno;

    abspath_size = strlen(abspath);
    if (abspath_size == 0)
        return -EIO;

    *size -= 1;
    if ((size_t)*size > abspath_size)
        *size = abspath_size;

    memcpy(buffer, abspath, *size);
    buffer[*size] = '\0';

    return 0;
}

#endif // defined(__MACH__) || defined(__APPLE__)

#define PATH_MAX 4096

using namespace std;

namespace toolkit {

static constexpr char CCH[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

string makeRandStr(int sz, bool printable) {
    string ret;
    ret.resize(sz);
    std::mt19937 rng(std::random_device {}());
    for (int i = 0; i < sz; ++i) {
        if (printable) {
            uint32_t x = rng() % (sizeof(CCH) - 1);
            ret[i] = CCH[x];
        } else {
            ret[i] = rng() % 0xFF;
        }
    }
    return ret;
}

bool is_safe(uint8_t b) {
    return b >= ' ' && b < 128;
}

string hexdump(const void *buf, size_t len) {
    string ret("\r\n");
    char tmp[8];
    const uint8_t *data = (const uint8_t *)buf;
    for (size_t i = 0; i < len; i += 16) {
        for (int j = 0; j < 16; ++j) {
            if (i + j < len) {
                int sz = snprintf(tmp, sizeof(tmp), "%.2x ", data[i + j]);
                ret.append(tmp, sz);
            } else {
                int sz = snprintf(tmp, sizeof(tmp), "   ");
                ret.append(tmp, sz);
            }
        }
        for (int j = 0; j < 16; ++j) {
            if (i + j < len) {
                ret += (is_safe(data[i + j]) ? data[i + j] : '.');
            } else {
                ret += (' ');
            }
        }
        ret += ('\n');
    }
    return ret;
}

string hexmem(const void *buf, size_t len) {
    string ret;
    char tmp[8];
    const uint8_t *data = (const uint8_t *)buf;
    for (size_t i = 0; i < len; ++i) {
        int sz = sprintf(tmp, "%.2x ", data[i]);
        ret.append(tmp, sz);
    }
    return ret;
}

string exePath(bool isExe /*= true*/) {
    char buffer[PATH_MAX * 2 + 1] = { 0 };
    int n = -1;
#if defined(_WIN32)
    n = GetModuleFileNameA(isExe ? nullptr : (HINSTANCE)&__ImageBase, buffer, sizeof(buffer));
#elif defined(__MACH__) || defined(__APPLE__)
    n = sizeof(buffer);
    if (uv_exepath(buffer, &n) != 0) {
        n = -1;
    }
#elif defined(__linux__)
    n = readlink("/proc/self/exe", buffer, sizeof(buffer));
#endif

    string filePath;
    if (n <= 0) {
        filePath = "./";
    } else {
        filePath = buffer;
    }

#if defined(_WIN32)
    // windows下把路径统一转换层unix风格，因为后续都是按照unix风格处理的
    for (auto &ch : filePath) {
        if (ch == '\\') {
            ch = '/';
        }
    }
#endif // defined(_WIN32)

    return filePath;
}

string exeDir(bool isExe /*= true*/) {
    auto path = exePath(isExe);
    return path.substr(0, path.rfind('/') + 1);
}

string exeName(bool isExe /*= true*/) {
    auto path = exePath(isExe);
    return path.substr(path.rfind('/') + 1);
}

// string转小写
std::string &strToLower(std::string &str) {
    transform(str.begin(), str.end(), str.begin(), towlower);
    return str;
}

// string转大写
std::string &strToUpper(std::string &str) {
    transform(str.begin(), str.end(), str.begin(), towupper);
    return str;
}

// string转小写
std::string strToLower(std::string &&str) {
    transform(str.begin(), str.end(), str.begin(), towlower);
    return std::move(str);
}

// string转大写
std::string strToUpper(std::string &&str) {
    transform(str.begin(), str.end(), str.begin(), towupper);
    return std::move(str);
}

vector<string> split(const string &s, const char *delim) {
    vector<string> ret;
    size_t last = 0;
    auto index = s.find(delim, last);
    while (index != string::npos) {
        if (index - last > 0) {
            ret.push_back(s.substr(last, index - last));
        }
        last = index + strlen(delim);
        index = s.find(delim, last);
    }
    if (!s.size() || s.size() - last > 0) {
        ret.push_back(s.substr(last));
    }
    return ret;
}

#define TRIM(s, chars)                                                                                                 \
    do {                                                                                                               \
        string map(0xFF, '\0');                                                                                        \
        for (auto &ch : chars) {                                                                                       \
            map[(unsigned char &)ch] = '\1';                                                                           \
        }                                                                                                              \
        while (s.size() && map.at((unsigned char &)s.back()))                                                          \
            s.pop_back();                                                                                              \
        while (s.size() && map.at((unsigned char &)s.front()))                                                         \
            s.erase(0, 1);                                                                                             \
    } while (0);

// 去除前后的空格、回车符、制表符
std::string &trim(std::string &s, const string &chars) {
    TRIM(s, chars);
    return s;
}

std::string trim(std::string &&s, const string &chars) {
    TRIM(s, chars);
    return std::move(s);
}

void replace(string &str, const string &old_str, const string &new_str) {
    if (old_str.empty() || old_str == new_str) {
        return;
    }
    auto pos = str.find(old_str);
    if (pos == string::npos) {
        return;
    }
    str.replace(pos, old_str.size(), new_str);
    replace(str, old_str, new_str);
}

bool start_with(const string &str, const string &substr) {
    return str.find(substr) == 0;
}

bool end_with(const string &str, const string &substr) {
    auto pos = str.rfind(substr);
    return pos != string::npos && pos == str.size() - substr.size();
}

#if defined(_WIN32)
void sleep(int second) {
    Sleep(1000 * second);
}
void usleep(int micro_seconds) {
    this_thread::sleep_for(std::chrono::microseconds(micro_seconds));
}

const char *strcasestr(const char *big, const char *little) {
    string big_str = big;
    string little_str = little;
    strToLower(big_str);
    strToLower(little_str);
    auto pos = strstr(big_str.data(), little_str.data());
    if (!pos) {
        return nullptr;
    }
    return big + (pos - big_str.data());
}

int vasprintf(char **strp, const char *fmt, va_list ap) {
    // _vscprintf tells you how big the buffer needs to be
    int len = _vscprintf(fmt, ap);
    if (len == -1) {
        return -1;
    }
    size_t size = (size_t)len + 1;
    char *str = (char *)malloc(size);
    if (!str) {
        return -1;
    }
    // _vsprintf_s is the "secure" version of vsprintf
    int r = vsprintf_s(str, len + 1, fmt, ap);
    if (r == -1) {
        free(str);
        return -1;
    }
    *strp = str;
    return r;
}

int asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vasprintf(strp, fmt, ap);
    va_end(ap);
    return r;
}

#endif // WIN32

static inline uint64_t getCurrentMicrosecondOrigin() {
#if !defined(_WIN32)
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
#else
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
#endif
}

string getTimeStr(const char *fmt, time_t time) {
    if (!time) {
        time = ::time(nullptr);
    }
    auto tm = getLocalTime(time);
    char buffer[64];
    auto success = std::strftime(buffer, sizeof(buffer), fmt, &tm);
    return 0 == success ? string(fmt) : buffer;
}

struct tm getLocalTime(time_t sec) {
    struct tm tm;
#ifdef _WIN32
    localtime_s(&tm, &sec);
#else
    localtime_r(&sec, &tm);
#endif //_WIN32
    return tm;
}

static thread_local string thread_name;

static string limitString(const char *name, size_t max_size) {
    string str = name;
    if (str.size() + 1 > max_size) {
        auto erased = str.size() + 1 - max_size + 3;
        str.replace(5, erased, "...");
    }
    return str;
}

} // namespace toolkit
