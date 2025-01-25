//////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024  Rohit Jairaj Singh (rohit@singh.org.in)          //
//                                                                      //
// This program is free software: you can redistribute it and/or modify //
// it under the terms of the GNU General Public License as published by //
// the Free Software Foundation, either version 3 of the License, or    //
// (at your option) any later version.                                  //
//                                                                      //
// This program is distributed in the hope that it will be useful,      //
// but WITHOUT ANY WARRANTY; without even the implied warranty of       //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        //
// GNU General Public License for more details.                         //
//                                                                      //
// You should have received a copy of the GNU General Public License    //
// along with this program.  If not, see <https://www.gnu.org/licenses/ //
//////////////////////////////////////////////////////////////////////////

#include <rohit/serializer.h>
#include <rohit/serializercreator.h>
#include <fstream>

void DisplayHelp(const std::string &err) {
    std::cout << "Usage: Serializer input <input filename> output <output filename>" << std::endl;
    if (!err.empty()) {
        std::cout << "Error: " << err << std::endl;
    }
}

std::pair<char *, size_t> ReadBufferFromFile(const std::filesystem::path &path) {
    if (!std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument { "Not a valid file" };
    }
    std::ifstream filestream { path };

    filestream.seekg(0, std::ios::end);
    size_t size = filestream.tellg();
    filestream.seekg(0, std::ios::beg);

    auto buffer = new char[size];
    filestream.read(buffer, size);
    size = filestream.gcount();

    filestream.close();
    return { buffer, size };
}

void WriteBufferToFile(const std::filesystem::path &path, const rohit::FullStream &stream) {
    std::ofstream filestream { path };
    filestream.write(reinterpret_cast<const char *>(stream.begin()), stream.CurrentOffset());
    filestream.close();
}

std::ostream &operator<<(std::ostream &os, const std::vector<std::string> &strlist) {
    os << "{ ";
    for(auto &str: strlist) {
        os << str << ' ';
    }
    os << '}';
    return os;
}

int main(const int argc, const char *argv[]) {
    const std::vector<std::string> args {argv, argv + argc};
    std::filesystem::path input_file { };
    std::filesystem::path output_file { };
    for(size_t argi { 0 }; argi < args.size(); ++argi) {
        if (args[argi] == "input") {
            ++argi;
            if (argi >= args.size()) {
                DisplayHelp("Insufficient arguments");
                return 0;
            }
            input_file = std::filesystem::path { args[argi] };
            if (!std::filesystem::exists(input_file)) {
                DisplayHelp("input file does not exists");
                return 0;
            }
            std::cout << "Input File: " << input_file << std::endl;
        } else if (args[argi] == "output") {
            ++argi;
            if (argi >= args.size()) {
                DisplayHelp("Insufficient arguments");
                return 0;
            }
            output_file = std::filesystem::path { args[argi] };
            std::cout << "Output File: " << output_file << std::endl;
        }
    }

    if (input_file.empty() || output_file.empty()) {
        DisplayHelp("Input and output both parameters are required.");
        std::cout << "Param: " << args << std::endl;
        return 0;
    }

    auto [buffer, size] = ReadBufferFromFile(input_file);

    const rohit::FullStream inStream {buffer, size};
    rohit::FullStreamAutoAlloc outStream {256};

    bool OutputIsHeader = output_file.extension() == ".h" || output_file.extension() == ".hpp" || output_file.extension() == ".hxx";
    if (!OutputIsHeader) {
        std::cout << "WARNING: Output file is designed for C++ header, output extension must be one of .h, .hpp or .hxx" << std::endl;
    }

    try {
        auto statementlist = rohit::serializer::Parser::Parse(inStream);
        rohit::serializer::Writer::CPP::Write(outStream, statementlist);
        WriteBufferToFile(output_file, outStream);
    } catch(const std::exception &e) {
        std::cout << "Failed to parse with error:\n" << e.what() << std::endl;
    }

    return 0;
}