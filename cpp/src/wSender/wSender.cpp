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
#include "pugixml.hpp"
#include <unordered_map>
#include <unordered_set>

using namespace std;

class wSender
{
public:
    string hostname;
    int port;
    int window_size;
    string input_file;
    string output_log;

    struct PacketHeader
    {
        unsigned int type;
        unsigned int seqNum;
        unsigned int length;
        unsigned int checksum;
    };

    struct OutPkt
    {
        PacketHeader header;
        vector<unsigned char> buf;
    };

    void parseResults(argc, argv)
    {
        opts.add_options()("h,hostname", "The IP address of the host that wReceiver is running on.", cxxopts::value<string>())("p,port", "The port number on which wReceiver is listening", cxxopts::value<int>())("w,window-size", "Maximum number of outstanding packets in the current window.", cxxopts::value<int>())("i,input-size", "Path to the file that has to be transferred. It can be a text file or binary file.", cxxopts::value<string>())("o,output-log", "The file path to which you should log the messages as described above.", cxxopts::value<string>());
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

        if (!result.count("hostname") || !result.count("port") || !result.count("input-size") || !result.count("output-log") || !result.count("window-size"))
        {
            cerr << "Error: Missing required arguments\n\n"
                 << opts.help() << "\n";
            return 1;
        }

        if (port < 1024 || port > 65535)
        {
            spdlog::error("Error: port number must be in the range of [1024, 65535]\n");
            return 1;
        }
        hostname = result["hostname"].as<string>();
        port = result["port"].as<int>();
        window_size = result["window-size"].as<int>();
        input_file = result["input-size"].as<string>();
        output_log = result["output-log"].as<string>();
    }

    void readFile()
    {
        ifstream is(input_file, ios::binary);
        if (!is)
            return 1;

        is.seekg(0, ios::end);
        int length = is.tellg();
        is.seekg(0, ios::beg);

        vector<unsigned char> buffer(length);

        spdlog::debug << "Reading " << length << " bytes... ";
        is.read(reinterpret_cast<char *>(buffer.data()), length);
        if (is)
        {
            spdlog::debug << "all bytes read successfully.\n";
        }
        else
        {
            spdlog::debug << "error: only " << is.gcount() << " could be read" << endl;
        }
        is.close();

        size_t numChunksNeeded = static_cast<size_t>(ceil(double(length) / double(1456)));
        vector<OutPkt> dataPkts(numChunksNeeded);
        for (size_t i = 0; i < numChunksNeeded; i++)
        {
            size_t offset = i * 1456;
            size_t len = min(1456, file.size() - offset);
            dataPkts[i] = make_data(i, file.data() + offset, len);
        }

        OutPkt p;
        p.buf.resize(HDR_SZ + len);

        PacketHeader hdr{};
        hdr.type = TYPE_DATA;
        hdr.seqNum = seq;
        hdr.length = len;
        hdr.checksum = crc32(data, len);

        std::memcpy(p.buf.data(), &hdr, sizeof hdr);   // write header bytes
        std::memcpy(p.buf.data() + HDR_SZ, data, len); // write payload
        p.hdr = hdr;                                   // cached typed copy
        return p;

        return 0;
    }
};
int main(int argc, char **argv)
{

    ios_base::sync_with_stdio(false);

    cxxopts::Options opts("wSender");

    spdlog::set_level(spdlog::level::debug);
    spdlog::info("miProxy started");
    wSender sender;
    sender.parseResults(argc, argv);
    sender.readFile();

    return 0;
}