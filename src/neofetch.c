#include "colla/colla.h" 
#include "common.h" 
#include "toys.h"
#include "tui.h"

#include <windows.h>
#include <powerbase.h>
#include <Iphlpapi.h>
#include <dxgi1_6.h>
#include <iptypes.h>

#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

TOY_SHORT_DESC(neo, "Move files or directories to trash bin.");

#define NEO_MAX_SCREENS 32
#define NEO_MAX_GPUS 8
#define NEO_MAX_DISKS ('z' - 'a')
#define NEO_LOGO_WIDTH 37
#define NEO_MAX_IPS 16

#define NEO_LIGHT 1

#if NEO_LIGHT
    #define NEO_TL  "┌"
    #define NEO_TR  "┐"
    #define NEO_BL  "└"
    #define NEO_BR  "┘"
    #define NEO_ML  "├"
    #define NEO_MR  "┤"
    #define NEO_TT  "┬"
    #define NEO_TB  "┴"
    #define NEO_HOR "─"
    #define NEO_VER "│"
#else
    #define NEO_TL  "┏"
    #define NEO_TR  "┓"
    #define NEO_BL  "┗"
    #define NEO_BR  "┛"
    #define NEO_ML  "┣"
    #define NEO_MR  "┫"
    #define NEO_TT  "┳"
    #define NEO_TB  "┻"
    #define NEO_HOR "━"
    #define NEO_VER "┃"
#endif

typedef struct neo_info_t neo_info_t;
struct neo_info_t {
    str_t user;
    str_t machine;
    str_t date;
    str_t time;
    str_t host;
    str_t os;
    str_t kernel;
    str_t battery;
    str_t uptime;
    str_t monitors[NEO_MAX_SCREENS];
    int monitor_count;
    str_t cpu;
    str_t gpus[NEO_MAX_GPUS];
    int gpu_count;
    str_t ram;
    str_t disks[NEO_MAX_DISKS];
    struct {
        strview_t name;
        str_t ip;
    } ip_local[NEO_MAX_IPS];
    int ip_count;
} neo_info = {0};

const char win_logo_txt[] = 
"$1        ,.=:!!t3Z3z.,                \n"
"$1       :tt:::tt333EE3                \n"
"$1       Et:::ztt33EEEL$2 @Ee.,      .., \n"
"$1      ;tt:::tt333EE7$2 ;EEEEEEttttt33# \n"
"$1     :Et:::zt333EEQ.$2 $EEEEEttttt33QL \n"
"$1     it::::tt333EEF$2 @EEEEEEttttt33F  \n"
"$1    ;3=*^```\"*4EEV$2 :EEEEEEttttt33@.  \n"
"$3    ,.=::::!t=., $1`$2 @EEEEEEtttz33QF   \n"
"$3   ;::::::::zt33)$2   \"4EEEtttji3P*    \n"
"$3  :t::::::::tt33.$4:Z3z..$2  ``$4 ,..g.    \n"
"$3  i::::::::zt33F$4 AEEEtttt::::ztF     \n"
"$3 ;:::::::::t33V$4 ;EEEttttt::::t3      \n"
"$3 E::::::::zt33L$4 @EEEtttt::::z3F      \n"
"$3{3=*^```\"*4E3)$4 ;EEEtttt:::::tZ`      \n"
"$3             `$4 :EEEEtttt::::z7       \n"
"$3             $4  \"VEzjt:;;z>*`         ";

str_t neo_get_logo(arena_t *arena) {
    strview_t colors[] = {
        cstrv(""),
        cstrv(TERM_FG_RED),
        cstrv(TERM_FG_GREEN),
        cstrv(TERM_FG_BLUE),
        cstrv(TERM_FG_YELLOW),
    };

    outstream_t out = ostr_init(arena);

    for (usize i = 0; i < arrlen(win_logo_txt); ++i) {
        if (win_logo_txt[i] == '$') {
            int col = win_logo_txt[++i] - '0';
            if (col <= 0 || col >= arrlen(colors)) {
                ostr_putc(&out, win_logo_txt[--i]);
                continue;
            }
            ostr_puts(&out, colors[col]);
        }
        else {
            ostr_putc(&out, win_logo_txt[i]);
        }
    }

    return ostr_to_str(&out);
}

typedef LONG (WINAPI *RtlGetVersion_f)(PRTL_OSVERSIONINFOW);
typedef PWSTR (WINAPI* BrandingFormatString_f)(__in PCWSTR pstrFormat);

#define neo_load_function(lib, func) (func##_f)neo_load_function_impl(lib, #func)

void *neo_load_function_impl(const char *lib_name, const char *func_name) {
    HMODULE lib = LoadLibraryA(lib_name);
    if (!lib) fatal("failed to load library %s", lib_name);

    void *fn = (void*)GetProcAddress(lib, func_name);
    if (!fn) fatal("failed to get %s", func_name);

    return fn;
}

HKEY neo_regopen(const char *name) {
    HKEY key;
    LONG result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        name,
        0,
        KEY_READ,
        &key
    );
    if (result != ERROR_SUCCESS) {
        fatal("failed to open registry key %s", name);
    }
    return key;
}

str_t neo_regread_str(arena_t *arena, HKEY key, const char *value) {
    u8 buf[KB(1)];
    DWORD type = 0;
    DWORD bufsize = sizeof(buf);
    LONG result = RegQueryValueExA(
        key,
        value,
        NULL,
        &type,
        buf,
        &bufsize
    );
    if (result != ERROR_SUCCESS) {
        fatal("failed to read registry key %s", value);
    }

    return str(arena, strv((char*)buf));
}

i64 neo_regread_int(HKEY key, const char *value) {
    DWORD regvalue = 0;
    DWORD type = 0;
    DWORD bufsize = sizeof(regvalue);
    LONG result = RegQueryValueExA(
        key,
        value,
        NULL,
        &type,
        (u8*)&regvalue,
        &bufsize
    );
    if (result != ERROR_SUCCESS) {
        fatal("failed to read registry key %s", value);
    }

    return regvalue;
}

str_t neo_regkey_str(arena_t *arena, const char *key_name, const char *value) {
    HKEY key = neo_regopen(key_name);
    str_t out = neo_regread_str(arena, key, value);
    RegCloseKey(key);
    return out;
}

void job_user_and_machine(void *userdata) {
    arena_t *arena = userdata;

    wchar_t namebuf[KB(1)] = {0};
    DWORD namelen = arrlen(namebuf);
    if (!GetUserNameW(namebuf, &namelen)) {
        fatal("failed to get username: %v", os_get_error_string(os_get_last_error()));
    }
    
    neo_info.user = str_from_str16(arena, str16_init(namebuf, namelen));
    neo_info.machine = os_get_system_info().machine_name;
}

void job_host(void *userdata) {
    arena_t *arena = userdata;

    HKEY host_key = neo_regopen("HARDWARE\\DESCRIPTION\\System\\BIOS");
    str_t host_name   = neo_regread_str(arena, host_key, "SystemProductName");
    str_t host_family = neo_regread_str(arena, host_key, "SystemFamily");
    neo_info.host = str_fmt(arena, "%v (%v)", host_name, host_family);
}

void job_date_and_time(void *userdata) {
    arena_t *arena = userdata;

    time_t t = time(0);
    struct tm tm = {0};
    localtime_s(&tm, &t);

    neo_info.date = str_fmt(arena, "%02d/%02d/%04d", tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
    neo_info.time = str_fmt(arena, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

void job_os(void *userdata) {
    arena_t *arena = userdata;

    RtlGetVersion_f rtl_get_version = neo_load_function("ntdll.dll", RtlGetVersion);
    BrandingFormatString_f branding_fmt_str = neo_load_function("winbrand.dll", BrandingFormatString);

    RTL_OSVERSIONINFOW os_info = {
        .dwOSVersionInfoSize = sizeof(os_info),
    };

    LONG res = rtl_get_version(&os_info);

    if (res) {
        fatal("RtlGetVersion failed with code: %d: %v", res, os_get_error_string(res));
    }

    bool is_win11 = false;
    if (os_info.dwBuildNumber > 22000) {
        is_win11 = true;
    }

    u16 *branding_str = branding_fmt_str(L"%WINDOWS_LONG%");

    str_t branding = str_from_str16(arena, str16_init(branding_str, 0));
    
    GlobalFree((HGLOBAL)branding_str);

    strview_t arch_names[] = {
        [OS_ARCH_X86]   = cstrv("x86"),
        [OS_ARCH_ARM]   = cstrv("ARM"),
        [OS_ARCH_IA64]  = cstrv("IA64"),
        [OS_ARCH_AMD64] = cstrv("x86_x64"),
        [OS_ARCH_ARM64] = cstrv("ARM64"),
    };

    neo_info.os = str_fmt(arena, "%v %v", branding, arch_names[os_get_system_info().architecture]);
    neo_info.kernel = str_fmt(arena, "WIN32_NT %d.%d.%d", os_info.dwMajorVersion, os_info.dwMinorVersion, os_info.dwBuildNumber);
}

void job_battery(void *userdata) {
    arena_t *arena = userdata;

    SYSTEM_POWER_STATUS ps = {0};
    if (!GetSystemPowerStatus(&ps)) {
        fatal("failed to get system power status: %v", os_get_error_string(os_get_last_error()));
    }
    
    bool ac_online = ps.ACLineStatus == 1;
    strview_t battery_colors[] = {
        cstrv(TERM_FG_GREEN),
        cstrv(TERM_FG_YELLOW),
        cstrv(TERM_FG_RED),
    };

    int battery_percent = ps.BatteryLifePercent;
    bool power_saver_on = ps.SystemStatusFlag == 1;
    int battery_lifetime = ps.BatteryLifeTime;

    outstream_t battery = ostr_init(arena);

    strview_t bc = cstrv(TERM_RESET);
    if (ps.BatteryFlag & 1) {
        bc = battery_colors[0];
    }
    else if (ps.BatteryFlag & 2) {
        bc = battery_colors[1];
    }
    else if (ps.BatteryFlag & 4) {
        bc = battery_colors[2];
    }

    if (battery_percent >= 0) {
        ostr_print(&battery, "%v%d%%" TERM_RESET, bc, battery_percent);
    }

    if (ps.BatteryFlag & 8) {
        ostr_puts(&battery, strv(" charging"));
    }

    if (power_saver_on) {
        ostr_puts(&battery, strv(" on power saver"));
    }

    if (ac_online) {
        ostr_puts(&battery, strv(" [AC connected]"));
    }

    if (battery_lifetime >= 0) {
        os_time_t time = common_convert_time(battery_lifetime);

        ostr_print(&battery, " time left: %02d:%02d:%02d", time.hours, time.minutes, time.seconds);
    }

    neo_info.battery = ostr_to_str(&battery);
}

void job_uptime(void *userdata) {
    arena_t *arena = userdata;

    os_time_t os_uptime = common_convert_time(GetTickCount64() / 1000);
    neo_info.uptime = common_print_time(arena, &os_uptime);
}

void job_display(void *userdata) {
    arena_t *arena = userdata;

    DISPLAY_DEVICEA adapter = { .cb = sizeof(adapter), };
    DISPLAY_DEVICEA monitor = { .cb = sizeof(monitor) };

    for (int i = 0; EnumDisplayDevicesA(NULL, i, &adapter, 0); ++i) {
        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
            continue;
        }
        for (int k = 0; EnumDisplayDevicesA(adapter.DeviceName, k, &monitor, 0); ++k) {
            if (!(monitor.StateFlags & DISPLAY_DEVICE_ACTIVE)) {
                continue;
            }
            DEVMODEA monitor_info = { .dmSize = sizeof(monitor_info), };
            if (!EnumDisplaySettingsA(adapter.DeviceName, ENUM_CURRENT_SETTINGS, &monitor_info)) {
                continue;
            }
            neo_info.monitors[neo_info.monitor_count++] = str_fmt(arena, "%ux%u @ %u HZ", monitor_info.dmPelsWidth, monitor_info.dmPelsHeight, monitor_info.dmDisplayFrequency);
        }
    }
}

void job_cpu(void *userdata) {
    arena_t *arena = userdata;

    HKEY cpu_key = neo_regopen("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0");
    str_t cpu_name = neo_regread_str(arena, cpu_key, "ProcessorNameString");
    i64 cpu_freq = neo_regread_int(cpu_key, "~MHz");

    neo_info.cpu = str_fmt(arena, "%v (%u) @ %.2f GHz", cpu_name, os_get_system_info().processor_count, (float)cpu_freq * 0.001f);
}

void job_gpu(void *userdata) {
    arena_t *arena = userdata;

    IDXGIFactory5 *gpu_factory = NULL;
    if (FAILED(CreateDXGIFactory(&IID_IDXGIFactory5, (void**)&gpu_factory))) {
        fatal("could not create gpu factory");
    }
    
    uint gpu_ids[NEO_MAX_GPUS] = {0};

    IDXGIAdapter1 *gpu_adapter = NULL;
    for (uint i = 0; IDXGIFactory5_EnumAdapters1(gpu_factory, i, &gpu_adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc = {0};
        IDXGIAdapter1_GetDesc1(gpu_adapter, &desc);

        if (desc.DedicatedVideoMemory == 0) {
            continue;
        }

        bool ignore = false;

        for (int k = 0; k < neo_info.gpu_count; ++k) {
            if (desc.DeviceId == gpu_ids[k]) {
                ignore = true;
                break;
            }
        }

        if (ignore) {
            continue;
        }

        str_t gpu_desc = str_from_str16(arena, str16_init(desc.Description, 0));
        u64 vmem = desc.DedicatedVideoMemory;

        neo_info.gpus[neo_info.gpu_count] = str_fmt(arena, "%v (%$$$uB)", gpu_desc, vmem);
        gpu_ids[neo_info.gpu_count++] = desc.DeviceId;
    }
}

void job_ram(void *userdata) {
    arena_t *arena = userdata;

    MEMORYSTATUSEX memory_info = { .dwLength = sizeof(memory_info) };
    if (!GlobalMemoryStatusEx(&memory_info)) {
        fatal("failed to get memory info");
    }

    int memory_percent = memory_info.dwMemoryLoad;
    u64 memory_total = memory_info.ullTotalPhys;
    u64 memory_free = memory_info.ullAvailPhys;
    u64 memory_used = memory_total - memory_free;

    strview_t memory_color = cstrv(TERM_FG_RED);
    if (memory_percent < 75) memory_color = strv(TERM_FG_YELLOW);
    if (memory_percent < 50) memory_color = strv(TERM_FG_GREEN);

    neo_info.ram = str_fmt(arena, "%$$$zuB / %$$$zuB (%v%d%%" TERM_RESET ")", memory_used, memory_total, memory_color, memory_percent);
}

void job_disk(void *userdata) {
    arena_t *arena = userdata;

    for (int i = 0; i < NEO_MAX_DISKS; ++i) {
        char disk_name[] = "X:\\";
        disk_name[0] = (char)i + 'A';

        ULARGE_INTEGER disk_free;
        ULARGE_INTEGER disk_total;
        if (!GetDiskFreeSpaceExA(disk_name, NULL, &disk_total, &disk_free)) {
            continue;
        }

        u64 df = disk_free.QuadPart;
        u64 dt = disk_total.QuadPart;
        u64 du = dt - df;
        u64 dp = (u64)(((double)du * 100.0) / (double)dt);

        strview_t disk_color = cstrv(TERM_FG_RED);
        if (dp < 75) disk_color = strv(TERM_FG_YELLOW);
        if (dp < 50) disk_color = strv(TERM_FG_GREEN);

        neo_info.disks[i] = str_fmt(arena, "%$$$zuB / %$$$zuB (%v%zu%%" TERM_RESET ")", du, dt, disk_color, dp);
    }
}

void job_ip(void *userdata) {
    arena_t *arena = userdata;

    ULONG ip_adapt_size = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &ip_adapt_size);

    u32 ip_adapt_count = ip_adapt_size / sizeof(IP_ADAPTER_ADDRESSES);
    IP_ADAPTER_ADDRESSES *ip_adapters = alloc(arena, IP_ADAPTER_ADDRESSES, ip_adapt_count);

    if (GetAdaptersAddresses(
        AF_UNSPEC, 
        GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, 
        NULL, 
        ip_adapters, 
        &ip_adapt_size
    )) {
        fatal("failed to get ip info: %v", os_get_error_string(os_get_last_error()));
    }

    DWORD invalid_iftypes[] = {
        IF_TYPE_SOFTWARE_LOOPBACK,
        IF_TYPE_PROP_VIRTUAL,
    };

    for (IP_ADAPTER_ADDRESSES *aa = ip_adapters; aa; aa = aa->Next) {
        if (aa->OperStatus != IfOperStatusUp || aa->Ipv4Enabled != 1) {
            continue;
        }

        bool skip = false;
        for (int i = 0; i < arrlen(invalid_iftypes); ++i) {
            if (aa->IfType == invalid_iftypes[i]) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        for (IP_ADAPTER_UNICAST_ADDRESS *ua = aa->FirstUnicastAddress; ua; ua = ua->Next) {
            // skip ipv6
            if (ua->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            char addr[NI_MAXHOST];
            getnameinfo(
                ua->Address.lpSockaddr,
                ua->Address.iSockaddrLength,
                addr, sizeof(addr),
                NULL, 0,
                NI_NUMERICHOST
            );
            neo_info.ip_local[neo_info.ip_count].name = str_sub(str_from_str16(arena, str16_init(aa->FriendlyName, 0)), 0, 5); 
            neo_info.ip_local[neo_info.ip_count].ip = str(arena, addr); 
            neo_info.ip_count++;
            break;
        }
    }
}

void print_line_impl(strview_t left, strview_t right, instream_t *logo, i64 left_width, i64 right_width) {
    strview_t logo_line = istr_get_line(logo);
    if (logo_line.len == 0) print("%*s", NEO_LOGO_WIDTH, "");
    else                    print("%v" TERM_RESET, logo_line);
    print(TERM_FG_ORANGE NEO_VER " " TERM_FG_LIGHT_ORANGE "%v", left);
    i64 left_spaces = left_width - 3 - left.len;
    print(TERM_FG_ORANGE "%*s" NEO_VER " " TERM_RESET, left_spaces, "");
    
    i64 right_len = right.len;
    if (strv_contains(right, '\x1b')) {
        usize pos = strv_find(right, '\x1b', 0);
        strview_t bef = strv_sub(right, 0, pos);
        strview_t aft = strv_sub(right, pos, STR_END);
        pos = strv_find(aft, 'm', 0);
        strview_t cod = strv_sub(aft, 0, pos + 1);
        aft = strv_sub(aft, pos + 1, STR_END);
        usize res_len = sizeof(TERM_RESET) - 1;
        right_len = bef.len + (aft.len - res_len);
        if (right_len > right_width) {
            i64 diff = right_len - right_width;
            if (diff >= (i64)(aft.len - res_len)) {
                diff -= aft.len - res_len;
                aft.len = 0;
                bef.len -= diff;
            }
            else {
                aft.len -= diff + res_len;
            }
        }
        print("%v%v%v", bef, cod, aft);
    }
    else {
        if (right_len > (right_width)) {
            right.len = right_width;
            right_len = right.len;
        }
        print("%v", right);
    }
    i64 right_spaces = right_width - right_len;
    print(TERM_FG_ORANGE "%*s" NEO_VER "\n", right_spaces, "");
}

void TOY(neo)(int argc, char **argv) {
    COLLA_UNUSED(argc); COLLA_UNUSED(argv);
    net_init();

    arena_t arena = arena_make(ARENA_VIRTUAL, GB(1));

    job_queue_t *jq = jq_init(&arena, 10);

    struct {
        job_func_f *func;
        arena_t arena;
    } jobs[] = {
        { job_user_and_machine },
        { job_host },
        { job_date_and_time },
        { job_os },
        { job_battery },
        { job_uptime },
        { job_display },
        { job_cpu },
        { job_gpu },
        { job_ram },
        { job_disk },
        { job_ip },
    };

    usize page_size = os_get_system_info().page_size;
    usize arena_size = MAX(page_size, MB(1));

    for (int i = 0; i < arrlen(jobs); ++i) {
        jobs[i].arena = arena_scratch(&arena, arena_size);
        jq_push(&arena, jq, jobs[i].func, &jobs[i].arena);
    }

    jq_cleanup(jq);

    instream_t logo = istr_init(strv(neo_get_logo(&arena)));

    // prepare terminal

    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD console_mode = 0;
    if (!GetConsoleMode(handle, &console_mode)) {
        fatal("couldn't get console mode: %v", os_get_error_string(os_get_last_error()));
    }

    console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(handle, console_mode);

    tui_update_size();

    i64 left_len = 14;
    i64 right_len = 0;

    right_len = MAX((usize)right_len, neo_info.user.len);
    right_len = MAX((usize)right_len, neo_info.machine.len);
    right_len = MAX((usize)right_len, neo_info.date.len);
    right_len = MAX((usize)right_len, neo_info.time.len);
    right_len = MAX((usize)right_len, neo_info.host.len);
    right_len = MAX((usize)right_len, neo_info.os.len);
    right_len = MAX((usize)right_len, neo_info.kernel.len);
    right_len = MAX((usize)right_len, neo_info.battery.len);
    right_len = MAX((usize)right_len, neo_info.uptime.len);
    for (int i = 0; i < neo_info.monitor_count; ++i) {
        right_len = MAX((usize)right_len, neo_info.monitors[i].len);
    }
    right_len = MAX((usize)right_len, neo_info.cpu.len);
    for (int i = 0; i < neo_info.gpu_count; ++i) {
        right_len = MAX((usize)right_len, neo_info.gpus[i].len);
    }
    right_len = MAX((usize)right_len, neo_info.ram.len);
    for (int i = 0; i < NEO_MAX_DISKS; ++i) {
        str_t d = neo_info.disks[i];
        if (d.len > 0) {
            right_len = MAX((usize)right_len, d.len);
        }
    }
    for (int i = 0; i < neo_info.ip_count; ++i) {
        right_len = MAX((usize)right_len, neo_info.ip_local[i].name.len);
    }

    right_len += 7;

    i64 tw = tui_width() - NEO_LOGO_WIDTH;
    if (left_len + right_len > tw) {
        right_len = tw - left_len; 
    }
    i64 width = left_len + right_len;

    print("\n");

    print("%v" TERM_RESET, istr_get_line(&logo));

    print(TERM_FG_ORANGE);
    print(NEO_TL);
    for (i64 i = 2; i < width; ++i) {
        print(NEO_HOR);
    }
    print(NEO_TR "\n");

    print("%v" TERM_RESET, istr_get_line(&logo));

    print(TERM_FG_ORANGE NEO_VER " ");

    print(
        TERM_FG_GREEN "%v" 
        TERM_RESET "@" 
        TERM_FG_RED "%v ",
        neo_info.user, neo_info.machine
    );

    usize header_rem = width - 5 - (neo_info.user.len + neo_info.machine.len);
    if (neo_info.date.len > header_rem) neo_info.date.len = 0;
    header_rem -= neo_info.date.len;
    if (neo_info.time.len > header_rem) neo_info.time.len = 0;
    header_rem -= neo_info.time.len;
    print(TERM_RESET "%v %v", neo_info.date, neo_info.time);

    i64 header_spaces = width - 5 - (neo_info.user.len + neo_info.machine.len + neo_info.date.len + neo_info.time.len);

    print(TERM_FG_ORANGE "%*s" NEO_VER "\n", header_spaces, "");

    print("%v" TERM_RESET, istr_get_line(&logo));

    print(TERM_FG_ORANGE NEO_ML);
    for (i64 i = 2; i < left_len; ++i) {
        print(NEO_HOR);
    }
    print(NEO_TT);
    for (i64 i = 1; i < right_len; ++i) {
        print(NEO_HOR);
    }
    print(NEO_MR "\n");

    right_len -= 2;

#define print_line(left, right) print_line_impl(left, strv(right), &logo, left_len, right_len)

    print_line(strv("Host"), neo_info.host);
    print_line(strv("Os"), neo_info.os);
    print_line(strv("Kernel"), neo_info.kernel);
    print_line(strv("Battery"), neo_info.battery);
    print_line(strv("Uptime"), neo_info.uptime);
    for (int i = 0; i < neo_info.monitor_count; ++i) {
        if (i == 0) {
            print_line(strv("Monitor/s"), neo_info.monitors[i]);
        }
        else {
            print_line(strv(""), neo_info.monitors[i]);
        }
    }
    print_line(strv("CPU"), neo_info.cpu);
    for (int i = 0; i < neo_info.gpu_count; ++i) {
        if (i == 0) {
            print_line(strv("GPU/s"), neo_info.gpus[i]);
        }
        else {
            print_line(strv(""), neo_info.gpus[i]);
        }
    }
    print_line(strv("RAM"), neo_info.ram);
    for (int i = 0; i < NEO_MAX_DISKS; ++i) {
        char c = (char)i + 'A';
        if (neo_info.disks[i].len > 0) {
            arena_t scratch = arena;
            str_t name = str_fmt(&scratch, "Disk (%c:\\)", c);
            print_line(strv(name), neo_info.disks[i]);
        }
    }
    for (int i = 0; i < neo_info.ip_count; ++i) {
        arena_t scratch = arena;
        str_t label = str_fmt(&scratch, "IP (%v)", neo_info.ip_local[i].name);
        print_line(strv(label), neo_info.ip_local[i].ip);
    }

    right_len += 2;

    print(TERM_FG_ORANGE "%*s" NEO_BL, NEO_LOGO_WIDTH, "");
    for (i64 i = 2; i < left_len; ++i) {
        print(NEO_HOR);
    }
    print(NEO_TB);
    for (i64 i = 1; i < right_len; ++i) {
        print(NEO_HOR);
    }
    print(NEO_BR TERM_RESET "\n");
}
