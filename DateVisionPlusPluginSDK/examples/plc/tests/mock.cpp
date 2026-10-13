#include "mock.h"

#include <cstdint>
#include <cstring>

#ifdef _WIN32
#define MOCK_EXPORT extern "C" __declspec(dllexport)
#define MOCK_CALL __stdcall
#else
#define MOCK_EXPORT extern "C" __attribute__((visibility("default")))
#define MOCK_CALL
#endif

namespace {
MockStats stats = {};
bool connected = false;
bool failNextCreate = false;
int statusResult = 0;
int ioResult = 0;
int connectResult = 0;

int byteSize(int area, int wordLen)
{
    if (area == 0x1C || area == 0x1D)
        return 2;
    switch (wordLen) {
    case 0x01:
    case 0x02:
    case 0x03: return 1;
    case 0x04:
    case 0x05:
    case 0x1C:
    case 0x1D: return 2;
    case 0x06:
    case 0x07:
    case 0x08: return 4;
    default: return 0;
    }
}
}

MOCK_EXPORT void MOCK_CALL MockReset()
{
    stats = {};
    connected = false;
    failNextCreate = false;
    statusResult = ioResult = connectResult = 0;
}
MOCK_EXPORT MockStats MOCK_CALL MockStatsSnapshot() { return stats; }
MOCK_EXPORT void MOCK_CALL MockFailNextCreate() { failNextCreate = true; }
MOCK_EXPORT void MOCK_CALL MockStatusError(int result) { statusResult = result; }
MOCK_EXPORT void MOCK_CALL MockIOError(int result) { ioResult = result; }
MOCK_EXPORT void MOCK_CALL MockConnectError(int result)
{
    connectResult = result;
    connected = false;
}

MOCK_EXPORT std::uintptr_t MOCK_CALL Cli_Create()
{
    ++stats.creates;
    if (failNextCreate) {
        failNextCreate = false;
        return 0;
    }
    return 1;
}
MOCK_EXPORT void MOCK_CALL Cli_Destroy(std::uintptr_t *client)
{
    ++stats.destroys;
    *client = 0;
}
MOCK_EXPORT int MOCK_CALL Cli_ConnectTo(std::uintptr_t client, const char *, int rack, int slot)
{
    ++stats.connects;
    stats.rack = rack;
    stats.slot = slot;
    if (client == 0 || connectResult != 0)
        return connectResult != 0 ? connectResult : -1;
    connected = true;
    return 0;
}
MOCK_EXPORT int MOCK_CALL Cli_Disconnect(std::uintptr_t)
{
    ++stats.disconnects;
    connected = false;
    return 0;
}
MOCK_EXPORT int MOCK_CALL Cli_GetConnected(std::uintptr_t, int *result)
{
    ++stats.statusQueries;
    *result = connected ? 1 : 0;
    return statusResult;
}
MOCK_EXPORT int MOCK_CALL Cli_WriteArea(std::uintptr_t, int area, int, int, int amount, int wordLen, void *data)
{
    ++stats.writes;
    stats.amount = amount;
    stats.wordLen = wordLen;
    stats.value = *static_cast<unsigned char *>(data);
    if (area != 0x1C && area != 0x1D && wordLen == 0x01 && amount > 1)
        return 2;
    return ioResult;
}
MOCK_EXPORT int MOCK_CALL Cli_ReadArea(std::uintptr_t, int area, int, int, int amount, int wordLen, void *data)
{
    ++stats.reads;
    stats.amount = amount;
    stats.wordLen = wordLen;
    if (area != 0x1C && area != 0x1D && wordLen == 0x01 && amount > 1)
        return 2;
    if (ioResult != 0)
        return ioResult;
    std::memset(data, 0x5A, amount * byteSize(area, wordLen));
    return 0;
}
MOCK_EXPORT int MOCK_CALL Cli_ErrorText(int, char *text, int capacity)
{
    if (capacity > 0)
        text[0] = '\0';
    return 0;
}
