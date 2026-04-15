#include "pingworker.h"

#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>

#include <cstring>
#include <cstdlib>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <icmpapi.h>
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

// ICMP type constants
#ifndef ICMP_ECHO
#define ICMP_ECHO 8
#endif
#ifndef ICMP_ECHOREPLY
#define ICMP_ECHOREPLY 0
#endif
#ifndef ICMP_MINLEN
#define ICMP_MINLEN 8
#endif

static quint16 calculateChecksum(const quint16* data, int length) {
    const quint16* ptr = data;
    quint32 sum = 0;
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

PingWorker::PingWorker(const QString& targetIp, int timeoutMs, int payloadSize, int index)
    : m_targetIp(targetIp)
    , m_timeoutMs(timeoutMs)
    , m_payloadSize(payloadSize)
    , m_index(index)
{
    setAutoDelete(true);
}

#ifdef Q_OS_WIN

// Windows: use IcmpSendEcho API
static bool initIcmpLib();
static void* icmpHandle = nullptr;
static bool icmpInited = false;

static bool initIcmpLib() {
    if (icmpInited) return icmpHandle != nullptr;
    icmpInited = true;
    typedef void* (WINAPI* IcmpCreateFileFn)(void);
    IcmpCreateFileFn fn = (IcmpCreateFileFn)GetProcAddress(
        GetModuleHandleA("icmp.dll"), "IcmpCreateFile");
    if (fn) icmpHandle = fn();
    return icmpHandle != nullptr;
}

void PingWorker::run() {
    if (!initIcmpLib()) {
        emit finished(m_index, false, -1);
        return;
    }

    unsigned long ipAddr = inet_addr(m_targetIp.toLatin1().constData());
    if (ipAddr == INADDR_NONE) {
        emit finished(m_index, false, -1);
        return;
    }

    char* sendBuf = new char[m_payloadSize];
    memset(sendBuf, 0xAB, m_payloadSize);
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + m_payloadSize + 8] = {};

    unsigned long startMs = GetTickCount();
    unsigned long result = IcmpSendEcho(icmpHandle, ipAddr, sendBuf,
        static_cast<unsigned short>(m_payloadSize), nullptr, replyBuf,
        sizeof(replyBuf), static_cast<unsigned long>(m_timeoutMs));
    unsigned long endMs = GetTickCount();
    delete[] sendBuf;

    bool success = false;
    qint64 rttUs = -1;

    if (result != 0) {
        ICMP_ECHO_REPLY* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
        if (reply->Status == 0) {
            success = true;
            rttUs = static_cast<qint64>(reply->RoundTripTime) * 1000;
        }
    }

    if (rttUs < 0 && success) {
        rttUs = static_cast<qint64>(endMs - startMs) * 1000;
    }

    emit finished(m_index, success, rttUs);
}

#else

// Linux/Unix: raw ICMP socket
void PingWorker::run() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        emit finished(m_index, false, -1);
        return;
    }

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = m_timeoutMs / 1000;
    tv.tv_usec = (m_timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Build ICMP packet
    struct icmp icmpPkt;
    memset(&icmpPkt, 0, sizeof(icmpPkt));
    icmpPkt.icmp_type = ICMP_ECHO;
    icmpPkt.icmp_code = 0;
    icmpPkt.icmp_id   = static_cast<quint16>(QCoreApplication::applicationPid() & 0xFFFF);
    icmpPkt.icmp_seq  = static_cast<quint16>(m_index & 0xFFFF);

    // Fill payload
    char* payload = reinterpret_cast<char*>(icmpPkt.icmp_data);
    for (int i = 0; i < m_payloadSize; ++i) {
        payload[i] = static_cast<char>((i + m_index) & 0xFF);
    }

    // Checksum: ICMP header (8 bytes) + payload
    icmpPkt.icmp_cksum = 0;
    icmpPkt.icmp_cksum = calculateChecksum(
        reinterpret_cast<quint16*>(&icmpPkt),
        ICMP_MINLEN + m_payloadSize);

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(m_targetIp.toLatin1().constData());

    unsigned long sendMs = (QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFFUL);

    ssize_t sent = sendto(sock, &icmpPkt, sizeof(icmpPkt), 0,
                          reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));

    if (sent <= 0) {
        close(sock);
        emit finished(m_index, false, -1);
        return;
    }

    // Receive reply
    char recvBuf[1024];
    struct sockaddr_in from;
    socklen_t fromLen = sizeof(from);

    ssize_t received = recvfrom(sock, recvBuf, sizeof(recvBuf), 0,
                                reinterpret_cast<struct sockaddr*>(&from), &fromLen);
    unsigned long recvMs = (QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFFUL);

    close(sock);

    bool success = false;
    qint64 rttUs = -1;

    if (received >= 0) {
        struct ip* ipHdr = reinterpret_cast<struct ip*>(recvBuf);
        int ipHdrLen = ipHdr->ip_hl * 4;
        if (received >= ipHdrLen + ICMP_MINLEN) {
            struct icmp* reply = reinterpret_cast<struct icmp*>(recvBuf + ipHdrLen);
            quint16 pid = static_cast<quint16>(QCoreApplication::applicationPid() & 0xFFFF);
            if (reply->icmp_type == ICMP_ECHOREPLY && reply->icmp_id == pid
                && reply->icmp_seq == static_cast<quint16>(m_index & 0xFFFF)) {
                success = true;
                // RTT from tick count diff
                rttUs = static_cast<qint64>(recvMs - sendMs) * 1000;
                if (rttUs < 0) rttUs = -1;
            }
        }
    }

    emit finished(m_index, success, rttUs);
}

#endif
