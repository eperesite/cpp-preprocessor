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

bool ProcessFile(ifstream& input, const path& file_path, ostream& output, const vector<path>& include_directories);

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input.is_open()) {
        return false;
    }

    ofstream output(out_file);
    if (!output.is_open()) {
        return false;
    }

    bool success = ProcessFile(input, in_file, output, include_directories);
    return success;
}

bool ProcessFile(ifstream& input, const path& file_path, ostream& output, const vector<path>& include_directories) {
    static const regex include_quoted(R"raw(\s*#\s*include\s*"([^"]*)"\s*)raw");
    static const regex include_angled(R"raw(\s*#\s*include\s*<([^>]*)>\s*)raw");

    string line;
    int line_number = 0;
    while (getline(input, line)) {
        ++line_number;
        smatch match;
        if (regex_match(line, match, include_quoted)) {
            path include_path = match[1].str();
            path current_dir = file_path.parent_path();
            path full_path = current_dir / include_path;
            
            ifstream include_file(full_path);
            if (!include_file.is_open()) {
                bool found = false;
                for (const auto& dir : include_directories) {
                    full_path = dir / include_path;
                    include_file.open(full_path);
                    if (include_file.is_open()) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    cout << "unknown include file " << include_path.string() << " at file " << file_path.string() << " at line " << line_number << endl;
                    return false;
                }
            }
            if (!ProcessFile(include_file, full_path, output, include_directories)) {
                return false;
            }
        } else if (regex_match(line, match, include_angled)) {
            path include_path = match[1].str();
            bool found = false;
            path full_path;
            ifstream include_file;

            for (const auto& dir : include_directories) {
                full_path = dir / include_path;
                include_file.open(full_path);
                if (include_file.is_open()) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                cout << "unknown include file " << include_path.string() << " at file " << file_path.string() << " at line " << line_number << endl;
                return false;
            }

            if (!ProcessFile(include_file, full_path, output, include_directories)) {
                return false;
            }
        } else {
            output << line << "\n";
        }
    }
    return true;
}

string GetFileContents(string file) {
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
