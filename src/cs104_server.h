#ifndef CS104_SERVER_H
#define CS104_SERVER_H

#include <napi.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>

extern "C" {
#include "cs104_slave.h"
#include "hal_thread.h"
#include "hal_time.h"
}

class IEC104Server : public Napi::ObjectWrap<IEC104Server> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    IEC104Server(const Napi::CallbackInfo& info);
    virtual ~IEC104Server();

    Napi::Value SendCommandsAsync(const Napi::CallbackInfo& info);

private:
    struct PendingBatch {
        Napi::Promise::Deferred deferred;
        std::atomic<bool> resolved{false};
        std::mutex mtx;
        uint64_t timeoutMs;
        int expectedConfirmations;   // количество отправленных ASDU
        int receivedConfirmations;   // количество полученных подтверждений

        PendingBatch(Napi::Env env, int expected)
            : deferred(Napi::Promise::Deferred::New(env)),
              resolved(false),
              timeoutMs(0),
              expectedConfirmations(expected),
              receivedConfirmations(0) {}
    };

    std::unordered_map<std::string, std::shared_ptr<PendingBatch>> pendingBatches; // batchId -> batch
    std::mutex pendingBatchesMutex;
    uint64_t batchCounter = 0;

    // Для каждого клиента храним ID текущего активного батча
    std::unordered_map<std::string, std::string> clientCurrentBatch;
    std::mutex clientBatchMutex;

    void startBatchTimeoutTimer(const std::string& batchId, std::shared_ptr<PendingBatch> batch, uint64_t timeoutMs);
    void resolveBatch(const std::string& batchId, bool success, const std::string& errorMsg = "");

    // ===== Существующие члены класса =====
    static Napi::FunctionReference constructor;
    CS104_Slave server;
    std::thread _thread;
    std::mutex connMutex;
    std::string serverID;
    std::map<int, CS101_ASDU> asduGroups;
    int cnt = 0;
    Napi::ThreadSafeFunction tsfn;
    bool running;
    bool started;
    std::map<IMasterConnection, std::string> clientConnections;
    std::map<std::string, int> ipConnectionCounts;
    std::map<std::string, CS104_RedundancyGroup> redundancyGroups;
    bool restrictIPs;
    CS104_ServerMode serverMode;

    // Статические обработчики
    static bool ConnectionRequestHandler(void *parameter, const char *ipAddress);
    static void ConnectionEventHandler(void *parameter, IMasterConnection connection, CS104_PeerConnectionEvent event);
    static bool RawMessageHandler(void *parameter, IMasterConnection connection, CS101_ASDU asdu);

    // Методы, доступные из JavaScript
    Napi::Value Start(const Napi::CallbackInfo& info);
    Napi::Value Stop(const Napi::CallbackInfo& info);
    Napi::Value SendCommands(const Napi::CallbackInfo& info);
    Napi::Value GetStatus(const Napi::CallbackInfo& info);
};

#endif // CS104_SERVER_H