// VWAP calculation for all stocks at each trading hour 
// given NASDAQ ITCH5.0 protocol file
#include <iostream>
#include <boost/iostreams/device/mapped_file.hpp> // for mmap
#include <boost/program_options.hpp>
#include <fstream>
#include "Vwap.H"

namespace trex{
// prints to file(<hour>.txt) VWAP data for each stock at each trading hour 
// including the market close
void printVwap(const std::string& outputDir){
    std::ofstream outFile(outputDir + formatTime(CURRTIME)+ ".txt");
    outFile << "VWAP at " << CURRTIME << "\n";
    for(size_t locateId = 1; locateId < attributedBuyOrders.size(); ++locateId){
        if(attributedBuyOrders[locateId].empty())
            continue;
        
        uint64_t volume = 0;
        uint64_t priceWtVolume = 0;

        for(auto& order : attributedBuyOrders[locateId]){
            volume += order._qty;
            priceWtVolume += (order._qty * order._price);
        }
        outFile << stockSymbols[locateId] << " " << (double)priceWtVolume/(volume * 10000) << "\n";
    }
    outFile << "-----------------" << "\n";
}

// parses the NASDAQ ITCH 5.0 protocol file.
// calls printVwap at each trading hour
// input: 
//  @start/@end: mmap ptr to start/end of file
//  @output: output dir for VWAP values at each hour
//  @numMsg: number of messages processed
void process(const unsigned char* start, const unsigned char* end, const std::string& output, uintmax_t &numMsg){
    unsigned char* fptr = const_cast<unsigned char*>(start);
    while(fptr <= end){
        numMsg += 1;
        uint16_t msg_size = reinterpret2bytes(fptr);
        unsigned char* msg_start = fptr + (size_t)2;

        switch(msg_start[0]){
            case 'S':
                systemMsg(msg_start);
                break;
            case 'R':
                stockRelatedMsg(msg_start);
                break;
            case 'A': // fallthrough
            case 'F':
                addOrder(msg_start);
                break;
            case 'E':
                orderExec(msg_start);
                break;
            case 'C':
                orderExecPrice(msg_start);
                break;
            case 'X':
                orderCancel(msg_start);
                break;
            case 'D':
                orderDelete(msg_start);
                break;
            case 'U':
                orderReplace(msg_start);
                break;
            case 'P':
                nonCrossTrade(msg_start);
                break;
            case 'Q':
                crossTrade(msg_start);
                break;
            case 'B':
                brokenTrade(msg_start);
                break;
            default:
                break;
        }
        if(END or CURRTIME - LASTPRINTED >= HOUR){
            printVwap(output);
            std::cout << formatTime(CURRTIME) << ":00  processed " 
               << numMsg << " messages" << std::endl;
            LASTPRINTED = CURRTIME;
            // break early at close
            if(END)
                return;
        }
        fptr += 2 + msg_size;
    }   
}
};// trex

// usign memory mapped file for input (mmap) to avoid multiple 
// system calls (as in read()). mmap has fixed overhead of 2-3 
// syscalls which is appropriate for large i/o. 
// We are dealing with a ~4GB input file
int main(int argc, char** argv){
    
    boost::program_options::options_description description("Usage:");
    description.add_options()
        ("help,h", "Display this help message")
        ("input,i", boost::program_options::value<std::string>(), "Input ITCH file")
        ("output,o", boost::program_options::value<std::string>(), "Output directory");

    boost::program_options::variables_map vm;
    boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(description).run(), vm);

    if (vm.count("help") or vm.count("input") == 0 or vm.count("output") == 0){
        std::cout << description;
        return 1;
    }
    boost::program_options::notify(vm);

    std::string inputDir = vm["input"].as<std::string>();
    std::string outputDir = vm["output"].as<std::string>();
    
    boost::iostreams::mapped_file mmap(inputDir, boost::iostreams::mapped_file::readonly);
    const unsigned char* f = reinterpret_cast<const unsigned char*>(mmap.const_data());
    const unsigned char* l = f + mmap.size();
    
    uintmax_t m_numMsgs = 0;
    trex::process(f, l, outputDir, m_numMsgs);
    std::cout << "number of messages = " << m_numMsgs << "\n";
    return 0;
}
