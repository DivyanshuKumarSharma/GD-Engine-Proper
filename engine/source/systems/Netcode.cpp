#include "systems/Netcode.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace eng::systems::net {
namespace {
bool Valid(Input in) { return in.sequence != 0 && std::isfinite(in.x) && std::isfinite(in.z) && std::abs(in.x) <= 1 && std::abs(in.z) <= 1; }
bool Valid(Snapshot s) { return s.tick == s.acknowledged && std::isfinite(s.position.x) && std::isfinite(s.position.z) && std::abs(s.position.x) <= 1e6f && std::abs(s.position.z) <= 1e6f; }
void Put(std::vector<std::uint8_t>& out, std::uint32_t value) { for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<std::uint8_t>(value >> shift)); }
std::uint32_t Get(const std::vector<std::uint8_t>& in, std::size_t offset) { std::uint32_t n = 0; for (int i = 0; i < 4; ++i) n = (n << 8) | in[offset + i]; return n; }
void PutFloat(std::vector<std::uint8_t>& out, float value) { static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559); std::uint32_t n; std::memcpy(&n, &value, 4); Put(out, n); }
float GetFloat(const std::vector<std::uint8_t>& in, std::size_t offset) { auto n = Get(in, offset); float f; std::memcpy(&f, &n, 4); return f; }
bool Header(const std::vector<std::uint8_t>& in, std::uint8_t kind, std::size_t size) { return in.size() == size && in[0] == 'G' && in[1] == 'D' && in[2] == 1 && in[3] == kind; }
#ifdef _WIN32
using Native = SOCKET;
void Close(Native s) { closesocket(s); }
struct Winsock { Winsock() { WSADATA data; if (WSAStartup(MAKEWORD(2,2), &data)) throw std::runtime_error("WSAStartup failed"); } ~Winsock() { WSACleanup(); } };
#else
using Native = int;
void Close(Native s) { close(s); }
#endif
sockaddr_in Address(const Endpoint& ep) {
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(ep.port);
    if (inet_pton(AF_INET, ep.address.c_str(), &a.sin_addr) != 1) throw std::invalid_argument("expected numeric IPv4 address");
    return a;
}
}
Position Step(Position p, Input input) {
    float length = std::sqrt(input.x * input.x + input.z * input.z);
    if (length > 1) { input.x /= length; input.z /= length; }
    p.x += input.x * 6 * TickSeconds; p.z += input.z * 6 * TickSeconds; return p;
}
std::vector<std::uint8_t> Encode(Input in) {
    if (!Valid(in)) throw std::invalid_argument("invalid network input");
    std::vector<std::uint8_t> out{'G','D',1,1}; Put(out, in.sequence); PutFloat(out, in.x); PutFloat(out, in.z); return out;
}
std::vector<std::uint8_t> Encode(Snapshot s) {
    if (!Valid(s)) throw std::invalid_argument("invalid snapshot");
    std::vector<std::uint8_t> out{'G','D',1,2}; Put(out, s.tick); Put(out, s.acknowledged); PutFloat(out, s.position.x); PutFloat(out, s.position.z); return out;
}
std::optional<Input> DecodeInput(const std::vector<std::uint8_t>& in) {
    if (!Header(in, 1, 16)) return {};
    Input value{Get(in, 4), GetFloat(in, 8), GetFloat(in, 12)};
    return Valid(value) ? std::optional<Input>(value) : std::nullopt;
}
std::optional<Snapshot> DecodeSnapshot(const std::vector<std::uint8_t>& in) {
    if (!Header(in, 2, 20)) return {};
    Snapshot value{Get(in,4), Get(in,8), {GetFloat(in,12), GetFloat(in,16)}};
    return Valid(value) ? std::optional<Snapshot>(value) : std::nullopt;
}
Input Prediction::Push(float x, float z) {
    if (m_pending.size() >= MaxPending) throw std::runtime_error("prediction window exhausted: peer disconnected");
    if (m_sequence == std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("sequence exhausted: reconnect");
    Input input{m_sequence + 1, x, z};
    if (!Valid(input)) throw std::invalid_argument("invalid prediction input");
    ++m_sequence; m_pending.push_back(input); m_position = Step(m_position, input); return input;
}
bool Prediction::Reconcile(Snapshot s) {
    if (!Valid(s) || s.acknowledged <= m_ack || s.acknowledged > m_sequence) return false;
    m_ack = s.acknowledged; m_position = s.position;
    while (!m_pending.empty() && m_pending.front().sequence <= m_ack) m_pending.pop_front();
    for (const auto& input : m_pending) m_position = Step(m_position, input);
    return true;
}
bool Authority::Receive(Input in) {
    if (!Valid(in) || in.sequence <= m_ack || in.sequence - m_ack > MaxPending) return false;
    m_buffer.emplace(in.sequence, in);
    while (m_ack != std::numeric_limits<std::uint32_t>::max()) {
        auto it = m_buffer.find(m_ack + 1); if (it == m_buffer.end()) break;
        m_position = Step(m_position, it->second); ++m_ack; m_buffer.erase(it);
    }
    return true;
}
void Interpolation::Push(Snapshot s) {
    if (!Valid(s)) return;
    auto it = std::lower_bound(m_samples.begin(), m_samples.end(), s.tick, [](const auto& a, auto tick) { return a.tick < tick; });
    if (it != m_samples.end() && it->tick == s.tick) return;
    m_samples.insert(it, s); while (m_samples.size() > 64) m_samples.pop_front();
}
Position Interpolation::Sample(double tick) const {
    if (m_samples.empty()) return {};
    if (!std::isfinite(tick) || tick <= m_samples.front().tick) return m_samples.front().position;
    for (std::size_t i = 1; i < m_samples.size(); ++i) if (tick <= m_samples[i].tick) {
        const auto& a = m_samples[i-1]; const auto& b = m_samples[i];
        const float alpha = static_cast<float>((tick - a.tick) / (b.tick - a.tick));
        return {a.position.x + alpha * (b.position.x - a.position.x), a.position.z + alpha * (b.position.z - a.position.z)};
    }
    return m_samples.back().position;
}
UdpSocket::UdpSocket(Endpoint bindTo) {
#ifdef _WIN32
    static Winsock winsock;
#endif
    const auto address = Address(bindTo);
    auto s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == static_cast<Native>(-1)) throw std::runtime_error("UDP socket creation failed");
    if (::bind(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) { Close(s); throw std::runtime_error("UDP bind failed"); }
#ifdef _WIN32
    u_long enabled = 1;
    if (ioctlsocket(s, FIONBIO, &enabled) != 0) { Close(s); throw std::runtime_error("UDP nonblocking failed"); }
    int size = sizeof(sockaddr_in);
#else
    if (fcntl(s, F_SETFL, O_NONBLOCK) == -1) { Close(s); throw std::runtime_error("UDP nonblocking failed"); }
    socklen_t size = sizeof(sockaddr_in);
#endif
    sockaddr_in local{};
    if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &size)) { Close(s); throw std::runtime_error("UDP getsockname failed"); }
    m_socket = static_cast<std::intptr_t>(s); m_port = ntohs(local.sin_port);
}
UdpSocket::~UdpSocket() { if (m_socket != -1) Close(static_cast<Native>(m_socket)); }
bool UdpSocket::Send(const Endpoint& to, const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > 1200) return false;
    auto address = Address(to);
    return sendto(static_cast<Native>(m_socket), reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
        reinterpret_cast<sockaddr*>(&address), sizeof(address)) == static_cast<int>(bytes.size());
}
std::optional<Datagram> UdpSocket::Receive() {
    std::uint8_t bytes[1201]; sockaddr_in source{};
#ifdef _WIN32
    int size = sizeof(source);
#else
    socklen_t size = sizeof(source);
#endif
    auto count = recvfrom(static_cast<Native>(m_socket), reinterpret_cast<char*>(bytes), sizeof(bytes), 0, reinterpret_cast<sockaddr*>(&source), &size);
    if (count < 0) {
#ifdef _WIN32
        const auto e = WSAGetLastError(); if (e == WSAEWOULDBLOCK || e == WSAEMSGSIZE || e == WSAECONNRESET) return {};
#else
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) return {};
#endif
        throw std::runtime_error("UDP receive failed");
    }
    if (count == 0 || count > 1200) return {};
    char host[INET_ADDRSTRLEN]{}; inet_ntop(AF_INET, &source.sin_addr, host, sizeof(host));
    return Datagram{{host, ntohs(source.sin_port)}, std::vector<std::uint8_t>(bytes, bytes + count)};
}
Impairment::Impairment(double latency, double jitter, double loss, std::uint32_t seed)
    : m_latency(latency), m_jitter(jitter), m_loss(loss), m_random(seed) {
    if (!std::isfinite(latency) || !std::isfinite(jitter) || !std::isfinite(loss) || latency < 0 || jitter < 0 || loss < 0 || loss > 1)
        throw std::invalid_argument("invalid network impairment");
}
void Impairment::Send(UdpSocket& socket, Endpoint to, std::vector<std::uint8_t> bytes, double nowMs) {
    if (!std::isfinite(nowMs)) throw std::invalid_argument("invalid simulation time");
    std::uniform_real_distribution<double> chance(0, 1), jitter(-m_jitter, m_jitter);
    if (chance(m_random) < m_loss || m_pending.size() >= 8192) { ++m_dropped; return; }
    m_pending.push_back({nowMs + std::max(0.0, m_latency + jitter(m_random)), &socket, std::move(to), std::move(bytes)});
}
void Impairment::Flush(double nowMs) {
    std::stable_sort(m_pending.begin(), m_pending.end(), [](const auto& a, const auto& b) { return a.due < b.due; });
    auto end = m_pending.begin();
    while (end != m_pending.end() && end->due <= nowMs) {
        if (!end->socket->Send(end->to, end->bytes)) ++m_dropped;
        ++end;
    }
    m_pending.erase(m_pending.begin(), end);
}
}
