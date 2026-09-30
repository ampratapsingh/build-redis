#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

unordered_map<string, string> store;
unordered_map<string, long long> expiry;
long long clockMs = 0;

string eb(const string &s) { return "$" + to_string(s.size()) + "\r\n" + s + "\r\n"; }
string ebnull() { return "$-1\r\n"; }
string es(const string &s) { return "+" + s + "\r\n"; }
string ee(const string &m) { return "-" + m + "\r\n"; }
string ei(int n) { return ":" + to_string(n) + "\r\n"; }
string eil(long long n) { return ":" + to_string(n) + "\r\n"; }

string toUpper(string s)
{
    for (auto &c : s)
        c = toupper(c);
    return s;
}

vector<string> parseArgs(const string &line)
{
    vector<string> args;
    string cur;
    bool inQ = false;
    for (char ch : line)
    {
        if (ch == '"' && !inQ)
        {
            inQ = true;
        }
        else if (ch == '"' && inQ)
        {
            inQ = false;
        }
        else if (ch == ' ' && !inQ)
        {
            if (!cur.empty())
            {
                args.push_back(cur);
                cur.clear();
            }
        }
        else
        {
            cur += ch;
        }
    }
    if (!cur.empty())
        args.push_back(cur);
    return args;
}

bool isExpired(const string &key)
{
    auto it = expiry.find(key);
    if (it == expiry.end())
        return false;
    return clockMs >= it->second;
}

// Implement passive (lazy) expiry cleanup
void cleanupIfExpired(const string &key)
{
    if (isExpired(key))
    {
        store.erase(key);
        expiry.erase(key);
    }
}

bool keyExists(const string &key)
{
    cleanupIfExpired(key);
    return store.count(key) != 0;
}

bool tryParseInt(const string &s, long long &out)
{
    try
    {
        out = stoll(s);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

string incrByVal(const string &key, long long delta)
{
    cleanupIfExpired(key);
    long long cur = 0;
    auto it = store.find(key);
    if (it != store.end())
    {
        if (!tryParseInt(it->second, cur))
            return ee("ERR value is not an integer or out of range");
    }
    cur += delta;
    store[key] = to_string(cur);
    return eil(cur);
}

string handle(const vector<string> &args)
{
    if (args.empty())
        return "";
    string cmd = toUpper(args[0]);

    if (cmd == "WAIT")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'WAIT' command");
        clockMs += stoll(args[1]);
        return es("OK");
    }
    if (cmd == "PING")
    {
        if (args.size() > 2)
            return ee("ERR wrong number of arguments for 'PING' command");
        if (args.size() == 1)
            return es("PONG");
        return eb(args[1]);
    }
    if (cmd == "ECHO")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'ECHO' command");
        return eb(args[1]);
    }
    if (cmd == "COMMAND" && args.size() > 1 && toUpper(args[1]) == "DOCS")
    {
        return es("OK");
    }
    if (cmd == "SET")
    {
        if (args.size() < 3)
            return ee("ERR wrong number of arguments for 'SET' command");
        string key = args[1], val = args[2];
        bool nx = false, xx = false;
        long long exMs = -1;
        for (size_t i = 3; i < args.size(); i++)
        {
            string flag = toUpper(args[i]);
            if (flag == "NX")
                nx = true;
            else if (flag == "XX")
                xx = true;
            else if (flag == "EX" && i + 1 < args.size())
            {
                exMs = stoll(args[++i]) * 1000;
            }
            else if (flag == "PX" && i + 1 < args.size())
            {
                exMs = stoll(args[++i]);
            }
        }
        bool exists = keyExists(key);
        if (nx && exists)
            return ebnull();
        if (xx && !exists)
            return ebnull();
        store[key] = val;
        expiry.erase(key);
        if (exMs > 0)
            expiry[key] = clockMs + exMs;
        return es("OK");
    }
    if (cmd == "GET")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'GET' command");
        cleanupIfExpired(args[1]);
        if (!keyExists(args[1]))
            return ebnull();
        return eb(store[args[1]]);
    }
    if (cmd == "DBSIZE")
    {
        int cnt = 0;
        for (auto &p : store)
        {
            if (!isExpired(p.first))
                cnt++;
        }
        return ei(cnt);
    }
    if (cmd == "INCR")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'INCR' command");
        return incrByVal(args[1], 1);
    }
    if (cmd == "DECR")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'DECR' command");
        return incrByVal(args[1], -1);
    }
    if (cmd == "INCRBY")
    {
        if (args.size() != 3)
            return ee("ERR wrong number of arguments for 'INCRBY' command");
        long long v;
        if (!tryParseInt(args[2], v))
            return ee("ERR value is not an integer or out of range");
        return incrByVal(args[1], v);
    }
    if (cmd == "DECRBY")
    {
        if (args.size() != 3)
            return ee("ERR wrong number of arguments for 'DECRBY' command");
        long long v;
        if (!tryParseInt(args[2], v))
            return ee("ERR value is not an integer or out of range");
        return incrByVal(args[1], -v);
    }
    if (cmd == "EXPIRE")
    {
        if (args.size() != 3)
            return ee("ERR wrong number of arguments for 'EXPIRE' command");
        if (!keyExists(args[1]))
            return ei(0);
        expiry[args[1]] = clockMs + stoll(args[2]) * 1000;
        return ei(1);
    }
    if (cmd == "TTL")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'TTL' command");
        if (!keyExists(args[1]))
            return ei(-2);
        if (!expiry.count(args[1]))
            return ei(-1);
        long long rem = (expiry[args[1]] - clockMs + 999) / 1000;
        return eil(rem);
    }
    if (cmd == "PTTL")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'PTTL' command");
        if (!keyExists(args[1]))
            return ei(-2);
        if (!expiry.count(args[1]))
            return ei(-1);
        return eil(expiry[args[1]] - clockMs);
    }
    if (cmd == "PERSIST")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'PERSIST' command");
        if (!keyExists(args[1]))
            return ei(0);
        if (expiry.count(args[1]))
        {
            expiry.erase(args[1]);
            return ei(1);
        }
        return ei(0);
    }
    if (cmd == "EXISTS")
    {
        int cnt = 0;
        for (size_t i = 1; i < args.size(); i++)
        {
            cleanupIfExpired(args[i]);
            if (keyExists(args[i]))
                cnt++;
        }
        return ei(cnt);
    }

    return ee("ERR unknown command '" + args[0] + "'");
}

int main()
{
    string line;
    while (getline(cin, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        cout << handle(parseArgs(line));
        cout.flush();
    }
    return 0;
}