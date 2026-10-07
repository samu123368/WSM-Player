#ifndef WSM_PLAYER_UPDATE_H
#define WSM_PLAYER_UPDATE_H
#include <stdint.h>
#include <string>
namespace PlayerUpdate {
enum State { Idle, Checking, Available, Current, Downloading, Verifying,
    Installing, Installed, Failed, Cancelled };
enum Error { NoError, InvalidSource, NetworkError, TransferError, ManifestError,
    SignatureError, DiskError, HashError, DolError, WorkerError, ReplaceError, RestartError };
enum NetworkStage { NoNetworkStage, InitializingNetwork, ResolvingHost,
    CreatingSocket, ConfiguringSocket, ConnectingServer };
struct Snapshot {
    State state;
    Error error;
    bool busy;
    uint32_t received, total, build;
    std::string version;
    NetworkStage networkStage;
    int networkResult;
    Snapshot() : state(Idle),error(NoError),busy(false),received(0),total(0),build(0),
        networkStage(NoNetworkStage),networkResult(0) {}
};
Snapshot Get();
bool Busy();
bool Check(const std::string &source, const std::string &directory);
bool Install();
void Cancel();
void Reset();
void RequestRestart();
void RestartFailed();
bool TakeRestartRequest();
const char *StatusText(State state);
const char *ErrorText(Error error);
const char *NetworkStageText(NetworkStage stage);
}
#endif
