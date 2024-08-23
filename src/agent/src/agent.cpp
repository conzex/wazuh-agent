#include <agent.hpp>

#include <http_client.hpp>
#include <message.hpp>
#include <message_queue_utils.hpp>
#include <signal_handler.hpp>

#include <memory>
#include <string>
#include <thread>

Agent::Agent(std::unique_ptr<ISignalHandler> signalHandler)
    : m_signalHandler(std::move(signalHandler))
    , m_communicator(std::make_unique<http_client::HttpClient>(),
                     m_agentInfo.GetUUID(),
                     m_agentInfo.GetKey(),
                     [this](std::string table, std::string key) -> std::string
                     { return m_configurationParser.GetConfig<std::string>(table, key); })
{
    m_taskManager.Start(std::thread::hardware_concurrency());
}

Agent::~Agent()
{
    m_taskManager.Stop();
}

void Agent::Run()
{
    m_taskManager.EnqueueTask(m_communicator.WaitForTokenExpirationAndAuthenticate());

    m_taskManager.EnqueueTask(m_communicator.GetCommandsFromManager(
        [this](const std::string& response) { pushCommandsToQueue(m_messageQueue, response); }));

    m_taskManager.EnqueueTask(m_communicator.StatefulMessageProcessingTask(
        [this]() { return getMessagesFromQueue(m_messageQueue, MessageType::STATEFUL); },
        [this]([[maybe_unused]] const std::string& response)
        { popMessagesFromQueue(m_messageQueue, MessageType::STATEFUL); }));

    m_taskManager.EnqueueTask(m_communicator.StatelessMessageProcessingTask(
        [this]() { return getMessagesFromQueue(m_messageQueue, MessageType::STATELESS); },
        [this]([[maybe_unused]] const std::string& response)
        { popMessagesFromQueue(m_messageQueue, MessageType::STATELESS); }));

    m_signalHandler->WaitForSignal();
}
