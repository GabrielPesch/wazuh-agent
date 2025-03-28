#include "linuxInfoHelper.hpp"
#include "network/networkFamilyDataAFactory.h"
#include "network/networkLinuxWrapper.h"
#include "networkHelper.hpp"
#include "networkUnixHelper.hpp"
#include "osinfo/sysOsParsers.h"
#include "packages/berkeleyRpmDbHelper.h"
#include "packages/modernPackageDataRetriever.hpp"
#include "packages/packageLinuxDataRetriever.h"
#include "ports/portImpl.h"
#include "ports/portLinuxWrapper.h"
#include "sharedDefs.h"
#include "stringHelper.hpp"
#include "sysInfo.hpp"
#include <cstring>
#include <file_io_utils.hpp>
#include <filesystem_wrapper.hpp>
#include <fstream>
#include <iostream>
#include <proc/readproc.h>
#include <regex>
#include <span>
#include <string>
#include <sys/utsname.h>

using ProcessInfo = std::unordered_map<int64_t, std::pair<int32_t, std::string>>;

/// @brief Returns the process information of a port
/// @param procPath Process path
/// @param inodes Inodes of the process
/// @return Process information
ProcessInfo PortProcessInfo(const std::string& procPath, const std::deque<int64_t>& inodes);

constexpr auto A_HUNDRED {100};
constexpr auto A_THOUSAND {1000};

struct ProcTableDeleter
{
    void operator()(PROCTAB* proc)
    {
        closeproc(proc);
    }

    void operator()(proc_t* proc)
    {
        freeproc(proc);
    }
};

using SysInfoProcessesTable = std::unique_ptr<PROCTAB, ProcTableDeleter>;
using SysInfoProcess = std::unique_ptr<proc_t, ProcTableDeleter>;

static void ParseLineAndFillMap(const std::string& line,
                                const std::string& separator,
                                std::map<std::string, std::string>& systemInfo)
{
    const auto pos {line.find(separator)};

    if (pos != std::string::npos)
    {
        const auto key {Utils::Trim(line.substr(0, pos), " \t\"")};
        const auto value {Utils::Trim(line.substr(pos + 1), " \t\"")};
        systemInfo[key] = value;
    }
}

static bool
GetSystemInfo(const std::string& fileName, const std::string& separator, std::map<std::string, std::string>& systemInfo)
{
    std::fstream file {fileName, std::ios_base::in};
    const bool ret {file.is_open()};

    if (ret)
    {
        std::string line;

        while (file.good())
        {
            std::getline(file, line);
            ParseLineAndFillMap(line, separator, systemInfo);
        }
    }

    return ret;
}

static nlohmann::json GetProcessInfo(const SysInfoProcess& process)
{
    nlohmann::json jsProcessInfo {};
    // Current process information
    jsProcessInfo["pid"] = std::to_string(process->tid);
    jsProcessInfo["name"] = process->cmd;
    jsProcessInfo["state"] = &process->state;
    jsProcessInfo["ppid"] = process->ppid;
    jsProcessInfo["utime"] = process->utime;
    jsProcessInfo["stime"] = process->stime;
    std::string commandLine;
    std::string commandLineArgs;

    if (process->cmdline)
    {
        auto cmdlineSpan = std::span<char*>(process->cmdline, std::numeric_limits<size_t>::max());

        if (!cmdlineSpan.empty() && cmdlineSpan[0])
        {
            commandLine = cmdlineSpan[0];

            for (size_t idx = 1; idx < cmdlineSpan.size() && cmdlineSpan[idx]; ++idx)
            {
                if (std::strlen(cmdlineSpan[idx]) > 0)
                {
                    commandLineArgs += cmdlineSpan[idx];

                    if (idx + 1 < cmdlineSpan.size() && cmdlineSpan[idx + 1])
                    {
                        commandLineArgs += " ";
                    }
                }
            }
        }
    }

    jsProcessInfo["cmd"] = commandLine;
    jsProcessInfo["argvs"] = commandLineArgs;
    jsProcessInfo["euser"] = process->euser;
    jsProcessInfo["ruser"] = process->ruser;
    jsProcessInfo["suser"] = process->suser;
    jsProcessInfo["egroup"] = process->egroup;
    jsProcessInfo["rgroup"] = process->rgroup;
    jsProcessInfo["sgroup"] = process->sgroup;
    jsProcessInfo["fgroup"] = process->fgroup;
    jsProcessInfo["priority"] = process->priority;
    jsProcessInfo["nice"] = process->nice;
    jsProcessInfo["size"] = process->size;
    jsProcessInfo["vm_size"] = process->vm_size;
    jsProcessInfo["resident"] = process->vm_rss;
    jsProcessInfo["share"] = process->share;
    jsProcessInfo["start_time"] = Utils::timeTick2unixTime(process->start_time);
    jsProcessInfo["pgrp"] = process->pgrp;
    jsProcessInfo["session"] = process->session;
    jsProcessInfo["tgid"] = process->tgid;
    jsProcessInfo["tty"] = process->tty;
    jsProcessInfo["processor"] = process->processor;
    jsProcessInfo["nlwp"] = process->nlwp;
    return jsProcessInfo;
}

static void GetSerialNumber(nlohmann::json& info)
{
    info["board_serial"] = EMPTY_VALUE;
    std::fstream file {WM_SYS_HW_DIR, std::ios_base::in};

    if (file.is_open())
    {
        std::string serial;
        file >> serial;
        info["board_serial"] = serial;
    }
}

static void GetCpuName(nlohmann::json& info)
{
    info["cpu_name"] = UNKNOWN_VALUE;
    std::map<std::string, std::string> systemInfo;
    GetSystemInfo(WM_SYS_CPU_DIR, ":", systemInfo);
    const auto& it {systemInfo.find("model name")};

    if (it != systemInfo.end())
    {
        info["cpu_name"] = it->second;
    }
}

static void GetCpuCores(nlohmann::json& info)
{
    info["cpu_cores"] = UNKNOWN_VALUE;
    std::map<std::string, std::string> systemInfo;
    GetSystemInfo(WM_SYS_CPU_DIR, ":", systemInfo);
    const auto& it {systemInfo.find("processor")};

    if (it != systemInfo.end())
    {
        info["cpu_cores"] = std::stoi(it->second) + 1;
    }
}

static void GetCpuMHz(nlohmann::json& info)
{
    info["cpu_mhz"] = UNKNOWN_VALUE;
    int retVal {0};
    std::map<std::string, std::string> systemInfo;
    GetSystemInfo(WM_SYS_CPU_DIR, ":", systemInfo);

    const auto& it {systemInfo.find("cpu MHz")};

    if (it != systemInfo.end())
    {
        info["cpu_mhz"] = std::stoi(it->second) + 1;
    }
    else
    {
        int cpuFreq {0};
        const auto fsWrapper = std::make_unique<file_system::FileSystemWrapper>();
        std::vector<std::filesystem::path> cpusInfo;
        if (fsWrapper->exists(WM_SYS_CPU_FREC_DIR) && fsWrapper->is_directory(WM_SYS_CPU_FREC_DIR))
        {
            cpusInfo = fsWrapper->list_directory(WM_SYS_CPU_FREC_DIR);
        }
        constexpr auto CPU_FREQ_DIRNAME_PATTERN {"cpu[0-9]+"};
        const std::regex cpuDirectoryRegex {CPU_FREQ_DIRNAME_PATTERN};

        for (const auto& cpu : cpusInfo)
        {
            const auto cpuFilename = cpu.filename().string();
            if (std::regex_match(cpuFilename, cpuDirectoryRegex))
            {
                std::fstream file {WM_SYS_CPU_FREC_DIR + cpuFilename + "/cpufreq/cpuinfo_max_freq", std::ios_base::in};

                if (file.is_open())
                {
                    std::string frequency;
                    std::getline(file, frequency);

                    try
                    {
                        cpuFreq = std::stoi(frequency); // Frequency on KHz

                        if (cpuFreq > retVal)
                        {
                            retVal = cpuFreq;
                        }
                    }
                    catch (...) // NOLINT(bugprone-empty-catch)
                    {
                    }
                }
            }
        }

        info["cpu_mhz"] = retVal / A_THOUSAND; // Convert frequency from KHz to MHz
    }
}

static void GetMemory(nlohmann::json& info)
{
    std::map<std::string, std::string> systemInfo;
    GetSystemInfo(WM_SYS_MEM_DIR, ":", systemInfo);

    auto memTotal {1ull};
    auto memFree {0ull};

    const auto& itTotal {systemInfo.find("MemTotal")};

    if (itTotal != systemInfo.end())
    {
        memTotal = std::stoull(itTotal->second);
    }

    const auto& itAvailable {systemInfo.find("MemAvailable")};
    const auto& itFree {systemInfo.find("MemFree")};

    if (itAvailable != systemInfo.end())
    {
        memFree = std::stoull(itAvailable->second);
    }
    else if (itFree != systemInfo.end())
    {
        memFree = std::stoull(itFree->second);
    }

    const auto ramTotal {memTotal == 0 ? 1 : memTotal};
    info["ram_total"] = ramTotal;
    info["ram_free"] = memFree;
    info["ram_usage"] = A_HUNDRED - (A_HUNDRED * memFree / ramTotal);
}

nlohmann::json SysInfo::getHardware() const
{
    nlohmann::json hardware;

    GetSerialNumber(hardware);
    GetCpuName(hardware);
    GetCpuCores(hardware);
    GetCpuMHz(hardware);
    GetMemory(hardware);
    return hardware;
}

nlohmann::json SysInfo::getPackages() const
{
    nlohmann::json packages;
    getPackages([&packages](nlohmann::json& data) { packages.push_back(data); });
    return packages;
}

static bool GetOsInfoFromFiles(nlohmann::json& info)
{
    bool ret {false};
    const std::vector<std::string> UNIX_RELEASE_FILES {"/etc/os-release", "/usr/lib/os-release"};
    constexpr auto CENTOS_RELEASE_FILE {"/etc/centos-release"};
    static const std::vector<std::pair<std::string, std::string>> PLATFORMS_RELEASE_FILES {
        {"centos", CENTOS_RELEASE_FILE},
        {"fedora", "/etc/fedora-release"},
        {"rhel", "/etc/redhat-release"},
        {"gentoo", "/etc/gentoo-release"},
        {"suse", "/etc/SuSE-release"},
        {"debian", "/etc/debian_version"},
        {"slackware", "/etc/slackware-version"},
        {"ubuntu", "/etc/lsb-release"},
    };
    const auto parseFnc {[&info](const std::string& fileName, const std::string& platform)
                         {
                             std::fstream file {fileName, std::ios_base::in};

                             if (file.is_open())
                             {
                                 const auto spParser {FactorySysOsParser::create(platform)};
                                 return spParser->parseFile(file, info);
                             }

                             return false;
                         }};

    for (const auto& unixReleaseFile : UNIX_RELEASE_FILES)
    {
        ret |= parseFnc(unixReleaseFile, "unix");
    }

    if (ret)
    {
        ret |= parseFnc(CENTOS_RELEASE_FILE, "centos");
    }
    else
    {
        for (const auto& platform : PLATFORMS_RELEASE_FILES)
        {
            if (parseFnc(platform.second, platform.first))
            {
                ret = true;
                break;
            }
        }
    }

    return ret;
}

nlohmann::json SysInfo::getOsInfo() const
{
    nlohmann::json ret;

    struct utsname uts
    {
    };

    if (!GetOsInfoFromFiles(ret))
    {
        ret["os_name"] = "Linux";
        ret["os_platform"] = "linux";
        ret["os_version"] = UNKNOWN_VALUE;
    }

    if (uname(&uts) >= 0)
    {
        ret["sysname"] = uts.sysname;
        ret["hostname"] = uts.nodename;
        ret["version"] = uts.version;
        ret["architecture"] = uts.machine;
        ret["release"] = uts.release;
    }

    return ret;
}

nlohmann::json SysInfo::getProcessesInfo() const
{
    nlohmann::json jsProcessesList {};

    getProcessesInfo(
        [&jsProcessesList](nlohmann::json& processInfo)
        {
            // Append the current json process object to the list of processes
            jsProcessesList.push_back(processInfo);
        });

    return jsProcessesList;
}

nlohmann::json SysInfo::getNetworks() const
{
    nlohmann::json networks;

    std::unique_ptr<ifaddrs, Utils::IfAddressSmartDeleter> interfacesAddress;
    std::map<std::string, std::vector<ifaddrs*>> networkInterfaces;
    Utils::getNetworks(interfacesAddress, networkInterfaces);

    for (const auto& interface : networkInterfaces)
    {
        nlohmann::json ifaddr {};

        for (auto addr : interface.second)
        {
            const auto networkInterfacePtr {FactoryNetworkFamilyCreator<OSPlatformType::LINUX>::create(
                std::make_shared<NetworkLinuxInterface>(addr))};

            if (networkInterfacePtr)
            {
                networkInterfacePtr->buildNetworkData(ifaddr);
            }
        }

        networks["iface"].push_back(ifaddr);
    }

    return networks;
}

ProcessInfo PortProcessInfo(const std::string& procPath, const std::deque<int64_t>& inodes)
{
    ProcessInfo ret;

    const auto fsWrapper = std::make_unique<file_system::FileSystemWrapper>();

    auto getProcessName = [](const std::string& filePath) -> std::string
    {
        // Get stat file content.
        std::string processInfo {EMPTY_VALUE};
        const auto fileIoWrapper = std::make_unique<file_io::FileIOUtils>();
        const std::string statContent {fileIoWrapper->getFileContent(filePath)};

        const auto openParenthesisPos {statContent.find('(')};
        const auto closeParenthesisPos {statContent.find(')')};

        if (openParenthesisPos != std::string::npos && closeParenthesisPos != std::string::npos)
        {
            processInfo = statContent.substr(openParenthesisPos + 1, closeParenthesisPos - openParenthesisPos - 1);
        }

        return processInfo;
    };

    auto findInode = [](const std::string& filePath) -> int64_t
    {
        constexpr size_t MAX_LENGTH {256};
        char buffer[MAX_LENGTH] = "";

        if (-1 == readlink(filePath.c_str(), buffer, MAX_LENGTH - 1))
        {
            throw std::system_error(errno, std::system_category(), "readlink");
        }

        // ret format is "socket:[<num>]".
        const std::string bufferStr {buffer};
        const auto openBracketPos {bufferStr.find('[')};
        const auto closeBracketPos {bufferStr.find(']')};
        const auto match {bufferStr.substr(openBracketPos + 1, closeBracketPos - openBracketPos - 1)};

        return std::stoll(match);
    };

    if (fsWrapper->exists(procPath) && fsWrapper->is_directory(procPath))
    {
        const std::vector<std::filesystem::path> procFiles = fsWrapper->list_directory(procPath);

        // Iterate proc directory.
        for (const auto& procFile : procFiles)
        {
            const auto procFileName = procFile.filename().string();
            // Only directories that represent a PID are inspected.
            std::string procFilePath {procPath + "/"};
            procFilePath += procFileName;

            if (Utils::isNumber(procFileName) && fsWrapper->exists(procFilePath) &&
                fsWrapper->is_directory(procFilePath))
            {
                // Only fd directory is inspected.
                const std::string pidFilePath {procFilePath + "/fd"};

                if (fsWrapper->exists(procFilePath) && fsWrapper->is_directory(procFilePath))
                {
                    const std::vector<std::filesystem::path> fdFiles = fsWrapper->list_directory(pidFilePath);

                    // Iterate fd directory.
                    for (const auto& fdFile : fdFiles)
                    {
                        const auto fdFileName = fdFile.filename().string();
                        // Only sysmlinks that represent a socket are read.
                        std::string fdFilePath {pidFilePath + "/"};
                        fdFilePath += fdFileName;

                        if (!Utils::startsWith(fdFileName, ".") && fsWrapper->exists(fdFilePath) &&
                            fsWrapper->is_socket(fdFilePath))
                        {
                            try
                            {
                                int64_t inode {findInode(fdFilePath)};

                                if (std::any_of(
                                        inodes.cbegin(), inodes.cend(), [&](const auto it) { return it == inode; }))
                                {
                                    const std::string statPath {procFilePath + "/" + "stat"};
                                    const std::string processName = getProcessName(statPath);
                                    const int32_t pid {std::stoi(procFileName)};

                                    ret.emplace(std::make_pair(inode, std::make_pair(pid, processName)));
                                }
                            }
                            catch (const std::exception& e)
                            {
                                std::cerr << "Error: " << e.what() << '\n';
                            }
                        }
                    }
                }
            }
        }
    }

    return ret;
}

nlohmann::json SysInfo::getPorts() const
{
    nlohmann::json ports;
    std::deque<int64_t> inodes;

    for (const auto& portType : PORTS_TYPE)
    {
        const auto fileIoWrapper = std::make_unique<file_io::FileIOUtils>();
        const auto fileContent {fileIoWrapper->getFileContent(WM_SYS_NET_DIR + portType.second)};
        auto rows {Utils::split(fileContent, '\n')};
        auto fileBody {false};

        for (auto& row : rows)
        {
            nlohmann::json port {};

            try
            {
                if (fileBody)
                {
                    row = Utils::Trim(row);
                    Utils::replaceAll(row, "\t", " ");
                    Utils::replaceAll(row, "  ", " ");
                    std::make_unique<PortImpl>(std::make_shared<LinuxPortWrapper>(portType.first, row))
                        ->buildPortData(port);
                    inodes.push_back(port.at("inode"));
                    ports.push_back(std::move(port));
                }

                fileBody = true;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error while parsing port: " << e.what() << '\n';
            }
        }
    }

    if (!inodes.empty())
    {
        ProcessInfo ret = PortProcessInfo(WM_SYS_PROC_DIR, inodes);

        for (auto& port : ports)
        {
            try
            {
                auto portInode = port.at("inode");

                if (ret.find(portInode) != ret.end())
                {
                    const std::pair<int32_t, std::string> processInfoPair = ret.at(portInode);
                    port["pid"] = processInfoPair.first;
                    port["process"] = processInfoPair.second;
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error while parsing pid and process from ports: " << e.what() << '\n';
            }
        }
    }

    return ports;
}

void SysInfo::getProcessesInfo(const std::function<void(nlohmann::json&)>& callback) const
{

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    const SysInfoProcessesTable spProcTable {openproc(PROC_FILLMEM | PROC_FILLSTAT | PROC_FILLSTATUS | PROC_FILLARG |
                                                      PROC_FILLGRP | PROC_FILLUSR | PROC_FILLCOM | PROC_FILLENV)};

    SysInfoProcess spProcInfo {readproc(spProcTable.get(), nullptr)};

    while (nullptr != spProcInfo)
    {
        // Get process information object and push it to the caller
        auto processInfo = GetProcessInfo(spProcInfo);
        callback(processInfo);
        spProcInfo.reset(readproc(spProcTable.get(), nullptr));
    }
}

void SysInfo::getPackages(const std::function<void(nlohmann::json&)>& callback) const
{
    FactoryPackagesCreator::getPackages(callback);
    const std::map<std::string, std::set<std::string>> searchPaths = {{"PYPI", UNIX_PYPI_DEFAULT_BASE_DIRS},
                                                                      {"NPM", UNIX_NPM_DEFAULT_BASE_DIRS}};
    ModernFactoryPackagesCreator::getPackages(searchPaths, callback);
}

nlohmann::json SysInfo::getHotfixes() const
{
    // Currently not supported for this OS.
    return {};
}
