#pragma once

#include "Core/Platform/WinCompat.h"

#include <cstring>

// Same mixing function the client has always used for Local *.bmd checksums.
inline DWORD GenerateCheckSum2(const BYTE* pbyBuffer, DWORD dwSize, WORD wKey)
{
    DWORD dwKey = (DWORD)wKey;
    DWORD dwResult = dwKey << 9;
    if (dwSize < 4)
    {
        return dwResult;
    }

    for (DWORD dwChecked = 0; dwChecked <= dwSize - 4; dwChecked += 4)
    {
        DWORD dwTemp;
        memcpy(&dwTemp, pbyBuffer + dwChecked, sizeof(DWORD));

        switch ((dwChecked / 4 + wKey) % 2)
        {
        case 0:
            dwResult ^= dwTemp;
            break;
        case 1:
            dwResult += dwTemp;
            break;
        }
        if (0 == (dwChecked % 16))
        {
            dwResult ^= ((dwKey + dwResult) >> ((dwChecked / 4) % 8 + 1));
        }
    }

    return (dwResult);
}
