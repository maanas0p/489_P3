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
#include "pugixml.hpp"
#include <unordered_map>
#include <unordered_set>
#include <boost/regex.hpp>

using namespace std;

int main(int argc, char **argv)
{

    ios_base::sync_with_stdio(false);

    cxxopts::Options opts("wSender");
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

    int port = result["port"].as<int>();
    string hostname = result["hostname"].as<string>();
    int window_size = result["window-size"].as<int>();
    string input_size = result["input-size"].as<string>();
    string output_log = result["output-log"].as<string>();

    spdlog::set_level(spdlog::level::debug);
    spdlog::info("miProxy started");

    return 0;
}