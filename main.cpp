#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
using namespace std;

unordered_map<string, string> store;

string eb(const string &s) { return "$" + to_string(s.size()) + "\r\n" + s + "\r\n"; }
string ebnull() { return "$-1\r\n"; }
string es(const string &s) { return "+" + s + "\r\n"; }
string ee(const string &m) { return "-" + m + "\r\n"; }
string ei(int n) { return ":" + to_string(n) + "\r\n"; }

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

string handle(const vector<string> &args)
{
    string cmd = toUpper(args[0]);

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
        for (size_t i = 3; i < args.size(); i++)
        {
            string flag = toUpper(args[i]);
            if (flag == "NX")
                nx = true;
            else if (flag == "XX")
                xx = true;
        }
        if (nx && store.count(key))
            return ebnull();
        if (xx && !store.count(key))
            return ebnull();
        store[key] = val;
        return es("OK");
    }
    if (cmd == "GET")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'GET' command");
        auto it = store.find(args[1]);
        if (it == store.end())
            return ebnull();
        return eb(it->second);
    }
    if (cmd == "DBSIZE")
    {
        return ei((int)store.size());
    }

    if (cmd == "INCR")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'INCR' command");
        int val = 0;
        if (store.count(args[1]))
        {
            try
            {
                val = stoi(store[args[1]]);
            }
            catch (...)
            {
                return ee("ERR value is not an integer or out of range");
            }
        }
        val++;
        store[args[1]] = to_string(val);
        return ei(val);
    }

    if (cmd == "DECR")
    {
        if (args.size() != 2)
            return ee("ERR wrong number of arguments for 'DECR' command");
        int val = 0;
        if (store.count(args[1]))
        {
            try
            {
                val = stoi(store[args[1]]);
            }
            catch (...)
            {
                return ee("ERR value is not an integer or out of range");
            }
        }
        val--;
        store[args[1]] = to_string(val);
        return ei(val);
    }

    if (cmd == "INCRBY")
    {
        if (args.size() != 3)
            return ee("ERR wrong number of arguments for 'INCRBY' command");
        int amount = 0, val = 0;
        try
        {
            amount = stoi(args[2]);
        }
        catch (...)
        {
            return ee("ERR value is not an integer or out of range");
        }

        if (store.count(args[1]))
        {
            try
            {
                val = stoi(store[args[1]]);
            }
            catch (...)
            {
                return ee("ERR value is not an integer or out of range");
            }
        }
        val += amount;
        store[args[1]] = to_string(val);
        return ei(val);
    }

    if (cmd == "DECRBY")
    {
        if (args.size() != 3)
            return ee("ERR wrong number of arguments for 'DECRBY' command");
        int amount = 0, val = 0;
        try
        {
            amount = stoi(args[2]);
        }
        catch (...)
        {
            return ee("ERR value is not an integer or out of range");
        }

        if (store.count(args[1]))
        {
            try
            {
                val = stoi(store[args[1]]);
            }
            catch (...)
            {
                return ee("ERR value is not an integer or out of range");
            }
        }
        val -= amount;
        store[args[1]] = to_string(val);
        return ei(val);
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
}
