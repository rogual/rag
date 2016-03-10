#include <iostream>
#include <fstream>
#include <vector>
#include <cstdio>
#include <sstream>
#include <unistd.h>

#include <boost/program_options.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

namespace po = boost::program_options;
namespace io = boost::iostreams;

using std::string;
using std::vector;
using std::cin;
using std::cout;

enum {
    EXIT_CANCELLED = 2
};

struct Run {
    string find;
    string replace;
    bool color;
};

typedef io::stream<io::file_descriptor_source> PopenStream;

bool popen_stream(PopenStream &results_stream, const char *command)
{
    FILE *file = popen(command, "r");
    if (file) {
        io::file_descriptor_source results(fileno(file), io::close_handle);
        results_stream.open(results);
        return true;
    }
    return false;
}

void transform_line(string &line, const string &find, const string &replace)
{
    int n = find.length();

    for (;;) {
        int pos = line.find(find);
        if (pos == string::npos)
            return;

        line.replace(pos, n, replace);
    }
}

void apply_chunk(const Run &run, const string &filename, const vector<string> &chunk)
{
    string tmpname = filename + "~";

    std::ifstream orig(filename);

    std::ofstream tmp(tmpname);

    int cur_line = 1;

    for (const string &result: chunk) {
        int colon1 = result.find(':');
        int colon2 = result.find(':', colon1);

        string line = result.substr(colon1 + 1);
        string nostr = result.substr(colon1 + 1, colon2 - colon1 - 1);

        string new_line = line;
        transform_line(new_line, run.find, run.replace);

        std::istringstream ss(nostr);
        int no;
        ss >> no;

        string orig_line;
        while (orig) {
            getline(orig, orig_line);

            if (cur_line == no) {

                if (orig_line != line) {
                    cout << "File has changed.\n";
                    exit(EXIT_FAILURE);
                }

                tmp << new_line << "\n";
            }
            else {
                tmp << orig_line << "\n";
            }

            cur_line += 1;
        }

        cout << line << "\n";
    }

    orig.close();
    tmp.close();

    if (rename(tmpname.c_str(), filename.c_str()) != 0) {
        cout << "Rename failed.\n";
        exit(EXIT_FAILURE);
    }
}

void process_chunk(const Run &run, const string &filename, const vector<string> &chunk)
{
    string show_replace;
    if (run.color) {
        show_replace = "\x1b[42;30m" + run.replace + "\x1b[0m";
    }
    else {
        show_replace = run.replace;
    }

    cout << "\n" << filename << "\n";
    for (const string &result: chunk) {
        int colon1 = result.find(':');
        int colon2 = result.find(':', colon1);

        string line = result.substr(colon1 + 1);
        transform_line(line, run.find, show_replace);

        cout << line << "\n";
    }

    for (;;) {
        cout << "[y n q] ";

        string command;
        getline(cin, command);

        if (command == "q")
            exit(EXIT_CANCELLED);

        if (command == "n")
            return;

        if (command == "y") {
            apply_chunk(run, filename, chunk);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    po::options_description desc("OPTIONS");
    desc.add_options()
        ("help", "")
        ("find", po::value<string>())
        ("replace", po::value<string>())
        ("dir", po::value<string>()->default_value("."))
        ;

    po::positional_options_description pos;
    pos.add("find", 1);
    pos.add("replace", 1);
    pos.add("dir", 1);

    auto parse =
        po::command_line_parser(argc, argv).options(desc).positional(pos).run();
    po::variables_map vars;
    po::store(parse, vars);
    po::notify(vars);

    if (vars.count("help")) {
        cout << "rag [OPTIONS] [FIND] [REPLACE] [DIR]\n\n";
        cout << desc << "\n";
        return 1;
    }

    if (vars.count("find") == 0) {
        cout << "nothing to find\n";
        return 1;
    }

    if (vars.count("replace") == 0) {
        cout << "no replacement string given\n";
        return 1;
    }

    Run run;
    run.find = vars["find"].as<string>();
    run.replace = vars["replace"].as<string>();
    run.color = isatty(fileno(stdout));

    string dir = vars["dir"].as<string>();

    PopenStream results_stream;
    const vector<string> try_progs = {"ag", "ack", "grep"};
    for (const string &prog: try_progs) {
        string search_cmd = "ag " + run.find + " " + dir;
        if (popen_stream(results_stream, search_cmd.c_str()))
            break;
    }

    if (!results_stream.good()) {
        cout << "No suitable search program could be run.\n";
        return 1;
    }

    string filename;
    vector<string> chunk;

    while (results_stream) {
        string result;
        getline(results_stream, result);

        if (result == "")
            continue;

        int colon1 = result.find(':');
        int colon2 = result.find(':', colon1);

        if (colon1 == string::npos || colon2 == string::npos) {
            cout << "bad result line:\n";
            cout << result << "\n";
            return 1;
        }

        string new_filename = result.substr(0, colon1);
        if (!filename.empty() && new_filename != filename) {
            process_chunk(run, filename, chunk);
            chunk.clear();
        }

        filename = new_filename;
        chunk.push_back(result);
    }
    process_chunk(run, filename, chunk);
}
