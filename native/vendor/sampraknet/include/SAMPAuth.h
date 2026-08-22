#include "Export.h"

RAK_DLL_EXPORT extern char AuthKeyTable[512][2][128];

RAK_DLL_EXPORT namespace SAMPAuth {
    struct KeyPair {
        const char* serverKey;
        const char* clientKey;
    };
    KeyPair GetAuthKeyPair(unsigned int index = 0xFFFFFFFF);
    const char* GetClientKey(const char* serverKey);
    const char* GetServerKey(const char* clientKey);
}