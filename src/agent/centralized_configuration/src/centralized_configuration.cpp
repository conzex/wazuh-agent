#include <centralized_configuration.hpp>
#include <config.h>
#include <logger.hpp>

#include <filesystem>

namespace centralized_configuration
{
    boost::asio::awaitable<module_command::CommandExecutionResult> CentralizedConfiguration::ExecuteCommand(
        const std::string command,       // NOLINT(performance-unnecessary-value-param)
        const nlohmann::json parameters) // NOLINT(performance-unnecessary-value-param)
    {
        try
        {
            std::vector<std::string> groupIds {};
            std::string messageOnSuccess {};

            if (command == "set-group")
            {
                if (m_setGroupIdFunction && m_downloadGroupFilesFunction)
                {
                    if (parameters.empty())
                    {
                        LogWarn("Group set failed, no group list");
                        co_return module_command::CommandExecutionResult {
                            module_command::Status::FAILURE,
                            "CentralizedConfiguration group set failed, no group list"};
                    }

                    groupIds = parameters[0].get<std::vector<std::string>>();
                    messageOnSuccess = "CentralizedConfiguration group set";

                    m_setGroupIdFunction(groupIds);
                    m_saveGroupIdFunction();

                    try
                    {
                        std::filesystem::path sharedDirPath(config::DEFAULT_SHARED_CONFIG_PATH);

                        if (std::filesystem::exists(sharedDirPath) && std::filesystem::is_directory(sharedDirPath))
                        {
                            for (const auto& entry : std::filesystem::directory_iterator(sharedDirPath))
                            {
                                std::filesystem::remove_all(entry);
                            }
                        }
                    }
                    catch (const std::filesystem::filesystem_error& e)
                    {
                        LogWarn("Error while cleaning the shared directory {}.", e.what());
                        co_return module_command::CommandExecutionResult {
                            module_command::Status::FAILURE,
                            "CentralizedConfiguration group set failed, error while cleaning the shared directory"};
                    }
                }
                else
                {
                    LogWarn("Group set failed, no function set");
                    co_return module_command::CommandExecutionResult {
                        module_command::Status::FAILURE, "CentralizedConfiguration group set failed, no function set"};
                }
            }
            else if (command == "update-group")
            {
                if (m_getGroupIdFunction && m_downloadGroupFilesFunction)
                {
                    groupIds = m_getGroupIdFunction();
                    messageOnSuccess = "CentralizedConfiguration group updated";
                }
                else
                {
                    LogWarn("Group update failed, no function set");
                    co_return module_command::CommandExecutionResult {
                        module_command::Status::FAILURE,
                        "CentralizedConfiguration group update failed, no function set"};
                }
            }
            else
            {
                LogWarn("CentralizedConfiguration command not recognized");
                co_return module_command::CommandExecutionResult {module_command::Status::FAILURE,
                                                                  "CentralizedConfiguration command not recognized"};
            }

            for (const auto& groupId : groupIds)
            {
                const std::filesystem::path tmpGroupFile = std::filesystem::temp_directory_path() / (groupId + ".conf");
                m_downloadGroupFilesFunction(groupId, tmpGroupFile.string());
                if (!m_validateFileFunction(tmpGroupFile))
                {
                    LogWarn("Validate file failed, invalid group file received: {}", tmpGroupFile.string());
                    co_return module_command::CommandExecutionResult {
                        module_command::Status::FAILURE,
                        "CentralizedConfiguration validate file failed, invalid file received."};
                }

                const std::filesystem::path destGroupFile =
                    std::filesystem::path(config::DEFAULT_SHARED_CONFIG_PATH) / (groupId + ".conf");

                try
                {
                    std::filesystem::create_directories(destGroupFile.parent_path());
                    std::filesystem::rename(tmpGroupFile, destGroupFile);
                }
                catch (const std::filesystem::filesystem_error& e)
                {
                    LogWarn("Failed to move file to destination: {}. Error: {}", destGroupFile.string(), e.what());
                    co_return module_command::CommandExecutionResult {module_command::Status::FAILURE,
                                                                      "Failed to move shared file to destination."};
                }
            }

            // TODO apply configuration

            co_return module_command::CommandExecutionResult {module_command::Status::SUCCESS, messageOnSuccess};
        }
        catch (const nlohmann::json::exception&)
        {
            LogWarn("CentralizedConfiguration error while parsing parameters");
            co_return module_command::CommandExecutionResult {
                module_command::Status::FAILURE, "CentralizedConfiguration error while parsing parameters"};
        }
    }

    void CentralizedConfiguration::SetGroupIdFunction(SetGroupIdFunctionType setGroupIdFunction)
    {
        m_setGroupIdFunction = std::move(setGroupIdFunction);
    }

    void CentralizedConfiguration::GetGroupIdFunction(GetGroupIdFunctionType getGroupIdFunction)
    {
        m_getGroupIdFunction = std::move(getGroupIdFunction);
    }

    void CentralizedConfiguration::SaveGroupIdFunction(SaveGroupIdFunctionType saveGroupIdFunction)
    {
        m_saveGroupIdFunction = std::move(saveGroupIdFunction);
    }

    void
    CentralizedConfiguration::SetDownloadGroupFilesFunction(DownloadGroupFilesFunctionType downloadGroupFilesFunction)
    {
        m_downloadGroupFilesFunction = std::move(downloadGroupFilesFunction);
    }

    void CentralizedConfiguration::ValidateFileFunction(ValidateFileFunctionType validateFileFunction)
    {
        m_validateFileFunction = std::move(validateFileFunction);
    }
} // namespace centralized_configuration
