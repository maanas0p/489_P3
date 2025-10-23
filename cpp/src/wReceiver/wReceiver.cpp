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

    int port = result["port"].as<int>();
    int window_size = result["window-size"].as<int>();
    string output_dir = result["output-dir"].as<string>();
    string output_log = result["output-log"].as<string>();

    if (port < 1024 || port > 65535)
    {
        spdlog::error("Error: port number must be in the range of [1024, 65535]\n");
        return 1;
    }

    spdlog::set_level(spdlog::level::debug);
    spdlog::info("miProxy started");

    return 0;
}