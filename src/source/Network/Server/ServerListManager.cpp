//////////////////////////////////////////////////////////////////////
// ServerListManager.cpp: implementation of the CServerListManager class.
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ServerListManager.h"
#include "ServerListScript.h"
#include "I18N/All.h"
#include "Core/Text/Cp949.h"
#include "Core/Utilities/Log/MuLogger.h"

#include <cstdint>
#include <string>
#include <vector>

namespace
{

static_assert(Network::ServerList::kServerNameBytes == SLM_MAX_SERVER_NAME_LENGTH);
static_assert(Network::ServerList::kSeason6NonPvpCount == SLM_MAX_SERVER_COUNT);

void ReportMissingServerList()
{
    wchar_t szMessage[256];
    ::mu_swprintf(szMessage, L"Data\\Local\\ServerList.bmd file not found.\r\n");
    g_ErrorReport.Write(szMessage);
    ::MessageBox(g_hWnd, szMessage, NULL, MB_OK);
    ::PostMessage(g_hWnd, WM_DESTROY, 0, 0);
}

bool ReadServerListBytes(FILE* file, std::vector<std::uint8_t>& bytes, std::string& error)
{
    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        error = "could not seek ServerList.bmd";
        return false;
    }

    const long fileSize = std::ftell(file);
    if (fileSize < 0)
    {
        error = "could not read ServerList.bmd size";
        return false;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0)
    {
        error = "could not rewind ServerList.bmd";
        return false;
    }

    bytes.resize(static_cast<std::size_t>(fileSize));
    if (fileSize == 0)
    {
        return true;
    }

    const std::size_t readCount = std::fread(bytes.data(), 1, bytes.size(), file);
    if (readCount != bytes.size())
    {
        error = "could not read ServerList.bmd";
        return false;
    }
    return true;
}

bool UsesCp949(Network::ServerList::ScriptFormat format)
{
    return format == Network::ServerList::ScriptFormat::Season21 ||
           format == Network::ServerList::ScriptFormat::Season21WithoutIndex;
}

std::wstring ServerTextToWide(const std::string& text, bool cp949)
{
    if (text.empty())
    {
        return {};
    }
    if (cp949)
    {
        return Core::Text::WideFromCp949(text);
    }

    std::vector<wchar_t> wide(text.size() + 1, L'\0');
    const int written =
        CMultiLanguage::ConvertFromUtf8(wide.data(), text.data(), static_cast<int>(text.size()));
    if (written <= 0)
    {
        return {};
    }
    return std::wstring(wide.data());
}

void CopyGroupName(wchar_t* destination, const std::string& name, bool cp949)
{
    for (int i = 0; i <= SLM_MAX_SERVER_NAME_LENGTH; ++i)
    {
        destination[i] = L'\0';
    }
    const std::wstring wide = ServerTextToWide(name, cp949);
    if (wide.empty())
    {
        return;
    }
    const std::size_t count = wide.size() < static_cast<std::size_t>(SLM_MAX_SERVER_NAME_LENGTH)
                                  ? wide.size()
                                  : static_cast<std::size_t>(SLM_MAX_SERVER_NAME_LENGTH);
    for (std::size_t i = 0; i < count; ++i)
    {
        destination[i] = wide[i];
    }
}

void StoreServerGroups(ServerListScriptMap& groups, const Network::ServerList::ScriptDocument& document)
{
    groups.clear();
    for (const Network::ServerList::ScriptRecord& record : document.records)
    {
        SServerGroupInfo info{};
        const bool cp949 = UsesCp949(document.format);
        CopyGroupName(info.m_szName, record.name, cp949);
        info.m_byPos = record.position;
        info.m_bySequence = record.sequence;
        for (int i = 0; i < SLM_MAX_SERVER_COUNT; ++i)
        {
            info.m_abyNonPVP[i] = record.nonPvp[static_cast<std::size_t>(i)];
        }
        info.m_strDescript = ServerTextToWide(record.description, cp949);
        groups.insert(std::make_pair(record.index, info));
    }
}

} // namespace

CServerListManager::CServerListManager()
{
    m_iTotalServer = 0;
    m_szSelectServerName[0] = '\0';
    m_iSelectServerIndex = -1;
}

CServerListManager::~CServerListManager()
{
    Release();
}

CServerListManager* CServerListManager::GetInstance()
{
    static CServerListManager s_ServerListManager;
    return &s_ServerListManager;
}

void CServerListManager::Release()
{
    auto iterServerGroup = m_mapServerGroup.begin();
    for (; iterServerGroup != m_mapServerGroup.end(); iterServerGroup++)
    {
        delete iterServerGroup->second;
    }

    m_mapServerGroup.clear();
    m_iTotalServer = 0;
}

void CServerListManager::LoadServerListScript()
{
    FILE* file = ::_wfopen(L"Data\\Local\\ServerList.bmd", L"rb");
    if (file == NULL)
    {
        ReportMissingServerList();
        return;
    }

    std::vector<std::uint8_t> bytes;
    std::string readError;
    const bool read = ReadServerListBytes(file, bytes, readError);
    std::fclose(file);
    if (!read)
    {
        MU_LOG_ERROR(mu::log::Get("network"), "{}", readError);
        return;
    }

    const Network::ServerList::ScriptDocument document = Network::ServerList::ParseScript(bytes.data(), bytes.size());
    if (document.format == Network::ServerList::ScriptFormat::None)
    {
        MU_LOG_ERROR(mu::log::Get("network"), "ServerList.bmd ({} bytes) was not loaded: {}", bytes.size(),
                     document.error);
        return;
    }

    StoreServerGroups(m_mapServerListScript, document);
    MU_LOG_INFO(mu::log::Get("network"), "Loaded {} server groups from ServerList.bmd ({})", document.records.size(),
                Network::ServerList::ScriptFormatName(document.format));
}

const SServerGroupInfo* CServerListManager::GetServerGroupInfoInScript(WORD wServerGroupIndex)
{
    ServerListScriptMap::const_iterator iter = m_mapServerListScript.find(wServerGroupIndex);
    if (iter == m_mapServerListScript.end())
        return NULL;

    return &(iter->second);
}

void CServerListManager::InsertServerGroup(int iConnectIndex, int iServerPercent)
{
    CServerGroup* pServerGroup = NULL;

    auto iterServerGroup = m_mapServerGroup.begin();

    bool bEqual = false;
    while (iterServerGroup != m_mapServerGroup.end())
    {
        if ((iterServerGroup->second)->m_iServerIndex == iConnectIndex / MAX_SERVER_PER_GROUP)
        {
            bEqual = true;
            break;
        }

        iterServerGroup++;
    }

    if (bEqual == true)
    {
        pServerGroup = iterServerGroup->second;
    }
    else
    {
        pServerGroup = new CServerGroup;

        if (MakeServerGroup(iConnectIndex / MAX_SERVER_PER_GROUP, pServerGroup) == false)
            return;

        m_mapServerGroup.insert(type_mapServerGroup::value_type(pServerGroup->m_iSequence, pServerGroup));
    }

    InsertServer(pServerGroup, iConnectIndex, iServerPercent);

    m_iterServerGroup = m_mapServerGroup.begin();
}

bool CServerListManager::MakeServerGroup(IN int iServerGroupIndex, OUT CServerGroup* pServerGroup)
{
    const SServerGroupInfo* pServerGroupInfo = GetServerGroupInfoInScript(iServerGroupIndex);
    if (NULL == pServerGroupInfo)
        return false;

    ::wcscpy(pServerGroup->m_szName, pServerGroupInfo->m_szName);
    Network::ServerList::CopyBoundedWide(pServerGroup->m_szDescription, MAX_TEXT_LENGTH,
                                         pServerGroupInfo->m_strDescript);
    pServerGroup->m_iSequence = (int)pServerGroupInfo->m_bySequence;
    pServerGroup->m_iWidthPos = (int)pServerGroupInfo->m_byPos;
    pServerGroup->m_iServerIndex = iServerGroupIndex;
    pServerGroup->m_bPvPServer = true;
    int i;
    for (i = 0; i < SLM_MAX_SERVER_COUNT; ++i)
    {
        pServerGroup->m_abyNonPvpServer[i] = pServerGroupInfo->m_abyNonPVP[i];
        if (0x01 & pServerGroup->m_abyNonPvpServer[i])
            pServerGroup->m_bPvPServer = false;
    }
    for (; i < MAX_SERVER_PER_GROUP; ++i)
        pServerGroup->m_abyNonPvpServer[i] = 0;

    return true;
}

void CServerListManager::InsertServer(CServerGroup* pServerGroup, int iConnectIndex, int iServerPercent)
{
    auto* pServerInfo = new CServerInfo;
    pServerInfo->m_iSequence = pServerGroup->GetServerSize();
    pServerInfo->m_iIndex = (iConnectIndex % MAX_SERVER_PER_GROUP) + 1;
    pServerInfo->m_iConnectIndex = iConnectIndex;
    pServerInfo->m_iPercent = iServerPercent;
    pServerInfo->m_byNonPvP = pServerGroup->m_abyNonPvpServer[pServerInfo->m_iIndex - 1];

    int iTextIndex;
    if (iServerPercent >= 128)
    {
        iTextIndex = 560;
    }
    else if (iServerPercent >= 100)
    {
        iTextIndex = 561;
    }
    else
    {
        iTextIndex = 562;
    }

    switch (pServerInfo->m_byNonPvP)
    {
    case 0:
        mu_swprintf(pServerInfo->m_bName, L"%ls-%d %ls", pServerGroup->m_szName,
            pServerInfo->m_iIndex, I18N::Game::Lookup(iTextIndex));
        break;

    case 1:
        mu_swprintf(pServerInfo->m_bName, L"%ls-%d(Non-PVP) %ls", pServerGroup->m_szName,
            pServerInfo->m_iIndex, I18N::Game::Lookup(iTextIndex));
        break;

    case 2:
        mu_swprintf(pServerInfo->m_bName, L"%ls-%d(Gold PVP) %ls", pServerGroup->m_szName,
            pServerInfo->m_iIndex, I18N::Game::Lookup(iTextIndex));
        break;

    case 3:
        mu_swprintf(pServerInfo->m_bName, L"%ls-%d(Gold) %ls", pServerGroup->m_szName,
            pServerInfo->m_iIndex, I18N::Game::Lookup(iTextIndex));
        break;
    }

    pServerGroup->InsertServerInfo(pServerInfo);
}

int CServerListManager::GetServerGroupSize()
{
    return m_mapServerGroup.size();
}

void CServerListManager::SetFirst()
{
    m_iterServerGroup = m_mapServerGroup.begin();
}

bool CServerListManager::GetNext(OUT CServerGroup*& pServerGroup)
{
    if (m_iterServerGroup == m_mapServerGroup.end())
    {
        pServerGroup = NULL;

        return false;
    }

    pServerGroup = m_iterServerGroup->second;

    m_iterServerGroup++;

    return true;
}

CServerGroup* CServerListManager::GetServerGroupByBtnPos(int iBtnPos)
{
    auto iterServerGroup = m_mapServerGroup.begin();

    while (iterServerGroup != m_mapServerGroup.end())
    {
        if (iterServerGroup->second->m_iBtnPos == iBtnPos)
            return iterServerGroup->second;

        iterServerGroup++;
    }

    return NULL;
}

void CServerListManager::SetSelectServerInfo(wchar_t* pszName, int iIndex, BYTE byNonPvP)
{
    wcscpy(m_szSelectServerName, pszName);
    m_iSelectServerIndex = iIndex;
    m_byNonPvP = byNonPvP;
}

wchar_t* CServerListManager::GetSelectServerName()
{
    return m_szSelectServerName;
}

int CServerListManager::GetSelectServerIndex()
{
    return m_iSelectServerIndex;
}


BYTE CServerListManager::GetNonPVPInfo()
{
    return m_byNonPvP;
}

bool CServerListManager::IsNonPvP()
{
    return bool(0x01 & GetNonPVPInfo());
}

void CServerListManager::SetTotalServer(int iTotalServer)
{
    m_iTotalServer = iTotalServer;
}

int CServerListManager::GetTotalServer()
{
    return m_iTotalServer;
}