#pragma once

#if COLLA_TCC && COLLA_WIN

#include <Windows.h>
#include <stdlib.h>

//// FILE.H ////////////////////////////////////////////////////////////////////////////////////

static BOOL tcc_GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    LARGE_INTEGER file_size = {0};
    file_size.LowPart = GetFileSize(hFile, &file_size.HighPart);
    if (lpFileSize) *lpFileSize = file_size;
    return file_size.LowPart != INVALID_FILE_SIZE;
}

#define GetFileSizeEx tcc_GetFileSizeEx

////////////////////////////////////////////////////////////////////////////////////////////////

//// STR.H /////////////////////////////////////////////////////////////////////////////////////

#define CP_UTF8 65001

#define strtoull _strtoui64
#define strtoll _strtoi64
#define strtof strtod

extern unsigned __int64 __stdcall _strtoui64(const char *strSource, char **endptr, int base);
extern __int64 __stdcall _strtoi64(const char *strSource, char **endptr, int base);
// extern double __cdecl strtod(const char *strSource, char **endptr);

extern int __stdcall WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWCH lpWideCharStr, int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte, LPCCH lpDefaultChar, LPBOOL lpUsedDefaultChar);
extern int __stdcall MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCCH lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);

////////////////////////////////////////////////////////////////////////////////////////////////

//// SOCKET.H //////////////////////////////////////////////////////////////////////////////////

#define WSADESCRIPTION_LEN 256
#define WSASYS_STATUS_LEN  128
#define SOCK_STREAM        1               
#define SOCK_DGRAM         2               
#define AF_INET            2
#define INADDR_ANY         (ULONG)0x00000000

#define WSA_IO_PENDING                   (ERROR_IO_PENDING)
#define WSA_IO_INCOMPLETE                (ERROR_IO_INCOMPLETE)
#define WSA_INVALID_HANDLE               (ERROR_INVALID_HANDLE)
#define WSA_INVALID_PARAMETER            (ERROR_INVALID_PARAMETER)
#define WSA_NOT_ENOUGH_MEMORY            (ERROR_NOT_ENOUGH_MEMORY)
#define WSA_OPERATION_ABORTED            (ERROR_OPERATION_ABORTED)
#define WSA_INVALID_EVENT                ((WSAEVENT)NULL)
#define WSA_MAXIMUM_WAIT_EVENTS          (MAXIMUM_WAIT_OBJECTS)
#define WSA_WAIT_FAILED                  (WAIT_FAILED)
#define WSA_WAIT_EVENT_0                 (WAIT_OBJECT_0)
#define WSA_WAIT_IO_COMPLETION           (WAIT_IO_COMPLETION)
#define WSA_WAIT_TIMEOUT                 (WAIT_TIMEOUT)
#define WSA_INFINITE                     (INFINITE)
#define WSAEINTR                         10004L
#define WSAEBADF                         10009L
#define WSAEACCES                        10013L
#define WSAEFAULT                        10014L
#define WSAEINVAL                        10022L
#define WSAEMFILE                        10024L
#define WSAEWOULDBLOCK                   10035L
#define WSAEINPROGRESS                   10036L
#define WSAEALREADY                      10037L
#define WSAENOTSOCK                      10038L
#define WSAEDESTADDRREQ                  10039L
#define WSAEMSGSIZE                      10040L
#define WSAEPROTOTYPE                    10041L
#define WSAENOPROTOOPT                   10042L
#define WSAEPROTONOSUPPORT               10043L
#define WSAESOCKTNOSUPPORT               10044L
#define WSAEOPNOTSUPP                    10045L
#define WSAEPFNOSUPPORT                  10046L
#define WSAEAFNOSUPPORT                  10047L
#define WSAEADDRINUSE                    10048L
#define WSAEADDRNOTAVAIL                 10049L
#define WSAENETDOWN                      10050L
#define WSAENETUNREACH                   10051L
#define WSAENETRESET                     10052L
#define WSAECONNABORTED                  10053L
#define WSAECONNRESET                    10054L
#define WSAENOBUFS                       10055L
#define WSAEISCONN                       10056L
#define WSAENOTCONN                      10057L
#define WSAESHUTDOWN                     10058L
#define WSAETOOMANYREFS                  10059L
#define WSAETIMEDOUT                     10060L
#define WSAECONNREFUSED                  10061L
#define WSAELOOP                         10062L
#define WSAENAMETOOLONG                  10063L
#define WSAEHOSTDOWN                     10064L
#define WSAEHOSTUNREACH                  10065L
#define WSAENOTEMPTY                     10066L
#define WSAEPROCLIM                      10067L
#define WSAEUSERS                        10068L
#define WSAEDQUOT                        10069L
#define WSAESTALE                        10070L
#define WSAEREMOTE                       10071L
#define WSASYSNOTREADY                   10091L
#define WSAVERNOTSUPPORTED               10092L
#define WSANOTINITIALISED                10093L
#define WSAEDISCON                       10101L
#define WSAENOMORE                       10102L
#define WSAECANCELLED                    10103L
#define WSAEINVALIDPROCTABLE             10104L
#define WSAEINVALIDPROVIDER              10105L
#define WSAEPROVIDERFAILEDINIT           10106L
#define WSASYSCALLFAILURE                10107L
#define WSASERVICE_NOT_FOUND             10108L
#define WSATYPE_NOT_FOUND                10109L
#define WSA_E_NO_MORE                    10110L
#define WSA_E_CANCELLED                  10111L
#define WSAEREFUSED                      10112L
#define WSAHOST_NOT_FOUND                11001L
#define WSATRY_AGAIN                     11002L
#define WSANO_RECOVERY                   11003L
#define WSANO_DATA                       11004L
#define WSA_QOS_RECEIVERS                11005L
#define WSA_QOS_SENDERS                  11006L
#define WSA_QOS_NO_SENDERS               11007L
#define WSA_QOS_NO_RECEIVERS             11008L
#define WSA_QOS_REQUEST_CONFIRMED        11009L
#define WSA_QOS_ADMISSION_FAILURE        11010L
#define WSA_QOS_POLICY_FAILURE           11011L
#define WSA_QOS_BAD_STYLE                11012L
#define WSA_QOS_BAD_OBJECT               11013L
#define WSA_QOS_TRAFFIC_CTRL_ERROR       11014L
#define WSA_QOS_GENERIC_ERROR            11015L
#define WSA_QOS_ESERVICETYPE             11016L
#define WSA_QOS_EFLOWSPEC                11017L
#define WSA_QOS_EPROVSPECBUF             11018L
#define WSA_QOS_EFILTERSTYLE             11019L
#define WSA_QOS_EFILTERTYPE              11020L
#define WSA_QOS_EFILTERCOUNT             11021L
#define WSA_QOS_EOBJLENGTH               11022L
#define WSA_QOS_EFLOWCOUNT               11023L
#define WSA_QOS_EUNKOWNPSOBJ             11024L
#define WSA_QOS_EPOLICYOBJ               11025L
#define WSA_QOS_EFLOWDESC                11026L
#define WSA_QOS_EPSFLOWSPEC              11027L
#define WSA_QOS_EPSFILTERSPEC            11028L
#define WSA_QOS_ESDMODEOBJ               11029L
#define WSA_QOS_ESHAPERATEOBJ            11030L
#define WSA_QOS_RESERVED_PETYPE          11031L
#define WSA_SECURE_HOST_NOT_FOUND        11032L
#define WSA_IPSEC_NAME_POLICY_ERROR      11033L

typedef UINT_PTR SOCKET;

typedef struct WSAData {
        WORD                    wVersion;
        WORD                    wHighVersion;
#ifdef _WIN64
        unsigned short          iMaxSockets;
        unsigned short          iMaxUdpDg;
        char FAR *              lpVendorInfo;
        char                    szDescription[WSADESCRIPTION_LEN+1];
        char                    szSystemStatus[WSASYS_STATUS_LEN+1];
#else
        char                    szDescription[WSADESCRIPTION_LEN+1];
        char                    szSystemStatus[WSASYS_STATUS_LEN+1];
        unsigned short          iMaxSockets;
        unsigned short          iMaxUdpDg;
        char FAR *              lpVendorInfo;
#endif
} WSADATA, *LPWSADATA;

typedef struct pollfd {
    SOCKET  fd;
    SHORT   events;
    SHORT   revents;
} WSAPOLLFD, *LPWSAPOLLFD;

struct  tcc__protoent {
    char *p_name;
    char **p_aliases;
    short   p_proto;                /* protocol # */
};

typedef struct in_addr {
        union {
                struct { UCHAR s_b1,s_b2,s_b3,s_b4; } S_un_b;
                struct { USHORT s_w1,s_w2; } S_un_w;
                ULONG S_addr;
        } S_un;
#define s_addr  S_un.S_addr /* can be used for most tcp & ip code */
#define s_host  S_un.S_un_b.s_b2    // host on imp
#define s_net   S_un.S_un_b.s_b1    // network
#define s_imp   S_un.S_un_w.s_w2    // imp
#define s_impno S_un.S_un_b.s_b4    // imp #
#define s_lh    S_un.S_un_b.s_b3    // logical host
} IN_ADDR;

typedef struct sockaddr_in {
#if(_WIN32_WINNT < 0x0600)
    short   sin_family;
#else //(_WIN32_WINNT < 0x0600)
    ADDRESS_FAMILY sin_family;
#endif //(_WIN32_WINNT < 0x0600)

    USHORT sin_port;
    IN_ADDR sin_addr;
    CHAR sin_zero[8];
} SOCKADDR_IN;

struct  hostent {
    char    *h_name;           /* official name of host */
    char    **h_aliases;  /* alias list */
    short   h_addrtype;             /* host address type */
    short   h_length;               /* length of address */
    char    **h_addr_list; /* list of addresses */
#define h_addr  h_addr_list[0]          /* address, for backward compat */
};

typedef struct sockaddr {

#if (_WIN32_WINNT < 0x0600)
    USHORT sa_family;
#else
    ADDRESS_FAMILY sa_family;           // Address family.
#endif //(_WIN32_WINNT < 0x0600)

    CHAR sa_data[14];                   // Up to 14 bytes of direct address.
} SOCKADDR;

#define protoent tcc__protoent

extern int __stdcall WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData);
extern int __stdcall WSACleanup(void);
extern int __stdcall closesocket(SOCKET s);
extern int __stdcall WSAPoll(LPWSAPOLLFD fdArray, ULONG fds, INT timeout);
extern int __stdcall WSAGetLastError(void);
extern struct protoent *__stdcall getprotobyname(const char *name);
extern SOCKET __stdcall socket(int af, int type, int protocol);
extern USHORT __stdcall htons(USHORT hostshort);
extern unsigned long __stdcall inet_addr(const char *cp);
extern int __stdcall bind(SOCKET s, const struct sockaddr *name, int namelen);
extern int __stdcall listen(SOCKET s, int backlog);
extern SOCKET __stdcall accept(SOCKET s, struct sockaddr *addr, int *addrlen);
extern struct hostent *__stdcall gethostbyname(const char *name);

extern char *__stdcall inet_ntoa(struct in_addr in);
extern int __stdcall connect(SOCKET s, const struct sockaddr *name, int namelen);
extern int __stdcall send(SOCKET s, const char *buf, int len, int flags);
extern int __stdcall sendto(SOCKET s, const char *buf, int len, int flags, const struct sockaddr *to, int tolen);
extern int __stdcall recv(SOCKET s, char *buf, int len, int flags);
extern int __stdcall recvfrom(SOCKET s, char *buf, int len, int flags, struct sockaddr *from, int *fromlen);

////////////////////////////////////////////////////////////////////////////////////////////////

// for the next few, we need to use colla.def, so it doesn't work with tcc -run
// because of this, let's just put them behind the defines

#if !COLLA_NOTHREADS

typedef struct CONDITION_VARIABLE {                    
    PVOID Ptr;                                       
} CONDITION_VARIABLE, *PCONDITION_VARIABLE;

extern void __stdcall InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable);;
extern void __stdcall WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable);
extern void __stdcall WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable);
extern BOOL __stdcall SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds);

#endif

#if !COLLA_NOHTTP

#define INTERNET_OPEN_TYPE_PRECONFIG 0
#define INTERNET_DEFAULT_HTTPS_PORT 443
#define INTERNET_SERVICE_HTTP 3
#define INTERNET_FLAG_SECURE 0x00800000
#define HTTP_QUERY_STATUS_CODE 19
#define HTTP_QUERY_RAW_HEADERS_CRLF 22
#define HTTP_QUERY_FLAG_NUMBER 0x20000000


typedef LPVOID HINTERNET;
typedef WORD INTERNET_PORT;

#if UNICODE
    #define InternetOpen InternetOpenW
    #define InternetConnect InternetConnectW
    #define HttpOpenRequest HttpOpenRequestW
    #define HttpSendRequest HttpSendRequestW
    #define HttpAddRequestHeaders HttpAddRequestHeadersW
    #define HttpQueryInfo HttpQueryInfoW
#else
    #define InternetOpen InternetOpenA
    #define InternetConnect InternetConnectA
    #define HttpOpenRequest HttpOpenRequestA
    #define HttpSendRequest HttpSendRequestA
    #define HttpAddRequestHeaders HttpAddRequestHeadersA
    #define HttpQueryInfo HttpQueryInfoA
#endif

extern HINTERNET __stdcall InternetOpenW(LPCWSTR lpszAgent, DWORD dwAccessType, LPCWSTR lpszProxy, LPCWSTR lpszProxyBypass, DWORD dwFlags);
extern HINTERNET __stdcall InternetConnectW(HINTERNET hInternet, LPCWSTR lpszServerName, INTERNET_PORT nServerPort, LPCWSTR lpszUserName, LPCWSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext);
extern HINTERNET __stdcall HttpOpenRequestW(HINTERNET hConnect, LPCWSTR lpszVerb, LPCWSTR lpszObjectName, LPCWSTR lpszVersion, LPCWSTR lpszReferrer, LPCWSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext);
extern BOOL __stdcall HttpSendRequestW(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength);
extern BOOL __stdcall HttpAddRequestHeadersW(HINTERNET hRequest, LPCWSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwModifiers);
extern BOOL __stdcall HttpQueryInfoW(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex);

extern HINTERNET __stdcall InternetOpenA(LPCSTR lpszAgent, DWORD dwAccessType, LPCSTR lpszProxy, LPCSTR lpszProxyBypass, DWORD dwFlags);
extern HINTERNET __stdcall InternetConnectA(HINTERNET hInternet, LPCSTR lpszServerName, INTERNET_PORT nServerPort, LPCSTR lpszUserName, LPCSTR lpszPassword, DWORD dwService, DWORD dwFlags, DWORD_PTR dwContext);
extern HINTERNET __stdcall HttpOpenRequestA(HINTERNET hConnect, LPCSTR lpszVerb, LPCSTR lpszObjectName, LPCSTR lpszVersion, LPCSTR lpszReferrer, LPCSTR *lplpszAcceptTypes, DWORD dwFlags, DWORD_PTR dwContext);
extern BOOL __stdcall HttpSendRequestA(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional, DWORD dwOptionalLength);
extern BOOL __stdcall HttpAddRequestHeadersA(HINTERNET hRequest, LPCSTR lpszHeaders, DWORD dwHeadersLength, DWORD dwModifiers);
extern BOOL __stdcall HttpQueryInfoA(HINTERNET hRequest, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpdwBufferLength, LPDWORD lpdwIndex);

extern BOOL __stdcall InternetReadFile(HINTERNET hFile, LPVOID lpBuffer, DWORD dwNumberOfBytesToRead, LPDWORD lpdwNumberOfBytesRead);
extern BOOL __stdcall InternetCloseHandle(HINTERNET hInternet);


#endif

#endif

