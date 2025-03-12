/*
 * Wazuh SysInfo
 * Copyright (C) 2015, Wazuh Inc.
 * October 7, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */
#include "network/networkBSDWrapper.h"
#include "network/networkFamilyDataAFactory.h"
#include "networkUnixHelper.hpp"
#include "stringHelper.h"
#include "sysInfo.hpp"
#include <sys/sysctl.h>
#include <sys/types.h>

nlohmann::json SysInfo::getNetworks() const
{
    nlohmann::json networks;

    std::unique_ptr<ifaddrs, Utils::IfAddressSmartDeleter> interfacesAddress;
    std::map<std::string, std::vector<ifaddrs*>> networkInterfaces;
    Utils::getNetworks(interfacesAddress, networkInterfaces);

    for (const auto& interface : networkInterfaces)
    {
        nlohmann::json ifaddr {};

        for (const auto addr : interface.second)
        {
            const auto networkInterfacePtr {FactoryNetworkFamilyCreator<OSPlatformType::BSDBASED>::create(
                std::make_shared<NetworkBSDInterface>(addr))};

            if (networkInterfacePtr)
            {
                networkInterfacePtr->buildNetworkData(ifaddr);
            }
        }

        networks["iface"].push_back(ifaddr);
    }

    return networks;
}
