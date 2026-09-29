#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

unordered_map<string,string> store;

string eb(const string& s) { return "$" + to_string(s.size()) + "\r\n" + s + "\r\n"; }
string ebnull() { return "$-1\r\n"; }
string es(const string& s) { return "+" + s + "\r\n"; }
string ee(const string& m) { return "-" + m + "\r\n"; }
string ei(int n) { return ":" + to_string(n) + "\r\n"; }

string toUpper(string s) { for(auto&c:s)c=toupper(c); return s; }

vector<string> parseArgs(const string& line) {
    vector<string> args;
    string cur;
    bool inQ = false;
    for (char ch : line) {
        if (ch == '"' && !inQ) { inQ = true; }
        else if (ch == '"' && inQ) { inQ = false; }
        else if (ch == ' ' && !inQ) { if (!cur.empty()) { args.push_back(cur); cur.clear(); } }
        else { cur += ch; }
    }
    if (!cur.empty()) args.push_back(cur);
    return args;
}

string handle(const vector<string>& args) {
    string cmd = toUpper(args[0]);

    if (cmd == "PING") {
        if (args.size() > 2) return ee("ERR wrong number of arguments for 'PING' command");
        if (args.size() == 1) return es("PONG");
        return eb(args[1]);
    }
    if (cmd == "ECHO") {
        if (args.size() != 2) return ee("ERR wrong number of arguments for 'ECHO' command");
        return eb(args[1]);
    }
    if (cmd == "COMMAND" && args.size() > 1 && toUpper(args[1]) == "DOCS") {
        return es("OK");
    }

    return ee("ERR unknown command '" + args[0] + "'");
}

int main() {
    string line;
    while (getline(cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        cout << handle(parseArgs(line));
        cout.flush();
    }
}
