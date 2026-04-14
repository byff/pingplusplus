#include "pingworker.h"

#include <QThread>
#include <QDateTime>
#include <QDebug>

#include <cstring>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#define WIN32_LEAN_AND_MEAN
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

// ICMP echo request type
#ifndef ICMP_ECHO
#define ICMP_ECHO 8
#endif

// ICMP echo reply type
#ifndef ICMP_ECHOREPLY
#define ICMP_ECHOREPLY 0
#endif

// IP header structure for ICMP
struct PseudoHeader {
    quint32 srcAddr;
    quint32 dstAddr;
    quint8  zero;
    quint8  proto;
    quint16 icmpLength;
};

PingWorker::PingWorker(const QHostAddress& target, int timeoutMs, int payloadSize)
    : m_target(target)
    , m_timeoutMs(timeoutMs)
    , m_payloadSize(payloadSize)
    , m_index(0)
{
    setAutoDelete(true);
}

quint16 calculateChecksum(const quint16* data, int length) {
    quint32 sum = 0;
    const quint16* ptr = data;
    
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    
    if (length > 0) {
        sum += *reinterpret_cast<const quint8*>(ptr);
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return static_cast<quint16>(~sum);
}

#ifdef Q_OS_WIN

#include <windows.h>

typedef struct icmp_options {
    unsigned char Ttl;
    unsigned char Tos;
    unsigned char Flags;
    unsigned char OptionsSize;
    unsigned char* OptionsData;
} IP_OPTION_INFORMATION, * PIP_OPTION_INFORMATION;

typedef struct icmp_echo_reply {
    unsigned long  Address;
    unsigned long  Status;
    unsigned long  ReturnDataSize;
    unsigned long  RoundTripTime;
    void*          Data;
    IP_OPTION_INFORMATION Options;
} ICMP_ECHO_REPLY, * PICMP_ECHO_REPLY;

typedef unsigned long (WINAPI* IcmpSendEchoFn)(unsigned long, unsigned long, char*, unsigned short, 
                                                PIP_OPTION_INFORMATION, char*, unsigned long, unsigned long);
typedef void* (WINAPI* IcmpCloseHandleFn)(void*);
typedef void* (WINAPI* IcmpCreateFileFn)(void);
typedef unsigned char* (WINAPI* IcmpParseRepliesFn)(char*, unsigned long);

static IcmpSendEchoFn icmpSendEcho = nullptr;
static IcmpCloseHandleFn icmpCloseHandle = nullptr;
static IcmpCreateFileFn icmpCreateFile = nullptr;
static IcmpParseRepliesFn icmpParseReplies = nullptr;
static HMODULE icmpLib = nullptr;
static bool icmpInitialized = false;

static bool initIcmp() {
    if (icmpInitialized) return icmpSendEcho != nullptr;
    icmpInitialized = true;
    
    icmpLib = LoadLibraryA("icmp.dll");
    if (!icmpLib) return false;
    
    icmpSendEcho = (IcmpSendEchoFn)GetProcAddress(icmpLib, "IcmpSendEcho");
    icmpCloseHandle = (IcmpCloseHandleFn)GetProcAddress(icmpLib, "IcmpCloseHandle");
    icmpCreateFile = (IcmpCreateFileFn)GetProcAddress(icmpLib, "IcmpCreateFile");
    icmpParseReplies = (IcmpParseRepliesFn)GetProcAddress(icmpLib, "IcmpParseReplies");
    
    return icmpSendEcho != nullptr;
}

int PingWorker::rawSocket() {
    if (!initIcmp()) return -1;
    
    void* handle = icmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) return -1;
    
    // Store handle in a static for cleanup
    static void* storedHandle = handle;
    Q_UNUSED(storedHandle);
    
    return reinterpret_cast<int>(handle);
}

bool PingWorker::sendPing(int sock, const QHostAddress& addr, int payloadSize) {
    Q_UNUSED(sock);
    Q_UNUSED(addr);
    Q_UNUSED(payloadSize);
    // sendPing logic is handled in receivePing on Windows via IcmpSendEcho
    return true;
}

bool PingWorker::receivePing(int sock, int timeoutMs, qint64* outRttUs) {
    if (outRttUs) *outRttUs = -1;
    
    void* handle = reinterpret_cast<void*>(sock);
    if (!handle || handle == INVALID_HANDLE_VALUE) return false;
    
    quint32 ipAddr = inet_addr(m_target.toString().toLatin1().constData());
    
    char* sendBuf = new char[m_payloadSize];
    memset(sendBuf, 0xAB, m_payloadSize); // fill with pattern
    
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + m_payloadSize + 8];
    
    quint64 startTime = QDateTime::currentMSecsSinceEpoch();
    
    unsigned long result = icmpSendEcho(
        reinterpret_cast<unsigned long>(handle),
        ipAddr,
        sendBuf,
        static_cast<unsigned short>(m_payloadSize),
        nullptr,
        replyBuf,
        static_cast<unsigned long>(sizeof(replyBuf)),
        static_cast<unsigned long>(timeoutMs)
    );
    
    delete[] sendBuf;
    
    quint64 endTime = QDateTime::currentMSecsSinceEpoch();
    
    if (result == 0) {
        return false;
    }
    
    ICMP_ECHO_REPLY* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
    
    if (reply->Status == 0 && outRttUs) {
        *outRttUs = static_cast<qint64>(reply->RoundTripTime) * 1000; // ms to us
    } else if (reply->RoundTripTime != 0 && outRttUs) {
        *outRttUs = static_cast<qint64>(reply->RoundTripTime) * 1000;
    } else {
        *outRttUs = static_cast<qint64>((endTime - startTime) * 1000);
    }
    
    return reply->Status == 0;
}

#else // Unix/Linux implementation

int PingWorker::rawSocket() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        qWarning() << "Failed to create raw socket:" << strerror(errno);
        return -1;
    }
    
    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        close(sock);
        return -1;
    }
    
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

bool PingWorker::sendPing(int sock, const QHostAddress& addr, int payloadSize) {
    struct icmp icmpPacket;
    memset(&icmpPacket, 0, sizeof(icmpPacket));
    
    icmpPacket.icmp_type = ICMP_ECHO;
    icmpPacket.icmp_code = 0;
    icmpPacket.icmp_id = static_cast<quint16>(QCoreApplication::applicationPid() & 0xFFFF);
    icmpPacket.icmp_seq = static_cast<quint16>(m_index & 0xFFFF);
    
    // Fill payload with pattern
    char* payload = reinterpret_cast<char*>(&icmpPacket.icmp_data[0]);
    for (int i = 0; i < payloadSize; ++i) {
        payload[i] = static_cast<char>((i + m_index) & 0xFF);
    }
    
    // Calculate checksum
    icmpPacket.icmp_cksum = 0;
    icmpPacket.icmp_cksum = calculateChecksum(
        reinterpret_cast<quint16*>(&icmpPacket), 
        sizeof(icmpPacket) - payloadSize + m_payloadSize
    );
    
    struct sockaddr_in destAddr;
    memset(&destAddr, 0, sizeof(destAddr));
    destAddr.sin_family = AF_INET;
    destAddr.sin_addr.s_addr = inet_addr(addr.toString().toLatin1().constData());
    
    ssize_t sent = sendto(sock, &icmpPacket, sizeof(icmpPacket), 0,
                          reinterpret_cast<struct sockaddr*>(&destAddr),
                          sizeof(destAddr));
    
    return sent > 0;
}

bool PingWorker::receivePing(int sock, int timeoutMs, qint64* outRttUs) {
    if (outRttUs) *outRttUs = -1;
    
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sock, &readSet);
    
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    
    quint64 sendTime = QDateTime::currentMSecsSinceEpoch();
    
    int ready = select(sock + 1, &readSet, nullptr, nullptr, &tv);
    if (ready <= 0) {
        return false;
    }
    
    quint64 recvTime = QDateTime::currentMSecsSinceEpoch();
    
    char recvBuf[1024];
    struct sockaddr_in fromAddr;
    socklen_t fromLen = sizeof(fromAddr);
    
    ssize_t received = recvfrom(sock, recvBuf, sizeof(recvBuf), 0,
                                 reinterpret_cast<struct sockaddr*>(&fromAddr),
                                 &fromLen);
    
    if (received < 0) {
        return false;
    }
    
    // Skip IP header
    struct ip* ipHdr = reinterpret_cast<struct ip*>(recvBuf);
    int ipHeaderLen = ipHdr->ip_hl * 4;
    
    if (received < ipHeaderLen + ICMP_MINLEN) {
        return false;
    }
    
    struct icmp* icmpReply = reinterpret_cast<struct icmp*>(recvBuf + ipHeaderLen);
    
    // Check if it's an echo reply with matching ID
    quint16 pid = static_cast<quint16>(QCoreApplication::applicationPid() & 0xFFFF);
    
    if (icmpReply->icmp_type == ICMP_ECHOREPLY && 
        icmpReply->icmp_id == pid &&
        icmpReply->icmp_seq == static_cast<quint16>(m_index & 0xFFFF)) {
        
        if (outRttUs) {
            *outRttUs = static_cast<qint64>((recvTime - sendTime) * 1000);
        }
        return true;
    }
    
    return false;
}

#endif

void PingWorker::run() {
    bool success = false;
    qint64 rttUs = -1;
    
#ifdef Q_OS_WIN
    int sock = rawSocket();
    if (sock < 0) {
        emit finished(m_index, false, rttUs);
        return;
    }
    
    void* handle = reinterpret_cast<void*>(sock);
    
    quint32 ipAddr = inet_addr(m_target.toString().toLatin1().constData());
    char* sendBuf = new char[m_payloadSize];
    memset(sendBuf, 0xAB, m_payloadSize);
    
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + m_payloadSize + 8];
    
    quint64 startTime = QDateTime::currentMSecsSinceEpoch();
    
    unsigned long result = icmpSendEcho(
        reinterpret_cast<unsigned long>(handle),
        ipAddr,
        sendBuf,
        static_cast<unsigned short>(m_payloadSize),
        nullptr,
        replyBuf,
        static_cast<unsigned long>(sizeof(replyBuf)),
        static_cast<unsigned long>(m_timeoutMs)
    );
    
    delete[] sendBuf;
    
    if (result != 0) {
        ICMP_ECHO_REPLY* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
        if (reply->Status == 0) {
            success = true;
            if (reply->RoundTripTime != 0) {
                rttUs = static_cast<qint64>(reply->RoundTripTime) * 1000;
            } else {
                quint64 endTime = QDateTime::currentMSecsSinceEpoch();
                rttUs = static_cast<qint64>((endTime - startTime) * 1000);
            }
        }
    }
    
    if (icmpCloseHandle) {
        icmpCloseHandle(handle);
    }
    
#else
    int sock = rawSocket();
    if (sock < 0) {
        emit finished(m_index, false, rttUs);
        return;
    }
    
    if (!sendPing(sock, m_target, m_payloadSize)) {
        close(sock);
        emit finished(m_index, false, rttUs);
        return;
    }
    
    success = receivePing(sock, m_timeoutMs, &rttUs);
    
    close(sock);
#endif
    
    emit finished(m_index, success, rttUs);
}
