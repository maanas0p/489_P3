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
    unsigned int type;
    unsigned int seqNum;
    unsigned int length;
    unsigned int checksum;
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

    int startSeqNum = -1;
    int nextExpectedSeqNum = 0;

    vector<uint8_t> makePacket(uint32_t type, uint32_t seq, const uint8_t *data, size_t packLen)
    {

        PacketHeader h{type, seq, static_cast<uint32_t>(packLen), (type == DATA) ? crc32(data, packLen) : 0};
        vector<uint8_t> buff(sizeof(PacketHeader) + packLen);
        memcpy(buff.data(), &h, sizeof(h));
        if (packLen > 0)
            memcpy(buff.data() + sizeof(h), data, packLen);
        return buff;
    }

    void bindSocket()
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        receiverAddr.sin_family = AF_INET;
        receiverAddr.sin_addr.s_addr = INADDR_ANY;
        receiverAddr.sin_port = htons(port);

        if (bind(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
        {
            perror("bind failed");
            close(sockfd);
            return;
        }

        spdlog::debug("Socket successfully created and bound to port {}", port);
    }

    void parseArguments(int argc, char **argv)
    {
        cxxopts::Options opts("wReceiver");
        opts.add_options()("p,port", "The port number on which wReceiver is listening", cxxopts::value<int>())("w,window-size", "Maximum number of outstanding packets in the current window.", cxxopts::value<int>())("d,output-dir", "Path to the file that has to be transferred. It can be a text file or binary file.", cxxopts::value<string>())("o,output-log", "The file path to which you should log the messages as described above.", cxxopts::value<string>());
        // -p | --port The port number on which wReceiver is listening for data.
        // -w | --window-size Maximum number of outstanding packets.
        // -d | --output-dir The directory that the wReceiver will store the output files, i.e the FILE-i.out files.
        // -o | --output-log The file path to which you should log the messages as described above.

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

        spdlog::debug("Arguments parsed successfully: port={}, window_size={}, output_dir={}, output_log={}", port, window_size, output_dir, output_log);
    }

    void ackAndLog(uint32_t seqNum, sockaddr_in &clientAddr, socklen_t &len)
    {
        vector<uint8_t> ackPkt = makePacket(ACK, seqNum, nullptr, 0);
        sendto(sockfd, ackPkt.data(), ackPkt.size(), 0,
               (struct sockaddr *)&receiverAddr, sizeof(receiverAddr));

        PacketHeader ack{};
        memcpy(&ack, ackPkt.data(), sizeof(ack));
        loggingStream << ack.type << ' ' << ack.seqNum << ' ' << ack.length << ' ' << ack.checksum << '\n';
        loggingStream.flush();
    }

    void startProtocol()
    {
        vector<uint8_t> receivedPktHeader(sizeof(PacketHeader));
        while (true)
        {
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            ssize_t n = recvfrom(sockfd, rx.data(), sizeof(receivedPktHeader), 0,
                                 (struct sockaddr *)&clientAddr, &len);

            PacketHeader h{};
            memcpy(&h, receivedPktHeader.data(), sizeof(h));
            loggingStream << h.type << ' ' << h.seqNum << ' ' << h.length << ' ' << h.checksum << '\n';
            loggingStream.flush();

            if (h.type != START)
            {
                continue;
            }

            connection = true;
            startSeqNum = h.seqNum;
            nextExpectedSeqNum = 0;

            string filename = "FILE-" + to_string(fileNum) + ".out";
            outputStream.open(filename, ios::binary | ios::trunc);
            fileNum++;

            ackAndLog(startSeqNum, clientAddr, len);
        }
    }

    void handleData()
    {
        if (!connection)
            return;

        vector<uint8_t> receviedPackets(sizeof(PacketHeader) + 1456);
        while (true)
        {
            sockaddr_in clientAddr{};
            socklen_t len = sizeof(clientAddr);
            ssize_t n = recvfrom(sockfd, rx.data(), sizeof(receviedPackets), 0,
                                 (struct sockaddr *)&clientAddr, &len);

            PacketHeader h{};
            memcpy(&h, receviedPackets.data(), sizeof(h));
            loggingStream << h.type << ' ' << h.seqNum << ' ' << h.length << ' ' << h.checksum << '\n';
            loggingStream.flush();

            if (h.type != DATA)
            {
                continue;
            }
            if (n != static_cast<ssize_t>(sizeof(PacketHeader) + h.length))
            {
                continue;
            }

            uint8_t *data = rx.data() + sizeof(PacketHeader);
            if (crc32(data, h.length) != h.checksum)
            {
                continue;
            }

            uint32_t N = nextExpectedSeqNum;

            // If it receives a packet with seqNum not equal to N, it will send back an ACK with seqNum=N.
            if (h.seqNum != N)
            {
            }
            // If it receives a packet with seqNum=N, it will check for the highest sequence number (say M) of the in­order packets it has already received and send ACK with seqNum=M+1.
            else if (h.seqNum == N)
            {
            }
        }
    }
};

int main(int argc, char **argv)
{

    ios_base::sync_with_stdio(false);
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("miProxy started");

    wReceiver receiver;
    receiver.parseArguments(argc, argv);
    receiver.bindSocket();
    receiver.startProtocol();

    return 0;
}