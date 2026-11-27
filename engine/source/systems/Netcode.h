#pragma once
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace eng::systems::net {
constexpr float TickSeconds = 1.0f / 60.0f;
constexpr std::size_t MaxPending = 256;
struct Position { float x = 0, z = 0; };
struct Input { std::uint32_t sequence = 0; float x = 0, z = 0; };
struct Snapshot { std::uint32_t tick = 0, acknowledged = 0; Position position; };
Position Step(Position p, Input input);
std::vector<std::uint8_t> Encode(Input input);
std::vector<std::uint8_t> Encode(Snapshot snapshot);
std::optional<Input> DecodeInput(const std::vector<std::uint8_t>& data);
std::optional<Snapshot> DecodeSnapshot(const std::vector<std::uint8_t>& data);
class Prediction {
public:
    Input Push(float x, float z);
    bool Reconcile(Snapshot snapshot);
    Position GetPosition() const { return m_position; }
    const std::deque<Input>& Pending() const { return m_pending; }
private:
    Position m_position;
    std::uint32_t m_sequence = 0, m_ack = 0;
    std::deque<Input> m_pending;
};
// Single-client lab authority. Ordered, bounded input replay with duplicate
// suppression. This deliberately does not implement authentication/anti-cheat.
class Authority {
public:
    bool Receive(Input input);
    Snapshot State() const { return {m_ack, m_ack, m_position}; }
private:
    Position m_position;
    std::uint32_t m_ack = 0;
    std::map<std::uint32_t, Input> m_buffer;
};
class Interpolation {
public:
    void Push(Snapshot snapshot);
    Position Sample(double tick) const; // clamps to endpoints; no unbounded extrapolation
private:
    std::deque<Snapshot> m_samples;
};
struct Endpoint {
    std::string address = "127.0.0.1";
    std::uint16_t port = 0;
    bool operator==(const Endpoint& b) const { return address == b.address && port == b.port; }
};
struct Datagram { Endpoint source; std::vector<std::uint8_t> bytes; };
class UdpSocket {
public:
    explicit UdpSocket(Endpoint bind = {});
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    std::uint16_t Port() const { return m_port; }
    bool Send(const Endpoint& destination, const std::vector<std::uint8_t>& bytes);
    std::optional<Datagram> Receive();
private:
    std::intptr_t m_socket = -1;
    std::uint16_t m_port = 0;
};
class Impairment {
public:
    Impairment(double latencyMs, double jitterMs, double loss, std::uint32_t seed);
    void Send(UdpSocket& socket, Endpoint to, std::vector<std::uint8_t> bytes, double nowMs);
    void Flush(double nowMs);
    std::size_t Dropped() const { return m_dropped; }
private:
    struct Pending { double due; UdpSocket* socket; Endpoint to; std::vector<std::uint8_t> bytes; };
    double m_latency, m_jitter, m_loss;
    std::mt19937 m_random;
    std::vector<Pending> m_pending;
    std::size_t m_dropped = 0;
};
}
