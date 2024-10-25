#pragma once

#include <module_command/command_entry.hpp>

#include <boost/asio/awaitable.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace centralized_configuration
{
    class CentralizedConfiguration
    {
    public:
        using SetGroupIdFunctionType = std::function<void(const std::vector<std::string>& groupList)>;
        using GetGroupIdFunctionType = std::function<std::vector<std::string>()>;
        using DownloadGroupFilesFunctionType =
            std::function<bool(const std::string& group, const std::string& dstFilePath)>;

        /**
         * @brief Executes a command for the centralized configuration system.
         * @param command A string containing a JSON command to execute.
         * @return An awaitable with the result of executing the command, either success or failure.
         */
        boost::asio::awaitable<module_command::CommandExecutionResult> ExecuteCommand(std::string command);

        /**
         * @brief Sets the function to assign group IDs.
         * @details The "set-group" commands requires such function to be set.
         * @param setGroupIdFunction A function to set group IDs.
         */
        void SetGroupIdFunction(SetGroupIdFunctionType setGroupIdFunction);

        /**
         * @brief Sets the function to retrieve group IDs.
         * @details The "update-group" commands requires such function to be set.
         * @param getGroupIdFunction A function to get group IDs.
         */
        void GetGroupIdFunction(GetGroupIdFunctionType getGroupIdFunction);

        /**
         * @brief Sets the function to download group configuration files.
         * @details Configures how and where to download group configuration files.
         * These will be used to set and update the Agent groups via the "set-group" and "update-group"
         * commands.
         * @param downloadGroupFilesFunction A function to download files for a given group ID.
         */
        void SetDownloadGroupFilesFunction(DownloadGroupFilesFunctionType downloadGroupFilesFunction);

    private:
        SetGroupIdFunctionType m_setGroupIdFunction;
        GetGroupIdFunctionType m_getGroupIdFunction;
        DownloadGroupFilesFunctionType m_downloadGroupFilesFunction;
    };
} // namespace centralized_configuration
