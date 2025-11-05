#include <iostream>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmath>
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
using Clock = chrono::high_resolution_clock;
using ms = chrono::milliseconds;

class wSender
{
public:
    string hostname;
    int port;
    int window_size;
    string input_file;
    string output_log;

    int sockfd = -1;
    sockaddr_in serverAddr{};
    socklen_t len = sizeof(serverAddr);

    ofstream outputStream;

    uint32_t firstInWindow = 0;
    uint32_t nextSeqNum = 0;
    uint32_t startSeq = 0;

    Clock::time_point endTime{};

    enum : uint32_t
    {
        START = 0,
        END = 1,
        DATA = 2,
        ACK = 3
    };

    struct PacketHeader
    {
        uint32_t type;     // 0: START; 1: END; 2: DATA; 3: ACK
        uint32_t seqNum;   // Described below
        uint32_t length;   // Length of data; 0 for ACK packets
        uint32_t checksum; // 32-bit CRC
    };

    vector<vector<uint8_t>> dataPkts;

    int parseArguments(int argc, char **argv)
    {
        cxxopts::Options opts("wSender");
        opts.add_options()("h,hostname", "The IP address of the host that wReceiver is running on.", cxxopts::value<string>())("p,port", "The port number on which wReceiver is listening", cxxopts::value<int>())("w,window-size", "Maximum number of outstanding packets in the current window.", cxxopts::value<int>())("i,input-file", "Path to the file that has to be transferred. It can be a text file or binary file.", cxxopts::value<string>())("o,output-log", "The file path to which you should log the messages as described above.", cxxopts::value<string>());
        //-h | --hostname The IP address of the host that wReceiver is running on.
        // -p | --port The port number on which wReceiver is listening.
        // -w | --window-size Maximum number of outstanding packets in the current window.
        // -i | --input-file Path to the file that has to be transferred. It can be a text file or binary file (e.g., image or video).
        // -o | --output-log The file path to which you should log the messages as described above.
        // ./wSender -h 127.0.0.1 -p 8000 -w 10 -i input.in -o sender.out

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

        if (!result.count("hostname") || !result.count("port") || !result.count("input-file") || !result.count("output-log") || !result.count("window-size"))
        {
            cerr << "Error: Missing required arguments\n\n"
                 << opts.help() << "\n";
            return 1;
        }

        hostname = result["hostname"].as<string>();
        port = result["port"].as<int>();
        window_size = result["window-size"].as<int>();
        input_file = result["input-file"].as<string>();
        output_log = result["output-log"].as<string>();

        if (port < 1024 || port > 65535)
        {
            spdlog::error("Error: port number must be in the range of [1024, 65535]\n");
            return 1;
        }

        outputStream.open(output_log, ios::out | ios::trunc);
        return 0;
    }

    void readFile()
    {
        ifstream is(input_file, ios::binary);
        is.seekg(0, ios::end);
        int length = is.tellg();
        is.seekg(0, ios::beg);

        vector<unsigned char> buffer(length);

        spdlog::debug("reading {} bytes...", length);
        is.read(reinterpret_cast<char *>(buffer.data()), length);
        if (is)
        {
            spdlog::debug("all bytes read successfully1.\n");
        }
        else
        {
            spdlog::debug("error: only {} bytes could be read", static_cast<size_t>(is.gcount()));
        }
        is.close();

        spdlog::debug("closed the input file.");

        size_t numChunksNeeded = (length + 1456 - 1) / 1456;
        dataPkts.resize(numChunksNeeded);
        spdlog::debug("Preparing {} data packets...", numChunksNeeded);
        for (size_t i = 0; i < numChunksNeeded; i++)
        {
            spdlog::debug("Preparing packet {}", i);
            size_t offset = i * 1456;
            auto pkt = makePacket(DATA, static_cast<uint32_t>(i), buffer.data() + offset, min(static_cast<unsigned int>(1456), static_cast<unsigned int>(length - offset)));
            dataPkts[i] = pkt;
            spdlog::debug("Packet {} has total size {}, packet SeqNum {}", i, dataPkts[i].size(), static_cast<uint32_t>(i));
        }
        spdlog::debug("Prepared {} data packets", numChunksNeeded);
    }

    vector<uint8_t> makePacket(uint32_t type, uint32_t seq, const uint8_t *data, size_t packLen)
    {

        PacketHeader h{type, seq, static_cast<uint32_t>(packLen), (type == DATA) ? crc32(data, packLen) : 0};
        vector<uint8_t> buff(sizeof(PacketHeader) + packLen);
        memcpy(buff.data(), &h, sizeof(h));
        if (packLen > 0)
            memcpy(buff.data() + sizeof(h), data, packLen);
        return buff;
    }

    int createSocket()
    {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
        {
            perror("socket failed");
            return 1;
        }

        // 2. Define server address
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, hostname.c_str(), &serverAddr.sin_addr) <= 0)
        {
            spdlog::error("Invalid address/ Address not supported: {}", hostname);
            return -1;
        }

        return 0;
    }

    void sendData(const vector<uint8_t> &bytes)
    {
        size_t currLen = sizeof(serverAddr);
        int sent = sendto(sockfd, bytes.data(), bytes.size(), 0,
                          (struct sockaddr *)&serverAddr, currLen);
        spdlog::debug("Actually sent {} bytes", sent);
        PacketHeader currHeader{};
        memcpy(&currHeader, bytes.data(), sizeof(currHeader));
        outputStream << currHeader.type << ' ' << currHeader.seqNum << ' ' << currHeader.length << ' ' << currHeader.checksum << '\n';
        outputStream.flush();
    }

    bool recvData(PacketHeader &ack)
    {
        for (;;)
        {
            socklen_t currLen = sizeof(serverAddr);
            uint8_t buffer[sizeof(PacketHeader)];
            ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT,
                                 (struct sockaddr *)&serverAddr, &currLen);

            if (Clock::now() >= endTime)
            {
                return false;
            }
            if (n >= static_cast<ssize_t>(sizeof(PacketHeader)))
            {
                memcpy(&ack, buffer, sizeof(PacketHeader));
                return true;
            }
        }
    }

    void sendStartPacket()
    {
        random_device rd;
        mt19937 r(rd());
        uniform_int_distribution<uint32_t> range;
        startSeq = range(r);
        vector<uint8_t> startPkt = makePacket(START, startSeq, nullptr, 0);
        while (true)
        {
            sendData(startPkt);
            endTime = Clock::now() + ms(500);
            PacketHeader ack{};
            spdlog::debug("Sent START packet with seq={}", startSeq);
            if (recvData(ack) && ack.type == ACK && ack.seqNum == startSeq)
            {
                spdlog::debug("START handshake complete (seq={})", startSeq);
                break;
            }
        }
    }

    void sendCurrWindow()
    {
        while ((firstInWindow + window_size) > nextSeqNum && nextSeqNum < dataPkts.size())
        {
            spdlog::debug("Sending DATA packet with size={}", dataPkts[nextSeqNum].size());
            sendData(dataPkts[nextSeqNum]);
            nextSeqNum++;
        }
        if (firstInWindow < nextSeqNum)
            endTime = Clock::now() + ms(500);
    }

    void resendCurrWindow()
    {
        for (int i = firstInWindow; i < nextSeqNum; i++)
        {
            sendData(dataPkts[i]);
        }
        endTime = Clock::now() + ms(500);
    }

    void sendAllDataPackets()
    {
        firstInWindow = 0;
        nextSeqNum = 0;
        sendCurrWindow();

        while (firstInWindow < dataPkts.size())
        {
            PacketHeader ack{};
            if (recvData(ack))
            {
                if (ack.type == ACK && ack.seqNum >= firstInWindow && ack.seqNum <= nextSeqNum)
                {
                    size_t prev = firstInWindow;
                    firstInWindow = min(ack.seqNum, nextSeqNum);

                    if (firstInWindow > prev)
                    {
                        sendCurrWindow();
                    }
                }
            }
            else
            {
                spdlog::debug("reached timeout");
                resendCurrWindow();
            }
        }
    }
    void sendEndPacket()
    {
        vector<uint8_t> endPkt = makePacket(END, startSeq, nullptr, 0);
        while (true)
        {
            sendData(endPkt);
            endTime = Clock::now() + ms(500);
            PacketHeader ack{};
            if (recvData(ack) && ack.type == ACK)
            {
                spdlog::debug("END handshake complete");
                break;
            }
        }
    }
};
int main(int argc, char **argv)
{

    ios_base::sync_with_stdio(false);
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("wSender started");

    wSender sender;
    sender.parseArguments(argc, argv);
    sender.readFile();
    spdlog::debug("Read file and prepared {} data packets", sender.dataPkts.size());
    sender.createSocket();
    spdlog::debug("Socket created");
    sender.sendStartPacket();
    spdlog::debug("START packet sent and acknowledged");
    sender.sendAllDataPackets();
    spdlog::debug("All DATA packets sent and acknowledged");
    sender.sendEndPacket();
    spdlog::debug("END packet sent and acknowledged");

    return 0;
}