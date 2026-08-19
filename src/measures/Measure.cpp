#include "measures/Measure.h"
#include <windows.h>
#include <cwchar>

using namespace argos;

namespace {

class ClockMeasure : public Measure {
public:
    void Update() override {
        GetLocalTime(&m_time);
    }
    std::wstring ValueText() const override {
        wchar_t buf[16];
        swprintf_s(buf, L"%02u:%02u:%02u", m_time.wHour, m_time.wMinute, m_time.wSecond);
        return buf;
    }
    double ValueFraction() const override {
        return 0.0;
    }

private:
    SYSTEMTIME m_time{};
};

class CPUUsageMeasure : public Measure {
public:
    void Update() override {
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return;
        }
        ULONGLONG idle = ToULL(idleTime);
        // lpKernelTime includes idle time on Windows, so kernel+user is the
        // right "total" denominator and (total - idleDelta) is busy time.
        ULONGLONG total = ToULL(kernelTime) + ToULL(userTime);

        if (m_haveSample) {
            ULONGLONG idleDelta = idle - m_lastIdle;
            ULONGLONG totalDelta = total - m_lastTotal;
            m_fraction = totalDelta == 0 ? 0.0
                                          : 1.0 - (static_cast<double>(idleDelta) / totalDelta);
        }
        m_lastIdle = idle;
        m_lastTotal = total;
        m_haveSample = true;
    }
    std::wstring ValueText() const override {
        wchar_t buf[8];
        swprintf_s(buf, L"%d%%", static_cast<int>(m_fraction * 100.0 + 0.5));
        return buf;
    }
    double ValueFraction() const override {
        return m_fraction;
    }

private:
    static ULONGLONG ToULL(const FILETIME& ft) {
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

    bool m_haveSample = false;
    ULONGLONG m_lastIdle = 0;
    ULONGLONG m_lastTotal = 0;
    double m_fraction = 0.0;
};

class MemoryUsageMeasure : public Measure {
public:
    void Update() override {
        MEMORYSTATUSEX status{};
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status)) {
            m_fraction = status.dwMemoryLoad / 100.0;
        }
    }
    std::wstring ValueText() const override {
        wchar_t buf[8];
        swprintf_s(buf, L"%d%%", static_cast<int>(m_fraction * 100.0 + 0.5));
        return buf;
    }
    double ValueFraction() const override {
        return m_fraction;
    }

private:
    double m_fraction = 0.0;
};

class DiskUsageMeasure : public Measure {
public:
    explicit DiskUsageMeasure(std::wstring rootPath) : m_rootPath(std::move(rootPath)) {}

    void Update() override {
        ULARGE_INTEGER freeBytes{}, totalBytes{};
        if (GetDiskFreeSpaceExW(m_rootPath.c_str(), nullptr, &totalBytes, &freeBytes) &&
            totalBytes.QuadPart != 0) {
            ULONGLONG used = totalBytes.QuadPart - freeBytes.QuadPart;
            m_fraction = static_cast<double>(used) / static_cast<double>(totalBytes.QuadPart);
        }
    }
    std::wstring ValueText() const override {
        wchar_t buf[8];
        swprintf_s(buf, L"%d%%", static_cast<int>(m_fraction * 100.0 + 0.5));
        return buf;
    }
    double ValueFraction() const override {
        return m_fraction;
    }

private:
    std::wstring m_rootPath;
    double m_fraction = 0.0;
};

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    w.resize(len - 1); // drop the null terminator MultiByteToWideChar counted
    return w;
}

}

std::unique_ptr<Measure> argos::CreateMeasure(const std::string& className, const IniSection& config,
                                               std::string& outError) {
    if (className == "Clock") {
        return std::make_unique<ClockMeasure>();
    }
    if (className == "CPUUsage") {
        return std::make_unique<CPUUsageMeasure>();
    }
    if (className == "MemoryUsage") {
        return std::make_unique<MemoryUsageMeasure>();
    }
    if (className == "DiskUsage") {
        const std::string* drive = config.Find("Drive");
        std::string letter = (drive && !drive->empty()) ? drive->substr(0, 1) : "C";
        std::wstring root = ToWide(letter) + L":\\";
        return std::make_unique<DiskUsageMeasure>(root);
    }
    outError = "unknown measure class \"" + className + "\"";
    return nullptr;
}
