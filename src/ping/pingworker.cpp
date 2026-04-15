#include "pingworker.h"

#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QCoreApplication>

#include <cstring>
#include <cstdlib>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
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
static HANDLE icmpHandle = INVALID_HANDLE_VALUE;
static bool icmpInited = false;

static bool initIcmpLib() {
    if (icmpInited) return icmpHandle != INVALID_HANDLE_VALUE;
    icmpInited = true;

    // IcmpCreateFile is available on all Windows versions (Windows 2000+)
    icmpHandle = IcmpCreateFile();
    if (icmpHandle == INVALID_HANDLE_VALUE) {
        qWarning() << "IcmpCreateFile failed, error:" << GetLastError();
        return false;
    }
    return true;
}

void PingWorker::run() {
    if (!initIcmpLib()) {
        qWarning() << "ICMP library init failed for" << m_targetIp;
        emit finished(m_index, false, -1);
        return;
    }

    // Convert IP string to unsigned long
    unsigned long ipAddr = inet_addr(m_targetIp.toLatin1().constData());
    if (ipAddr == INADDR_NONE) {
        qWarning() << "Invalid IP address:" << m_targetIp;
        emit finished(m_index, false, -1);
        return;
    }

    // Allocate send buffer (payload)
    char* sendBuf = new char[m_payloadSize];
    for (int i = 0; i < m_payloadSize; ++i) {
        sendBuf[i] = static_cast<char>(0xAB);
    }

    // Reply buffer: ICMP_ECHO_REPLY + options + reply data
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + m_payloadSize + 8;
    char* replyBuf = new char[replySize];
    memset(replyBuf, 0, replySize);

    // Send ICMP echo request
    DWORD startTick = GetTickCount();

    // IcmpSendEcho returns number of replies, 0 means timeout or error
    DWORD result = IcmpSendEcho(icmpHandle,
                                ipAddr,
                                sendBuf,
                                static_cast<WORD>(m_payloadSize),
                                NULL,          // no IP options
                                replyBuf,
                                replySize,
                                m_timeoutMs);

    DWORD endTick = GetTickCount();

    delete[] sendBuf;

    bool success = false;
    qint64 rttUs = -1;

    if (result > 0) {
        // First reply at beginning of buffer
        PICMP_ECHO_REPLY pEchoReply = reinterpret_cast<PICMP_ECHO_REPLY>(replyBuf);

        // Status codes: IP_SUCCESS (0) means reply received
        if (pEchoReply->Status == IP_SUCCESS) {
            success = true;
            // RoundTripTime is in milliseconds
            if (pEchoReply->RoundTripTime > 0) {
                rttUs = static_cast<qint64>(pEchoReply->RoundTripTime) * 1000;
            } else {
                // If RTT is 0, calculate from tick difference
                rttUs = static_cast<qint64>(endTick - startTick) * 1000;
            }
        } else {
            qDebug() << "ICMP error status:" << pEchoReply->Status << "for" << m_targetIp;
        }
    } else {
        DWORD err = GetLastError();
        qDebug() << "IcmpSendEcho returned" << result << "error:" << err << "for" << m_targetIp;
    }

    delete[] replyBuf;

    emit finished(m_index, success, rttUs);
}

#else

// Linux/Unix: raw ICMP socket (requires root or CAP_NET_RAW capability)
void PingWorker::run() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        qWarning() << "Failed to create raw ICMP socket (need root?):" << strerror(errno);
        emit finished(m_index, false, -1);
        return;
    }

    // Set socket timeout for receive
    struct timeval tv;
    tv.tv_sec = m_timeoutMs / 1000;
    tv.tv_usec = (m_timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Build ICMP packet
    struct icmp icmpPkt;
    memset(&icmpPkt, 0, sizeof(icmpPkt));
    icmpPkt.icmp_type = ICMP_ECHO;
    icmpPkt.icmp_code = 0;
    // Use PID + thread ID to make unique ICMP ID
    icmpPkt.icmp_id   = static_cast<quint16>((QCoreApplication::applicationPid() & 0xFFFF));
    icmpPkt.icmp_seq  = static_cast<quint16>(m_index & 0xFFFF);

    // Fill payload with pattern
    char* payload = icmpPkt.icmp_data;
    for (int i = 0; i < m_payloadSize; ++i) {
        payload[i] = static_cast<char>((i + m_index) & 0xFF);
    }

    // Calculate checksum: ICMP header (8 bytes) + payload
    icmpPkt.icmp_cksum = 0;
    icmpPkt.icmp_cksum = calculateChecksum(
        reinterpret_cast<quint16*>(&icmpPkt),
        ICMP_MINLEN + m_payloadSize);

    // Setup destination
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(m_targetIp.toLatin1().constData());

    // Record send time
    unsigned long sendMs = (QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFFUL);

    // Send ICMP packet
    ssize_t sent = sendto(sock, &icmpPkt, sizeof(icmpPkt), 0,
                          reinterpret_cast<struct sockaddr*>(&dest), sizeof(dest));

    if (sent <= 0) {
        qWarning() << "Failed to send ICMP to" << m_targetIp << ":" << strerror(errno);
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
        // Skip IP header to get to ICMP
        struct ip* ipHdr = reinterpret_cast<struct ip*>(recvBuf);
        int ipHdrLen = ipHdr->ip_hl * 4;

        if (received >= ipHdrLen + ICMP_MINLEN) {
            struct icmp* reply = reinterpret_cast<struct icmp*>(recvBuf + ipHdrLen);

            // Check if it's an echo reply with matching ID
            quint16 expectedId = static_cast<quint16>(QCoreApplication::applicationPid() & 0xFFFF);
            if (reply->icmp_type == ICMP_ECHOREPLY &&
                reply->icmp_id == expectedId &&
                reply->icmp_seq == static_cast<quint16>(m_index & 0xFFFF)) {
                success = true;
                rttUs = static_cast<qint64>(recvMs - sendMs) * 1000;
                if (rttUs < 0) rttUs = -1;
            }
        }
    }

    emit finished(m_index, success, rttUs);
}

#endif
