//
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// Copyright © 2011-2019 ANSSI. All Rights Reserved.
//
// Author(s): Jean Gautier (ANSSI)
//
#include "stdafx.h"

#include "EncodeMessageStream.h"
#include "SystemDetails.h"
#include "CryptoUtilities.h"
#include "LogFileWriter.h"

using namespace Orc;

STDMETHODIMP EncodeMessageStream::AddRecipient(const CBinaryBuffer& buffer)
{
    HRESULT hr = E_FAIL;
    PCCERT_CONTEXT pCertContext = NULL;

    if (FAILED(hr = AddCertificate(buffer, pCertContext)))
        return hr;

    m_recipients.push_back(pCertContext);
    return S_OK;
}

STDMETHODIMP EncodeMessageStream::AddRecipient(LPCWSTR szEncodedCert, DWORD cchLen, DWORD dwFlags)
{
    HRESULT hr = E_FAIL;
    PCCERT_CONTEXT pCertContext = NULL;

    if (FAILED(hr = AddCertificate(szEncodedCert, cchLen, pCertContext, dwFlags)))
        return hr;

    m_recipients.push_back(pCertContext);
    return S_OK;
}

STDMETHODIMP EncodeMessageStream::AddRecipient(const std::wstring& strEncodedCert, DWORD dwFlags)
{
    HRESULT hr = E_FAIL;
    PCCERT_CONTEXT pCertContext = NULL;

    if (FAILED(hr = AddCertificate(strEncodedCert, pCertContext, dwFlags)))
        return hr;

    m_recipients.push_back(pCertContext);
    return S_OK;
}

STDMETHODIMP EncodeMessageStream::AddRecipient(LPCSTR szEncodedCert, DWORD cchLen, DWORD dwFlags)
{
    HRESULT hr = E_FAIL;
    PCCERT_CONTEXT pCertContext = NULL;

    if (FAILED(hr = AddCertificate(szEncodedCert, cchLen, pCertContext, dwFlags)))
        return hr;

    m_recipients.push_back(pCertContext);
    return S_OK;
}

STDMETHODIMP EncodeMessageStream::AddRecipient(const std::string& strEncodedCert, DWORD dwFlags)
{
    HRESULT hr = E_FAIL;
    PCCERT_CONTEXT pCertContext = NULL;

    if (FAILED(hr = AddCertificate(strEncodedCert, pCertContext, dwFlags)))
        return hr;

    m_recipients.push_back(pCertContext);
    return S_OK;
}

STDMETHODIMP EncodeMessageStream::Initialize(const std::shared_ptr<ByteStream>& pInnerStream)
{
    HRESULT hr = E_FAIL;

    if (FAILED(hr = OpenCertStore()))
        return hr;

    DWORD dwMajor = 0L, dwMinor = 0L;
    SystemDetails::GetOSVersion(dwMajor, dwMinor);

    if (dwMajor < 6)
    {
        if (FAILED(hr = CryptoUtilities::AcquireContext(_L_, m_hProv)))
        {
            log::Error(_L_, hr, L"Failed to acquire suitable Crypto Service Provider\r\n");
            return hr;
        }
        EncodeInfo.ContentEncryptionAlgorithm.pszObjId = szOID_RSA_DES_EDE3_CBC;
        EncodeInfo.ContentEncryptionAlgorithm.Parameters.cbData = 0L;
        EncodeInfo.ContentEncryptionAlgorithm.Parameters.pbData = NULL;
    }
    else
    {
        EncodeInfo.ContentEncryptionAlgorithm.pszObjId = szOID_NIST_AES128_CBC;
        EncodeInfo.ContentEncryptionAlgorithm.Parameters.cbData = 0L;
        EncodeInfo.ContentEncryptionAlgorithm.Parameters.pbData = NULL;
        EncodeInfo.pvEncryptionAuxInfo = NULL;
    }

    EncodeInfo.hCryptProv = m_hProv;
    EncodeInfo.cbSize = sizeof(CMSG_ENVELOPED_ENCODE_INFO);

    EncodeInfo.cRecipients = (DWORD)m_recipients.size();
    EncodeInfo.rgpRecipients = (PCERT_INFO*)malloc(m_recipients.size() * sizeof(PCERT_INFO));

    DWORD dwIndex = 0L;
    for (auto item : m_recipients)
    {
        EncodeInfo.rgpRecipients[dwIndex] = item->pCertInfo;
        dwIndex++;
    }

    m_pChainedStream = pInnerStream;

    m_StreamInfo.cbContent = CMSG_INDEFINITE_LENGTH;
    m_StreamInfo.pvArg = m_pChainedStream.get();
    m_StreamInfo.pfnStreamOutput =
        (PFN_CMSG_STREAM_OUTPUT)[](IN const void* pvArg, IN BYTE* pbData, IN DWORD cbData, IN BOOL fFinal)->BOOL
    {

        auto pStream = (ByteStream*)pvArg;

        if (pStream == nullptr)
            return FALSE;

        ULONGLONG ullWritten = 0LL;
        if (FAILED(pStream->Write(pbData, cbData, &ullWritten)))
            return FALSE;

        if (fFinal)
            if (FAILED(pStream->Close()))
                return FALSE;

        return TRUE;
    };

    m_hMsg = CryptMsgOpenToEncode(
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0L, CMSG_ENVELOPED, &EncodeInfo, NULL, &m_StreamInfo);
    if (m_hMsg == NULL)
    {
        log::Error(_L_, hr = HRESULT_FROM_WIN32(GetLastError()), L"Failed to open message for encoding\r\n");
        return hr;
    }
    return S_OK;
}

__data_entrypoint(File) HRESULT EncodeMessageStream::Read(
    __out_bcount_part(cbBytesToRead, *pcbBytesRead) PVOID pBuffer,
    __in ULONGLONG cbBytesToRead,
    __out_opt PULONGLONG pcbBytesRead)
{
    DBG_UNREFERENCED_PARAMETER(pBuffer);

    *pcbBytesRead = 0;

    if (cbBytesToRead > MAXDWORD)
        return E_INVALIDARG;

    log::Verbose(_L_, L"Cannot read from an EncodeMessageStream\r\n");
    return E_NOTIMPL;
}

HRESULT EncodeMessageStream::Write(
    __in_bcount(cbBytes) const PVOID pBuffer,
    __in ULONGLONG cbBytes,
    __out PULONGLONG pcbBytesWritten)
{
    HRESULT hr = E_FAIL;

    if (m_hMsg == NULL)
        return E_POINTER;

    if (cbBytes > MAXDWORD)
    {
        log::Error(_L_, E_INVALIDARG, L"Too many bytes to XOR\r\n");
        return E_INVALIDARG;
    }

    if (!CryptMsgUpdate(m_hMsg, (const BYTE*)pBuffer, static_cast<DWORD>(cbBytes), FALSE))
    {
        log::Error(_L_, hr = HRESULT_FROM_WIN32(GetLastError()), L"CryptMsgUpdate failed\r\n");
        return hr;
    }
    *pcbBytesWritten = cbBytes;
    log::Verbose(_L_, L"CryptMsgUpdate %d bytes succeeded (hMsg=0x%lx)\r\n", cbBytes, m_hMsg);
    return S_OK;
}

HRESULT EncodeMessageStream::SetFilePointer(
    __in LONGLONG lDistanceToMove,
    __in DWORD dwMoveMethod,
    __out_opt PULONG64 pqwCurrPointer)
{
    DBG_UNREFERENCED_PARAMETER(lDistanceToMove);
    DBG_UNREFERENCED_PARAMETER(dwMoveMethod);
    DBG_UNREFERENCED_PARAMETER(pqwCurrPointer);
    log::Error(
        _L_,
        E_NOTIMPL,
        L"SetFilePointer is not implemented in EncodeMessageStream distance: %I64d, Method:%d\r\n",
        lDistanceToMove,
        dwMoveMethod);
    return S_OK;
}

ULONG64 EncodeMessageStream::GetSize()
{
    log::Verbose(_L_, L"GetFileSizeEx is not implemented on EncodeMessageStream\r\n");
    return (ULONG64)-1;
}

HRESULT EncodeMessageStream::SetSize(ULONG64 ullNewSize)
{
    DBG_UNREFERENCED_PARAMETER(ullNewSize);
    log::Verbose(_L_, L"SetSize is not implemented on EncodeMessageStream\r\n");
    return S_OK;
}

HRESULT EncodeMessageStream::Close()
{
    if (m_hMsg != NULL)
    {
        HRESULT hr = E_FAIL;
        if (!CryptMsgUpdate(m_hMsg, NULL, 0L, TRUE))
        {
            log::Error(_L_, hr = HRESULT_FROM_WIN32(GetLastError()), L"Final CryptMsgUpdate failed\r\n");
            return hr;
        }
        if (!CryptMsgClose(m_hMsg))
        {
            log::Error(_L_, hr = HRESULT_FROM_WIN32(GetLastError()), L"CryptMsgClose failed\r\n");
            m_hMsg = NULL;
            return hr;
        }
        m_hMsg = NULL;
    }
    return S_OK;
}

EncodeMessageStream::~EncodeMessageStream()
{
    if (EncodeInfo.rgpRecipients != NULL)
    {
        free(EncodeInfo.rgpRecipients);
        EncodeInfo.rgpRecipients = NULL;
    }
}
