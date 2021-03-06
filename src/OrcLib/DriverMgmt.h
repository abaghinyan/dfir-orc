//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//

#pragma once

#include "OrcLib.h"

#pragma managed(push, off)

namespace Orc {

class LogFileWriter;

enum ServiceStatus
{
    Inexistent,
    Installed,
    PendingStart,
    Started,
    PendingStop,
    Stopped,
    Paused,
    PendingPause,
    PendingContinue
};

class DriverMgmt;

class DriverTermination;

class ORCLIB_API Driver : public std::enable_shared_from_this<Driver>
{
    friend class DriverMgmt;
    friend class DriverTermination;

public:
    Driver(logger pLog, std::shared_ptr<DriverMgmt> manager, std::wstring serviceName)
        : m_manager(std::move(manager))
        , m_strServiceName(std::move(serviceName))
        , _L_(std::move(pLog))
    {
    }

    const std::wstring& Name() const { return m_strServiceName; }

    HRESULT UnInstall();

    HRESULT Start();
    HRESULT Stop();

    ServiceStatus GetStatus();

private:
    HRESULT Install(const std::wstring& strX86DriverRef, const std::wstring& strX64DriverRef);

    logger _L_;
    std::wstring m_strServiceName;
    std::wstring m_strDriverRef;
    std::wstring m_strDriverFileName;
    bool m_bDeleteDriverOnClose = false;

    std::shared_ptr<DriverMgmt> m_manager;
};

class ORCLIB_API DriverMgmt : public std::enable_shared_from_this<DriverMgmt>
{
    friend class Driver;
    friend class DriverTermination;

public:
    DriverMgmt(logger pLog, const std::wstring& strComputerName = L"")
        : _L_(std::move(pLog))
        , m_strComputerName(strComputerName) {};

    DriverMgmt(DriverMgmt&& other) = default;

    HRESULT SetTemporaryDirectory(const std::wstring& strTempDir);

    std::shared_ptr<Driver> GetDriver(
        const std::wstring& strServiceName,
        const std::wstring& strX86DriverRef,
        const std::wstring& strX64DriverRef);

    ~DriverMgmt() { Disconnect(); }

private:
    static HRESULT
    InstallDriver(const logger& pLog, __in SC_HANDLE SchSCManager, __in LPCTSTR DriverName, __in LPCTSTR ServiceExe);
    static HRESULT RemoveDriver(const logger& pLog, __in SC_HANDLE SchSCManager, __in LPCTSTR DriverName);
    static HRESULT StartDriver(const logger& pLog, __in SC_HANDLE SchSCManager, __in LPCTSTR DriverName);
    static HRESULT StopDriver(const logger& pLog, __in SC_HANDLE SchSCManager, __in LPCTSTR DriverName);
    static HRESULT GetDriverStatus(const logger& pLog, __in SC_HANDLE SchSCManager, __in LPCTSTR DriverName);
    static HRESULT
    ManageDriver(const logger& pLog, __in LPCTSTR DriverName, __in LPCTSTR ServiceName, __in USHORT Function);
    static HRESULT
    SetupDriverName(const logger& pLog, WCHAR* DriverLocation, WCHAR* szDriverFileName, ULONG BufferLength);
    static HRESULT GetDriverBinaryPathName(
        const logger& pLog,
        __in SC_HANDLE SchSCManager,
        const WCHAR* DriverName,
        WCHAR* szDriverFileName,
        ULONG BufferLength);

    logger _L_;

    SC_HANDLE m_SchSCManager = NULL;
    SC_HANDLE m_SchService = NULL;
    std::wstring m_strComputerName;
    std::wstring m_strTempDir;

    std::vector<std::shared_ptr<DriverTermination>> m_pTerminationHandlers;

    HRESULT ConnectToSCM();
    HRESULT Disconnect();
};

static const int DRIVER_FUNC_INSTALL = 0x01;
static const int DRIVER_FUNC_REMOVE = 0x02;

}  // namespace Orc

#pragma managed(pop)
