#include "nect/io.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

namespace {
std::string read_bounded(std::istream& stream) {
    std::string data;
    char c;
    while(stream.get(c)) {
        if(data.size()>=8*1024*1024) throw nect::Error("INPUT_LIMIT","Maximum 8 MiB");
        data.push_back(c);
    }
    if(stream.bad()) throw nect::Error("IO_ERROR","Read failed");
    return data;
}
}

int main(int argc,char** argv) {
    try {
        const std::string mode=argc>1?argv[1]:"--help";
        if(mode=="--help") {
            std::cout<<"nect --demo | --validate | --normalize | --svg | --serve [native-file]\n"
                        "Input for validate/normalize/svg is native JSON on stdin. No file writes or network. "
                        "--serve is JSON-lines, not MCP.\n";
            return 0;
        }
        if(mode=="--demo") {
            std::cout<<nect::encode(nect::demo_document())<<'\n';
            return 0;
        }
        if(mode=="--serve") {
            auto d=nect::demo_document();
            if(argc>2) {
                std::ifstream in(argv[2],std::ios::binary);
                if(!in) throw nect::Error("IO_ERROR","Cannot open native file");
                d=nect::decode(read_bounded(in));
            }
            nect::Session session(std::move(d));
            std::string line;
            char c;
            while(std::cin.get(c)) {
                if(c=='\n') {
                    if(!line.empty()) std::cout<<nect::request(session,line)<<std::endl;
                    line.clear();
                } else {
                    if(line.size()>=8*1024*1024) throw nect::Error("INPUT_LIMIT","Request line exceeds 8 MiB");
                    line+=c;
                }
            }
            if(!line.empty()) std::cout<<nect::request(session,line)<<std::endl;
            return 0;
        }
        if(mode!="--validate"&&mode!="--normalize"&&mode!="--svg")
            throw nect::Error("USAGE","Unknown CLI mode");

        auto d=nect::decode(read_bounded(std::cin));
        if(mode=="--validate") std::cout<<"{\"ok\":true,\"native_version\":\"0.2\"}\n";
        else if(mode=="--normalize") std::cout<<nect::encode(d)<<'\n';
        else {
            if(d.compositions.empty()||d.compositions.front().artboards.empty())
                throw nect::Error("MISSING_ARTBOARD","SVG CLI requires a first artboard");
            std::cout<<nect::export_svg(d,d.compositions.front().id,d.compositions.front().artboards.front().id);
        }
        return 0;
    } catch(const nect::Error& e) {
        std::cerr<<e.code<<": "<<e.what()<<'\n';
        return 2;
    } catch(const std::exception& e) {
        std::cerr<<"INTERNAL_ERROR: "<<e.what()<<'\n';
        return 3;
    }
}
