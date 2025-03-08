extern "C"
{
#include <stdint.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <stdio.h>
}

static constexpr uint32_t VERSION_MASK = 0xffff0000;

// Existing and new firmware version constants
static constexpr uint32_t V100 = 0x1000000;
static constexpr uint32_t V101 = 0x1010000;
static constexpr uint32_t V102 = 0x1020000;
static constexpr uint32_t V105 = 0x1050000;
static constexpr uint32_t V110 = 0x1100000;
static constexpr uint32_t V111 = 0x1110000;
static constexpr uint32_t V112 = 0x1120000;
static constexpr uint32_t V113 = 0x1130000;
static constexpr uint32_t V114 = 0x1140000;
static constexpr uint32_t V200 = 0x2000000;
static constexpr uint32_t V220 = 0x2200000;
static constexpr uint32_t V225 = 0x2250000;
static constexpr uint32_t V226 = 0x2260000;
static constexpr uint32_t V230 = 0x2300000;
static constexpr uint32_t V250 = 0x2500000;
static constexpr uint32_t V270 = 0x2700000;
static constexpr uint32_t V300 = 0x3000000;
static constexpr uint32_t V310 = 0x3100000;
static constexpr uint32_t V320 = 0x3200000;
static constexpr uint32_t V321 = 0x3210000;
static constexpr uint32_t V400 = 0x4000000;
static constexpr uint32_t V402 = 0x4020000;
static constexpr uint32_t V403 = 0x4030000;
static constexpr uint32_t V450 = 0x4500000;
static constexpr uint32_t V451 = 0x4510000;
static constexpr uint32_t V500 = 0x5000000;
static constexpr uint32_t V502 = 0x5020000;
static constexpr uint32_t V510 = 0x5100000;
static constexpr uint32_t V550 = 0x5500000;
// new
static constexpr uint32_t V600 = 0x6000000;
static constexpr uint32_t V602 = 0x6020000;
static constexpr uint32_t V650 = 0x6500000;
static constexpr uint32_t V700 = 0x7000000;
static constexpr uint32_t V701 = 0x7010000;
static constexpr uint32_t V720 = 0x7200000;
static constexpr uint32_t V740 = 0x7400000;
static constexpr uint32_t V760 = 0x7600000;
static constexpr uint32_t V761 = 0x7610000;

uint32_t getSystemSwVersion() {
    static uint32_t version;
    if (version != 0) [[likely]] {
        return version;
    }
    size_t size = 4;
    sysctlbyname("kern.sdk_version", &version, &size, nullptr, 0);
    return version;
}

namespace offsets {
    // Function definitions to retrieve system-specific offsets

    size_t allproc() {
        static size_t allprocOffset;
        if (allprocOffset != 0) [[likely]] {
            return allprocOffset;
        }
        switch (getSystemSwVersion() & VERSION_MASK) {
            case V100: case V101: case V102: case V105:
            case V110: case V111: case V112: case V113:
            case V114:
                allprocOffset =  0x26D1C18;
                break;
            case V200: case V220: case V225: case V226:
            case V230: case V250: case V270:
                allprocOffset =  0x2701C28;
                break;
            case V300: case V310: case V320: case V321:
                allprocOffset = 0x276DC58;
                break;
            case V400: case V402: case V403: case V450:
            case V451:
                allprocOffset = 0x27EDCB8;
                break;
            case V500: case V502: case V510: case V550:
                allprocOffset =  0x291DD00;
                break;
            case V600: case V602: case V650:
                allprocOffset =  0x2869D20;
                break;
            default:
                printf("Unsupported firmware version: 0x%x\n", getSystemSwVersion() & VERSION_MASK);
                allprocOffset = -1;
                break;
        }
        return allprocOffset;
    }

    size_t security_flags() {
        switch (getSystemSwVersion() & VERSION_MASK) {
            case V100: case V101: case V102: case V105:
            case V110: case V111: case V112: case V113:
            case V114:
                return  0x6241074;
            case V200: case V220: case V225: case V226:
            case V230: case V250: case V270:
                return  0x63E1274;
            case V300: case V310: case V320: case V321:
                return 0x6466474;
            case V400:
                return 0x6506474;
            case V402: case V403: case V450: case V451:
                return 0x6505474;
            case V500: case V502: case V510: case V550:
                return 0x66466EC;
            case V600: case V602: case V650:
                return 0x65968EC;
            case V700: case V701: case V720: case V740: case V760: case V761:
                return 0x0AC8064;
            default:
                printf("Unsupported firmware version: 0x%x\n", getSystemSwVersion() & VERSION_MASK);
                return -1;
        }
    }

    size_t qa_flags() {
        switch (getSystemSwVersion() & VERSION_MASK) {
            case V100: case V101: case V102: case V105:
            case V110: case V111: case V112: case V113:
            case V114:
            case V200: case V220: case V225: case V226:
            case V230: case V250: case V270:
            case V300: case V310: case V320: case V321:
            case V400: case V402: case V403: case V450:
            case V451:
            case V500: case V502: case V510: case V550:
                return  0x6241098;
            case V600: case V602: case V650:
                return 0x65968EC + 0x24;
            case V700: case V701: case V720: case V740: case V760: case V761:
                return 0x0AC8064 + 0x24;
            default:
                printf("Unsupported firmware version: 0x%x\n", getSystemSwVersion() & VERSION_MASK);
                return -1;
        }
    }

    size_t utoken_flags() {
        switch (getSystemSwVersion() & VERSION_MASK) {
            case V100: case V101: case V102: case V105:
            case V110: case V111: case V112: case V113:
            case V114:
            case V200: case V220: case V225: case V226:
            case V230: case V250: case V270:
            case V300: case V310: case V320: case V321:
            case V400: case V402: case V403: case V450:
            case V451:
            case V500: case V502: case V510: case V550:
                return  0x6646710;
            case V600: case V602: case V650:
                return 0x65968EC + 0x8C;
            case V700: case V701: case V720: case V740: case V760: case V761:
                return 0x0AC8064 + 0x8C;
            default:
                printf("Unsupported firmware version: 0x%x\n", getSystemSwVersion() & VERSION_MASK);
                return -1;
        }
    }

    size_t root_vnode() {
        switch (getSystemSwVersion() & VERSION_MASK) {
            case V100: case V101: case V102: case V105:
            case V110: case V111: case V112: case V113:
            case V114:
                return  0x6565540;
            case V200: case V220: case V225: case V226:
            case V230: case V250: case V270:
                return  0x67134C0;
            case V300: case V310: case V320: case V321:
                return 0x67AB4C0;
            case V400: case V402: case V403: case V450:
            case V451:
                return 0x66E74C0;
            case V500: case V502: case V510: case V550:
                return  0x6853510;
            case V600: case V602: case V650:
                return 0x679F510; 
            case V700: case V701: case V720: case V740: case V760: case V761:
                return 0x30C7510;
            default:
                printf("Unsupported firmware version: 0x%x\n", getSystemSwVersion() & VERSION_MASK);
                return -1;
        }
    }
}