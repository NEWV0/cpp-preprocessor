#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

    regex include_1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    regex include_2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");

bool ProcessInclude(const string& line, const path& current_path, const vector<path>& include_directories, ofstream& out_file, int line_number) {
    smatch m;
    if (regex_match(line, m, include_1)) {
        path included_file = current_path.parent_path() / m[1].str();
        if (!filesystem::exists(included_file)) {
            bool found = false;
            for (const auto& dir : include_directories) {
                included_file = dir / m[1].str();
                if (filesystem::exists(included_file)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                cout << "unknown include file " << m[1].str() << " at file " << string(current_path) << " at line " << line_number << endl;
                return false;
            }
        }
        ifstream infile(included_file);
        string inner_line;
        while (getline(infile, inner_line)) {
            if (!ProcessInclude(inner_line, included_file, include_directories, out_file, line_number)) {
                return false;
            }
        }
    } else if (regex_match(line, m, include_2)) {
        path included_file;
        bool found = false;
        for (const auto& dir : include_directories) {
            included_file = dir / m[1].str();
            if (filesystem::exists(included_file)) {
                found = true;
                break;
            }
        }
        if (!found) {
            cout << "unknown include file " << m[1].str() << " at file " << string(current_path) << " at line " << line_number << endl;
            return false;
        }
        ifstream infile(included_file);
        string inner_line;
        while (getline(infile, inner_line)) {
            if (!ProcessInclude(inner_line, included_file, include_directories, out_file, line_number)) {
                return false;
            }
        }
    } else {
        out_file << line << endl;
    }
    return true;
}


bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream infile(in_file);
    if (!infile.is_open()) {
        return false;
    }

    ofstream outfile(out_file);
    if (!outfile.is_open()) {
        return false;
    }

    string line;
    int line_number = 1;
    while (getline(infile, line)) {
        if (!ProcessInclude(line, in_file, include_directories, outfile, line_number)) {
            return false;
        }
        line_number++;
    }

    return true;
}


string GetFileContents(const string& file) {
    ifstream stream(file);
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}