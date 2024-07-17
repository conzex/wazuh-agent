/*
 * Wazuh Inventory
 * Copyright (C) 2015, Wazuh Inc.
 * October 8, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */
#ifndef _INVENTORY_HPP
#define _INVENTORY_HPP
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <memory>
#include "sysInfoInterface.h"
#include "commonDefs.h"
#include "dbsync.hpp"
#include "rsync.hpp"
#include "inventoryNormalizer.h"
#include "inventory.h"

// Define EXPORTED for any platform
#ifdef _WIN32
#ifdef WIN_EXPORT
#define EXPORTED __declspec(dllexport)
#else
#define EXPORTED __declspec(dllimport)
#endif
#elif __GNUC__ >= 4
#define EXPORTED __attribute__((visibility("default")))
#else
#define EXPORTED
#endif

class EXPORTED Inventory final
{
    public:
        static Inventory& instance()
        {
            static Inventory s_instance;
            return s_instance;
        }

        void init(const std::shared_ptr<ISysInfo>& spInfo,
                  const std::function<void(const std::string&)> reportDiffFunction,
                  const std::function<void(const std::string&)> reportSyncFunction,
                  const std::function<void(const modules_log_level_t, const std::string&)> logFunction,
                  const std::string& dbPath,
                  const std::string& normalizerConfigPath,
                  const std::string& normalizerType,
                  const unsigned int inverval = 3600ul,
                  const bool scanOnStart = true,
                  const bool hardware = true,
                  const bool os = true,
                  const bool network = true,
                  const bool packages = true,
                  const bool ports = true,
                  const bool portsAll = true,
                  const bool processes = true,
                  const bool hotfixes = true,
                  const bool notifyOnFirstScan = false);

        void destroy();
        void push(const std::string& data);
    private:
        Inventory();
        ~Inventory() = default;
        Inventory(const Inventory&) = delete;
        Inventory& operator=(const Inventory&) = delete;

        std::string getCreateStatement() const;
        nlohmann::json getOSData();
        nlohmann::json getHardwareData();
        nlohmann::json getNetworkData();
        nlohmann::json getPortsData();

        void registerWithRsync();
        void updateChanges(const std::string& table,
                           const nlohmann::json& values);
        void notifyChange(ReturnTypeCallback result,
                          const nlohmann::json& data,
                          const std::string& table);
        void scanHardware();
        void scanOs();
        void scanNetwork();
        void scanPackages();
        void scanHotfixes();
        void scanPorts();
        void scanProcesses();
        void syncOs();
        void syncHardware();
        void syncNetwork();
        void syncPackages();
        void syncHotfixes();
        void syncPorts();
        void syncProcesses();
        void scan();
        void sync();
        void syncLoop(std::unique_lock<std::mutex>& lock);
        void syncAlgorithm();
        std::shared_ptr<ISysInfo>                                               m_spInfo;
        std::function<void(const std::string&)>                                 m_reportDiffFunction;
        std::function<void(const std::string&)>                                 m_reportSyncFunction;
        std::function<void(const modules_log_level_t, const std::string&)>      m_logFunction;
        std::chrono::seconds                                                    m_intervalValue;
        std::chrono::seconds                                                    m_currentIntervalValue;
        bool                                                                    m_scanOnStart;
        bool                                                                    m_hardware;
        bool                                                                    m_os;
        bool                                                                    m_network;
        bool                                                                    m_packages;
        bool                                                                    m_ports;
        bool                                                                    m_portsAll;
        bool                                                                    m_processes;
        bool                                                                    m_hotfixes;
        bool                                                                    m_stopping;
        bool                                                                    m_notify;
        std::unique_ptr<DBSync>                                                 m_spDBSync;
        std::unique_ptr<RemoteSync>                                             m_spRsync;
        std::condition_variable                                                 m_cv;
        std::mutex                                                              m_mutex;
        std::unique_ptr<InvNormalizer>                                          m_spNormalizer;
        std::string                                                             m_scanTime;
        std::chrono::seconds                                                    m_lastSyncMsg;
};


#endif //_INVENTORY_HPP
