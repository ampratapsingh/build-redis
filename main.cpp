#include <iostream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

vector<string> parseArgs(const string& line) {
    vector<string> args;
    string current;
    bool inQuotes = false;
    for (char ch : line) {
        if (ch == '"' && !inQuotes) { inQuotes = true; }
        else if (ch == '"' && inQuotes) { inQuotes = false; }
        else if (ch == ' ' && !inQuotes) {
            if (!current.empty()) { args.push_back(current); current.clear(); }
        } else { current += ch; }
    }
    if (!current.empty()) args.push_back(current);
    return args;
}

string toUpper(string s) {
    for (auto& c : s) c = toupper(c);
    return s;
}

string encodeBulkString(const string& s) {
    return "$" + to_string(s.size()) + "\r\n" + s + "\r\n";
}

string handleCommand(const vector<string>& args) {
    string cmd = toUpper(args[0]);

    if (cmd == "PING") {
        // TODO: Return "+PONG\r\n" for no args
        // TODO: Return bulk string for PING <message>
    }

    return "-ERR unknown command\r\n";
}

int main() {
    string line;
    while (getline(cin, line)) {
        // trim trailing \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto args = parseArgs(line);
        cout << handleCommand(args);
        cout.flush();
    }
    return 0;
}
