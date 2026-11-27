#include "systems/Netcode.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace eng::systems::net;
// Real UDP sockets, one process, two endpoints; virtual time makes the CI demo
// quick. Each endpoint sees packets only after the seeded impairment queue.
int main(int argc, char** argv) {
    try {
        int ticks = 600; double latency = 75, jitter = 20, loss = .05; std::string output;
        for (int i=1;i<argc;++i) {
            const std::string arg=argv[i];
            if (i+1>=argc) throw std::invalid_argument("missing option value");
            const std::string value=argv[++i];
            if(arg=="--ticks") ticks=std::stoi(value);
            else if(arg=="--latency") latency=std::stod(value);
            else if(arg=="--jitter") jitter=std::stod(value);
            else if(arg=="--loss") loss=std::stod(value);
            else if(arg=="--csv") output=value;
            else throw std::invalid_argument("unknown option: "+arg);
        }
        if(ticks<1 || ticks>100000) throw std::invalid_argument("ticks must be 1..100000");
        UdpSocket clientSocket, serverSocket;
        Endpoint clientEnd{"127.0.0.1",clientSocket.Port()}, serverEnd{"127.0.0.1",serverSocket.Port()};
        Impairment up(latency,jitter,loss,42), down(latency,jitter,loss,43);
        Prediction client; Authority server; Interpolation remote;
        std::ofstream csv;
        if(!output.empty()) { csv.open(output); if(!csv) throw std::runtime_error("cannot open CSV"); csv<<"tick,predicted_x,server_x,interpolated_x,pending\n"; }
        int tick=0;
        for(;tick<ticks+1200;++tick) {
            const double now=tick*(1000.0/60.0);
            if(tick<ticks) {
                auto input=client.Push((tick/120)%2 ? -1.0f : 1.0f, 0);
                up.Send(clientSocket,serverEnd,Encode(input),now);
            }
            // Bounded redundant inputs are retried until acknowledged by a snapshot.
            if(tick%6==0) for(const auto& input:client.Pending()) up.Send(clientSocket,serverEnd,Encode(input),now);
            up.Flush(now); down.Flush(now);
            for(int n=0;n<512;++n) {
                auto packet=serverSocket.Receive(); if(!packet) break;
                if(packet->source==clientEnd) if(auto input=DecodeInput(packet->bytes)) server.Receive(*input);
            }
            if(tick%3==0) down.Send(serverSocket,clientEnd,Encode(server.State()),now);
            for(int n=0;n<512;++n) {
                auto packet=clientSocket.Receive(); if(!packet) break;
                if(packet->source==serverEnd) if(auto snapshot=DecodeSnapshot(packet->bytes)) { client.Reconcile(*snapshot); remote.Push(*snapshot); }
            }
            auto rendered=remote.Sample(static_cast<double>(server.State().tick)-6);
            if(csv) csv<<tick<<','<<client.GetPosition().x<<','<<server.State().position.x<<','<<rendered.x<<','<<client.Pending().size()<<'\n';
            if(tick>=ticks && client.Pending().empty()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // give OS socket delivery a turn
        }
        const double error=std::abs(client.GetPosition().x-server.State().position.x);
        const bool passed=server.State().acknowledged==static_cast<std::uint32_t>(ticks) && client.Pending().empty() && error<.0001;
        std::cout<<(passed?"NETCODE DEMO PASSED":"NETCODE DEMO FAILED")<<" inputs="<<ticks<<" ack="<<server.State().acknowledged
            <<" one_way_ms="<<latency<<" jitter_ms="<<jitter<<" loss="<<loss<<" dropped="<<(up.Dropped()+down.Dropped())<<" final_error="<<error<<'\n';
        return passed?0:1;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
