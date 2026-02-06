/**
 * @file signer/signer.c
 *
 * Yori sign files for exclusive use on the executing machine
 *
 * Copyright (c) 2018-2022 Malcolm J. Smith
 * Copyright (c) 2025 Jochen Neubeck
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <yoripch.h>
#include <yorilib.h>
#pragma warning(disable: 4005)
#include <wincrypt.h>
#pragma warning(default: 4005)

/**
 Help text to display to the user.
 */
const CHAR strSignerHelpText[] =
    "\n"
    "Sign files for exclusive use on the executing machine.\n"
    "\n"
    "SIGNER [-license] [-s] [-t] <file>...\n"
    "\n"
    "   -s             Process files from all subdirectories\n"
    "   -t             Add certificate to TrustedPublisher store\n";

/**
 Display usage text to the user.
 */
static BOOL SignerHelp(VOID)
{
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("Signer %i.%02i\n"), YORI_VER_MAJOR, YORI_VER_MINOR);
#if YORI_BUILD_ID
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("  Build %i\n"), YORI_BUILD_ID);
#endif
    YoriLibOutput(YORI_LIB_OUTPUT_STDOUT, _T("%hs"), strSignerHelpText);
    return TRUE;
}

/**
 Output an error message alongside with as much reasoning as GetLastError() can give.
 */
static void OutputLastError(LPCTSTR szFmt)
{
    DWORD LastError = GetLastError();
    LPTSTR ErrText = YoriLibGetWinErrorText(LastError);
    YoriLibOutput(YORI_LIB_OUTPUT_STDERR, szFmt, ErrText);
    YoriLibFreeWinErrorText(ErrText);
}

// See http://msdn.microsoft.com/en-us/library/windows/desktop/jj835834.aspx

// dwSubjectChoice should be one of the following:
static const DWORD SIGNER_SUBJECT_FILE              = 0x01;
static const DWORD SIGNER_SUBJECT_BLOB              = 0x02;

// dwAttrChoice should be one of the following:
static const DWORD SIGNER_NO_ATTR                   = 0x00;
static const DWORD SIGNER_AUTHCODE_ATTR             = 0x01;

// dwPvkChoice should be one of the following:
static const DWORD PVK_TYPE_FILE_NAME               = 0x01;
static const DWORD PVK_TYPE_KEYCONTAINER            = 0x02;

// dwCertPolicy can be a combination of the following flags:
static const DWORD SIGNER_CERT_POLICY_STORE         = 0x01;
static const DWORD SIGNER_CERT_POLICY_CHAIN         = 0x02;
static const DWORD SIGNER_CERT_POLICY_SPC           = 0x04;
static const DWORD SIGNER_CERT_POLICY_CHAIN_NO_ROOT = 0x08;

// dwCertChoice should be one of the following:
static const DWORD SIGNER_CERT_SPC_FILE             = 0x01;
static const DWORD SIGNER_CERT_STORE                = 0x02;
static const DWORD SIGNER_CERT_SPC_CHAIN            = 0x03;

typedef struct {
    DWORD cbSize;
    LPCWSTR pwszFileName;
    HANDLE hFile;
} SIGNER_FILE_INFO;

typedef struct {
    DWORD cbSize;
    GUID *pGuidSubject;
    DWORD cbBlob;
    BYTE *pbBlob;
    LPCWSTR pwszDisplayName;
} SIGNER_BLOB_INFO;

typedef struct {
    DWORD cbSize;
    DWORD *pdwIndex;
    DWORD dwSubjectChoice;
    union {
        SIGNER_FILE_INFO *pSignerFileInfo;
        SIGNER_BLOB_INFO *pSignerBlobInfo;
    };
} SIGNER_SUBJECT_INFO;

typedef struct {
    DWORD cbSize;
    PCCERT_CONTEXT pSigningCert;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
} SIGNER_CERT_STORE_INFO;

typedef struct {
    DWORD cbSize;
    LPCWSTR pwszSpcFile;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
} SIGNER_SPC_CHAIN_INFO;

typedef struct {
    DWORD cbSize;
    DWORD dwCertChoice;
    union {
        LPCWSTR pwszSpcFile;
        SIGNER_CERT_STORE_INFO *pCertStoreInfo;
        SIGNER_SPC_CHAIN_INFO *pSpcChainInfo;
    };
    HWND hwnd;
} SIGNER_CERT;

typedef struct {
    DWORD cbSize;
    BOOL fCommercial;
    BOOL fIndividual;
    LPCWSTR pwszName;
    LPCWSTR pwszInfo;
} SIGNER_ATTR_AUTHCODE;

typedef struct {
    DWORD cbSize;
    ALG_ID algidHash;
    DWORD dwAttrChoice;
    union {
        SIGNER_ATTR_AUTHCODE *pAttrAuthcode;
    };
    PCRYPT_ATTRIBUTES psAuthenticated;
    PCRYPT_ATTRIBUTES psUnauthenticated;
} SIGNER_SIGNATURE_INFO;

typedef struct {
    DWORD cbSize;
    LPCWSTR pwszProviderName;
    DWORD dwProviderType;
    DWORD dwKeySpec;
    DWORD dwPvkChoice;
    union {
        LPWSTR pwszPvkFileName;
        LPWSTR pwszKeyContainer;
    };
} SIGNER_PROVIDER_INFO;

typedef struct {
    DWORD cbSize;
    DWORD cbBlob;
    BYTE *pbBlob;
} SIGNER_CONTEXT;

/**
 A structure containing optional function pointers to mssign32.dll exported
 functions which programs can operate without having hard dependencies on.
 */
static struct {
    LPCWSTR Name;

    union {
        LPCSTR Name; HRESULT(WINAPI* Invoke)
        (
            __in SIGNER_CONTEXT* pSignerContext
        );
    } SignerFreeSignerContext;

    union {
        LPCSTR Name; HRESULT(WINAPI* Invoke)
        (
            __in DWORD dwFlags,
            __in SIGNER_SUBJECT_INFO* pSubjectInfo,
            __in SIGNER_CERT* pSignerCert,
            __in SIGNER_SIGNATURE_INFO* pSignatureInfo,
            __in_opt SIGNER_PROVIDER_INFO* pProviderInfo,
            __in_opt LPCWSTR pwszHttpTimeStamp,
            __in_opt PCRYPT_ATTRIBUTES psRequest,
            __in_opt LPVOID pSipData,
            __out SIGNER_CONTEXT** ppSignerContext
        );
    } SignerSignEx;

    /**
     A handle to the Dll module.
     */
    HINSTANCE Handle;
} DllMssign32 = {
    L"MSSIGN32.DLL",
    "SignerFreeSignerContext",
    "SignerSignEx",
    (HINSTANCE)NULL
};

/*
 Load a DLL alongside with pointers to its exported functions.
*/
static HINSTANCE LoadDll(LPCWSTR *pname, HINSTANCE *pinst)
{
    HINSTANCE hinst = *pinst;
    if (hinst == NULL) {
        hinst = LoadLibraryEx(*pname, NULL, 0);
        while ((void *)--pinst > (void *)pname) {
            *(FARPROC *)pinst = GetProcAddress(hinst, (LPCSTR)*pinst);
        }
    }
    return hinst;
}

/**
 Context passed to the callback which is invoked for each file found.
 */
typedef struct _YSIGNER_CONTEXT {
    /**
     Counts the number of files processed in an enumerate.  If this is zero,
     the program assumes the request is to create a new file.
     */
    ULONG FilesFoundThisArg;

    /**
     The YoriSigner certificate.  Created on the fly if none exists yet.
     */
    PCCERT_CONTEXT pCertContext;
} YSIGNER_CONTEXT, *PYSIGNER_CONTEXT;

/**
 A callback that is invoked when a file is found that matches a search criteria
 specified in the set of strings to enumerate.

 @param FilePath Pointer to the file path that was found.

 @param FileInfo Information about the file.  Note in this application this can
        be NULL when it is operating on files that do not yet exist.

 @param Depth Specifies the recursion depth.  Ignored in this application.

 @param Context Pointer to the touch context structure indicating the
        action to perform and populated with the file and line count found.

 @return TRUE to continute enumerating, FALSE to abort.
 */
BOOL
SignerFileFoundCallback(
    __in PYORI_STRING FilePath,
    __in_opt PWIN32_FIND_DATA FileInfo,
    __in DWORD Depth,
    __in PVOID Context
    )
{
    PYSIGNER_CONTEXT SignerContext = (PYSIGNER_CONTEXT)Context;

    DWORD dwIndex = 0;
    SIGNER_FILE_INFO signerFileInfo;
    SIGNER_SUBJECT_INFO signerSubjectInfo;
    SIGNER_CERT_STORE_INFO signerCertStoreInfo;
    SIGNER_CERT signerCert;
    SIGNER_SIGNATURE_INFO signerSignatureInfo;
    SIGNER_CONTEXT *pSignerContext = NULL;

    UNREFERENCED_PARAMETER(Depth);
    UNREFERENCED_PARAMETER(FileInfo);

    ASSERT(YoriLibIsStringNullTerminated(FilePath));

    SignerContext->FilesFoundThisArg++;

    // Prepare SIGNER_FILE_INFO struct
    signerFileInfo.cbSize = sizeof signerFileInfo;
    signerFileInfo.pwszFileName = FilePath->StartOfString;
    signerFileInfo.hFile = NULL;

    // Prepare SIGNER_SUBJECT_INFO struct
    signerSubjectInfo.cbSize = sizeof signerSubjectInfo;
    signerSubjectInfo.pdwIndex = &dwIndex;
    signerSubjectInfo.dwSubjectChoice = SIGNER_SUBJECT_FILE;
    signerSubjectInfo.pSignerFileInfo = &signerFileInfo;

    // Prepare SIGNER_CERT_STORE_INFO struct
    signerCertStoreInfo.cbSize = sizeof signerCertStoreInfo;
    signerCertStoreInfo.pSigningCert = SignerContext->pCertContext;
    signerCertStoreInfo.dwCertPolicy = SIGNER_CERT_POLICY_CHAIN;
    signerCertStoreInfo.hCertStore = NULL;

    // Prepare SIGNER_CERT struct
    signerCert.cbSize = sizeof signerCert;
    signerCert.dwCertChoice = SIGNER_CERT_STORE;
    signerCert.pCertStoreInfo = &signerCertStoreInfo;
    signerCert.hwnd = NULL;

    // Prepare SIGNER_SIGNATURE_INFO struct
    signerSignatureInfo.cbSize = sizeof signerSignatureInfo;
    signerSignatureInfo.algidHash = CALG_SHA1;
    signerSignatureInfo.dwAttrChoice = SIGNER_NO_ATTR;
    signerSignatureInfo.pAttrAuthcode = NULL;
    signerSignatureInfo.psAuthenticated = NULL;
    signerSignatureInfo.psUnauthenticated = NULL;

    // Sign file with cert
    if (SUCCEEDED(DllMssign32.SignerSignEx.Invoke(0, &signerSubjectInfo, &signerCert, &signerSignatureInfo, NULL, NULL, NULL, NULL, &pSignerContext))) {
        DllMssign32.SignerFreeSignerContext.Invoke(pSignerContext);
    } else {
        OutputLastError(_T("Signing file failed: %s"));
    }
    return TRUE;
}

#ifdef YORI_BUILTIN
/**
 The main entrypoint for the touch builtin command.
 */
#define ENTRYPOINT YoriCmd_TOUCH
#else
/**
 The main entrypoint for the touch standalone application.
 */
#define ENTRYPOINT ymain
#endif

/**
 The main entrypoint for the touch cmdlet.

 @param ArgC The number of arguments.

 @param ArgV An array of arguments.

 @return Exit code of the child process on success, or failure if the child
         could not be launched.
 */
DWORD
ENTRYPOINT(
    __in YORI_ALLOC_SIZE_T ArgC,
    __in YORI_STRING ArgV[]
    )
{
    BOOLEAN ArgumentUnderstood;
    YORI_ALLOC_SIZE_T i;
    YORI_ALLOC_SIZE_T StartArg = 1;
    WORD MatchFlags;
    BOOLEAN Recursive = FALSE;
    BOOLEAN BasicEnumeration = FALSE;
    BOOLEAN TrustedPublisher = FALSE;
    YSIGNER_CONTEXT SignerContext;
    YORI_STRING Arg;

    static WCHAR wszContainerName[] = L"YoriSigner";

    static CRYPT_KEY_PROV_INFO kpInfo =
    {
        wszContainerName,		// pwszContainerName
        NULL,					// pwszProvName
        PROV_RSA_FULL,			// dwProvType
        CRYPT_MACHINE_KEYSET,	// dwFlags
        0,						// cProvParam
        NULL,					// rgProvParam
        AT_SIGNATURE			// dwKeySpec
    };

    static const WCHAR subject[] = L"CN=YoriSigner";

    static SYSTEMTIME startTime = { 1901, 1, 0, 1, 0, 0, 0, 0 };
    static SYSTEMTIME endTime = { 9999, 12, 0, 31, 0, 0, 0, 0 };

    HCERTSTORE hStore;

    CERT_NAME_BLOB SubjectIssuerBlob = { 0, NULL };

    ZeroMemory(&SignerContext, sizeof(SignerContext));

    for (i = 1; i < ArgC; i++) {

        ArgumentUnderstood = FALSE;
        ASSERT(YoriLibIsStringNullTerminated(&ArgV[i]));

        if (YoriLibIsCommandLineOption(&ArgV[i], &Arg)) {

            if (YoriLibCompareStringLitIns(&Arg, _T("?")) == 0) {
                SignerHelp();
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("license")) == 0) {
                YoriLibDisplayMitLicense(_T("2018-2025"));
                return EXIT_SUCCESS;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("s")) == 0) {
                Recursive = TRUE;
                ArgumentUnderstood = TRUE;
            } else if (YoriLibCompareStringLitIns(&Arg, _T("t")) == 0) {
                TrustedPublisher = TRUE;
                ArgumentUnderstood = TRUE;
            }
        } else {
            ArgumentUnderstood = TRUE;
            StartArg = i;
            break;
        }

        if (!ArgumentUnderstood) {
            YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("Argument not understood, ignored: %y\n"), &ArgV[i]);
        }
    }

    //
    //  If no file name is specified, use stdin; otherwise open
    //  the file and use that
    //

    if (StartArg == ArgC) {
        YoriLibOutput(YORI_LIB_OUTPUT_STDERR, _T("signer: missing argument\n"));
        return EXIT_FAILURE;
    }

    LoadDll(&DllMssign32.Name, &DllMssign32.Handle);

    // Encode certificate Subject
    if (!CertStrToName(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, NULL, SubjectIssuerBlob.pbData, &SubjectIssuerBlob.cbData, NULL) ||
        !CertStrToName(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, NULL, SubjectIssuerBlob.pbData = (BYTE *)_alloca(SubjectIssuerBlob.cbData), &SubjectIssuerBlob.cbData, NULL)) {
        OutputLastError(_T("Encoding X.500 subject failed: %s"));
        return EXIT_FAILURE;
    }

    // Open Root cert store in machine profile
    hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"Root");
    if (hStore == NULL) {
        OutputLastError(_T("Opening certificate store failed: %s"));
        return EXIT_FAILURE;
    }

    // First look for a matching certificate with a public key which is known to the associated provider
    while ((SignerContext.pCertContext = CertFindCertificateInStore(hStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0,
        CERT_FIND_SUBJECT_NAME, &SubjectIssuerBlob, SignerContext.pCertContext)) != NULL) {
        // Check if the public key is known to the associated provider
        HCRYPTPROV hCryptProvOrNCryptKey;
        DWORD dwKeySpec;
        BOOL fCallerFreeProvOrNCryptKey;
        if (CryptAcquireCertificatePrivateKey(SignerContext.pCertContext,
            CRYPT_ACQUIRE_COMPARE_KEY_FLAG | CRYPT_ACQUIRE_CACHE_FLAG, NULL,
            &hCryptProvOrNCryptKey, &dwKeySpec, &fCallerFreeProvOrNCryptKey))
        {
            // CRYPT_ACQUIRE_CACHE_FLAG - no need to free the returned handle
            break;
        }
    }

    if (SignerContext.pCertContext == NULL) {

        // Acquire key container
        HCRYPTPROV hCryptProv;
        if (CryptAcquireContext(&hCryptProv, kpInfo.pwszContainerName, kpInfo.pwszProvName, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET) ||
            CryptAcquireContext(&hCryptProv, kpInfo.pwszContainerName, kpInfo.pwszProvName, PROV_RSA_FULL, CRYPT_MACHINE_KEYSET | CRYPT_NEWKEYSET)) {

            // Generate new key pair
            HCRYPTKEY hKey;
            if (CryptGenKey(hCryptProv, AT_SIGNATURE, RSA1024BIT_KEY, &hKey)) {
                CryptDestroyKey(hKey);
            }

            // Create self-signed certificate
            SignerContext.pCertContext = CertCreateSelfSignCertificate(0, &SubjectIssuerBlob, 0, &kpInfo, NULL, &startTime, &endTime, NULL);

            // Add self-signed cert to the store
            if (SignerContext.pCertContext != NULL) {
                CertAddCertificateContextToStore(hStore, SignerContext.pCertContext, CERT_STORE_ADD_REPLACE_EXISTING, 0);
            } else {
                OutputLastError(_T("Creating certificate failed: %s"));
            }

            CryptReleaseContext(hCryptProv, 0);
        }
    }

    MatchFlags = YORILIB_FILEENUM_RETURN_FILES | YORILIB_FILEENUM_RETURN_DIRECTORIES;
    if (Recursive) {
        MatchFlags |= YORILIB_FILEENUM_RECURSE_BEFORE_RETURN | YORILIB_FILEENUM_RECURSE_PRESERVE_WILD;
    }
    if (BasicEnumeration) {
        MatchFlags |= YORILIB_FILEENUM_BASIC_EXPANSION;
    }

    if (SignerContext.pCertContext != NULL) {

        if (TrustedPublisher) {
            HANDLE hStore2 = CertOpenStore(CERT_STORE_PROV_SYSTEM, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"TrustedPublisher");
            if (hStore2 != NULL) {
                CertAddCertificateContextToStore(hStore2, SignerContext.pCertContext, CERT_STORE_ADD_REPLACE_EXISTING, 0);
                CertCloseStore(hStore2, 0);
            } else {
                OutputLastError(_T("Opening TrustedPublisher certificate store failed: %s"));
            }
        }

        for (i = StartArg; i < ArgC; i++) {
            SignerContext.FilesFoundThisArg = 0;
            YoriLibForEachFile(&ArgV[i], MatchFlags, 0, SignerFileFoundCallback, NULL, &SignerContext);
            if (SignerContext.FilesFoundThisArg == 0) {
                YORI_STRING FullPath;
                YoriLibInitEmptyString(&FullPath);
                if (YoriLibUserStringToSingleFilePath(&ArgV[i], TRUE, &FullPath)) {
                    SignerFileFoundCallback(&FullPath, NULL, 0, &SignerContext);
                    YoriLibFreeStringContents(&FullPath);
                }
            }
        }

        CertFreeCertificateContext(SignerContext.pCertContext);
    }

    CertCloseStore(hStore, 0);

    return EXIT_SUCCESS;
}

// vim:sw=4:ts=4:et:
