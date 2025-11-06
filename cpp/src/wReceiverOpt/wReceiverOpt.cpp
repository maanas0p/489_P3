#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <array>
#include <chrono>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>
#include <string.h>
#include <string>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <netdb.h>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include "../common/Crc32.hpp"
#include <fstream>

using namespace std;

enum : uint32_t
{
    START = 0,
    END = 1,
    DATA = 2,
    ACK = 3
};
struct PacketHeader
{
    uint32_t type;
    uint32_t seqNum;
    uint32_t length;
    uint32_t checksum;
};

class wReceiver
{
public:
    int port;
    int window_size;
    string output_dir;
    string output_log;

    int sockfd = -1;
    sockaddr_in receiverAddr{};

    bool connection = false;
    int fileNum = 0;
    ofstream outputStream;
    ofstream loggingStream;

    uint32_t startSeqNum = 0;
    uint32_t nextExpectedSeqNum = 0;

    vector<pair<uint32_t, vector<uint8_t>>> resend;

    void ntohl_func(PacketHeader &h)
    {
        h.type = ntohl(h.type);
        h.seqNum = ntohl(h.seqNum);
        h.length = ntohl(h.length);
        h.checksum = ntohl(h.checksum);
    }

    void htonl_func(PacketHeader &h)
    {
        h.type = htonl(h.type);
        h.seqNum = htonl(h.seqNum);
        h.length = htonl(h.length);
        h.checksum = htonl(h.checksum);
    }

    vector<uint8_t> makePacket(uint32_t type, uint32_t seq, const uint8_t *data, size_t packLen)
    {

        PacketHeader h{type, seq, static_cast<uint32_t>(packLen), (type == DATA) ? crc32(data, packLen) : 0};
        PacketHeader h2 = h;
        htonl_func(h2);
        vector<uint8_t> buff(sizeof(PacketHeader) + packLen);
        memcpy(buff.data(), &h2, sizeof(h2));
        if (packLen > 0)
            memcpy(buff.data() + sizeof(h2), data, packLen);
        return buff;
    }

    void bindSocket()
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&receiverAddr, 0, sizeof(receiverAddr));
        receiverAddr.sin_family = AF_INET;
        receiverAddr.sin_addr.s_addr = INADDR_ANY;
        receiverAddr.sin_port = htons(port);

        if (::bind(sockfd, (struct sockaddr *)&receiverAddr, sizeof(receiverAddr)) < 0)
        {
            perror("bind failed");
            close(sockfd);
            return;
        }

        spdlog::debug("Socket successfully created and bound to port {}", port);
    }

    int parseArguments(int argc, char **argv)
    {
        cxxopts::Options opts("wReceiver");
        opts.add_options()("p,port", "The port number on which wReceiver is listening", cxxopts::value<int>())("w,window-size", "Maximum number of outstanding packets in the current window.", cxxopts::value<int>())("d,output-dir", "Path to the file that has to be transferred. It can be a text file or binary file.", cxxopts::value<string>())("o,output-log", "The file path to which you should log the messages as described above.", cxxopts::value<string>());
        // -p | --port The port number on which wReceiver is listening for data.
        // -w | --window-size Maximum number of outstanding packets.
        // -d | --output-dir The directory that the wReceiver will store the output files, i.e the FILE-i.out files.
        // -o | --output-log The file path to which you should log the messages as described above.
        // ./wReceiver -p 8000 -w 10 -d /tmp -o receiver.out

        cxxopts::ParseResult result;
        try
        {
            result = opts.parse(argc, argv);
        }
        catch (const exception &e)
        {
            cerr << "Error parsing options: " << e.what() << "\n\n"
                 << opts.help() << "\n";
            return 1;
        }

        if (!result.count("port") || !result.count("output-dir") || !result.count("output-log") || !result.count("window-size"))
        {
            cerr << "Error: Missing required arguments\n\n"
                 << opts.help() << "\n";
            return 1;
        }

        port = result["port"].as<int>();
        window_size = result["window-size"].as<int>();
        output_dir = result["output-dir"].as<string>();
        output_log = result["output-log"].as<string>();

        if (port < 1024 || port > 65535)
        {
            spdlog::error("Error: port number must be in the range of [1024, 65535]\n");
            return 1;
        }

        loggingStream.open(output_log, std::ios::out | std::ios::trunc);
        if (!loggingStream)
        {
            spdlog::error("Failed to open output log: ");
            return 1;
        }

        spdlog::debug("Arguments parsed successfully: port={}, window_size={}, output_dir={}, output_log={}", port, window_size, output_dir, output_log);
        return 0;
    }

    void ackAndLog(uint32_t seqNum, sockaddr_in &clientAddr, socklen_t &len)
    {
        vector<uint8_t> ackPkt = makePacket(ACK, seqNum, nullptr, 0);
        sendto(sockfd, ackPkt.data(), ackPkt.size(), 0,
               (struct sockaddr *)&clientAddr, len);

        PacketHeader ack{};
        memcpy(&ack, ackPkt.data(), sizeof(ack));
        ntohl_func(ack);
        loggingStream << ack.type << ' ' << ack.seqNum << ' ' << ack.length << ' ' << ack.checksum << '\n';
        loggingStream.flush();
    }

    void startProtocol()
    {
        vector<uint8_t> receivedPktHeader(sizeof(PacketHeader));
        while (true)
        {
            spdlog::debug("Waiting for START packet...");
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            ssize_t n = recvfrom(sockfd, receivedPktHeader.data(), receivedPktHeader.size(), 0,
                                 (struct sockaddr *)&clientAddr, &len);
            spdlog::debug("Received {} bytes for header", n);
            if (n < static_cast<ssize_t>(sizeof(PacketHeader)))
            {
                continue;
            }
            PacketHeader h{};
            memcpy(&h, receivedPktHeader.data(), sizeof(h));
            ntohl_func(h);
            loggingStream << h.type << ' ' << h.seqNum << ' ' << h.length << ' ' << h.checksum << '\n';
            loggingStream.flush();
            resend.clear();
            spdlog::debug("Packet type: {}, seqNum: {}", h.type, h.seqNum);
            if (h.type != START)
            {
                continue;
            }
            spdlog::debug("START packet received, establishing connection...");
            connection = true;
            startSeqNum = h.seqNum;
            nextExpectedSeqNum = 0;
            spdlog::debug("Connection established with startSeqNum={}", startSeqNum);
            string filename = output_dir + "/FILE-" + to_string(fileNum) + ".out";
            outputStream.open(filename, ios::binary | ios::trunc);
            fileNum++;
            ackAndLog(startSeqNum, clientAddr, len);
            break;
        }
    }

    void handleData()
    {
        if (!connection)
            return;

        spdlog::debug("Handling data packets...");
        vector<uint8_t> receviedPackets(sizeof(PacketHeader) + 1456);
        while (true)
        {
            spdlog::debug("Waiting for data packet...");
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            ssize_t n = recvfrom(sockfd, receviedPackets.data(), receviedPackets.size(), 0,
                                 (struct sockaddr *)&clientAddr, &len);

            spdlog::debug("Received {} bytes", n);
            PacketHeader h{};
            memcpy(&h, receviedPackets.data(), sizeof(h));
            ntohl_func(h);
            spdlog::debug("Packet type: {}, seqNum: {}, length: {}, checksum: {}", h.type, h.seqNum, h.length, h.checksum);
            if (h.type == END)
            {
                loggingStream << h.type << ' ' << h.seqNum << ' ' << h.length << ' ' << h.checksum << '\n';
                loggingStream.flush();
                if (connection && h.seqNum == startSeqNum)
                {
                    ackAndLog(startSeqNum, clientAddr, len);
                    if (outputStream.is_open())
                    {
                        outputStream.flush();
                        outputStream.close();
                    }
                    connection = false;
                    nextExpectedSeqNum = 0;
                    resend.clear();
                    spdlog::debug("END packet received, connection closed");
                    break;
                }
                spdlog::debug("END packet with wrong seq {}, expected {}", h.seqNum, startSeqNum);
                continue;
            }

            uint8_t *data = receviedPackets.data() + sizeof(PacketHeader);
            if (h.type != DATA)
            {
                spdlog::debug("Unexpected packet type: {}, expected DATA", h.type);
                continue;
            }

            if (n != static_cast<ssize_t>(sizeof(PacketHeader) + h.length))
            {
                spdlog::debug("Packet length mismatch: expected {}, got {}", sizeof(PacketHeader) + h.length, n);
                continue;
            }

            if (crc32(data, h.length) != h.checksum)
            {
                spdlog::debug("Checksum mismatch for seqNum={}: expected {}, got {}", h.seqNum, h.checksum, crc32(data, h.length));
                continue;
            }

            uint32_t N = nextExpectedSeqNum;
            spdlog::debug("Next expected seqNum={}", N);

            loggingStream << h.type << ' ' << h.seqNum << ' ' << h.length << ' ' << h.checksum << '\n';
            loggingStream.flush();

            // If it receives a packet with seqNum=N, it will check for the highest sequence number (say M) of the in­order packets it has already received and send ACK with seqNum=M+1.
            if (h.seqNum == N) // what ur expecting, need to deliver the buffer here
            {
                outputStream.write(reinterpret_cast<const char *>(data), h.length);
                ++nextExpectedSeqNum;

                bool restart = true;
                while (restart)
                {
                    restart = false;
                    for (size_t i = 0; i < resend.size(); ++i)
                    {
                        if (resend[i].first == nextExpectedSeqNum)
                        {
                            vector<uint8_t> &currData = resend[i].second;
                            outputStream.write(reinterpret_cast<const char *>(currData.data()),
                                               static_cast<std::streamsize>(currData.size()));
                            ++nextExpectedSeqNum;
                            resend.erase(resend.begin() + i);
                            restart = true;
                            break;
                        }
                    }
                }
                spdlog::debug("Sending ACK for seqNum={}", h.seqNum);
                ackAndLog(h.seqNum, clientAddr, len);
                // deliver the actual buffer not sure how we wanna implement that
            }
            else if ((h.seqNum < N) || (h.seqNum >= N + window_size))
            { // You get an older duplicate packet or way ahead of what you want, just drop it and reack
              // do nothing here
            }
            else if (h.seqNum > N && h.seqNum < N + window_size) // get something ahead of what you want but still in range
            {
                /// need to buffer this packet for later use, add the buffer here
                bool alreadyAcked = false;
                for (const auto &p : resend)
                {
                    if (p.first == h.seqNum)
                    {
                        alreadyAcked = true;
                        break;
                    }
                }
                if (!alreadyAcked)
                {
                    vector<uint8_t> buffer(data, data + h.length);
                    resend.emplace_back(h.seqNum, buffer);
                }
                spdlog::debug("Sending DUP ACK for seqNum={}", N);
                ackAndLog(h.seqNum, clientAddr, len);
            }
        }
    }
};

int main(int argc, char **argv)
{

    ios_base::sync_with_stdio(false);
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("wReceivers started");

    wReceiver receiver;
    receiver.parseArguments(argc, argv);
    spdlog::debug("Arguments parsed successfully");
    receiver.bindSocket();
    spdlog::debug("Socket bound successfully");
    while (true)
    {
        spdlog::debug("Waiting for new connection...");
        receiver.startProtocol();
        spdlog::debug("Connection established, handling data...");
        receiver.handleData();
        spdlog::debug("File transfer complete, waiting for new connection...");
    }

    return 0;
}
