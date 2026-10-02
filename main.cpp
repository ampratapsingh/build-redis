#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <deque>
using namespace std;

unordered_map<string,string> store;
unordered_map<string,long long> expiry;
unordered_map<string,deque<string>> lists;
unordered_map<string,map<string,string>> hashes;
unordered_map<string,string> keyType;
long long clockMs = 0;

string eb(const string& s) { return "$" + to_string(s.size()) + "\r\n" + s + "\r\n"; }
string ebnull() { return "$-1\r\n"; }
string es(const string& s) { return "+" + s + "\r\n"; }
string ee(const string& m) { return "-" + m + "\r\n"; }
string ei(int n) { return ":" + to_string(n) + "\r\n"; }
string eil(long long n) { return ":" + to_string(n) + "\r\n"; }
string ea(const vector<string>& items) {
    string r = "*" + to_string(items.size()) + "\r\n";
    for (auto& s : items) r += eb(s);
    return r;
}
string toUpper(string s) { for(auto&c:s)c=toupper(c); return s; }
vector<string> parseArgs(const string& line) {
    vector<string> args; string cur; bool inQ = false;
    for (char ch : line) {
        if (ch == '"' && !inQ) inQ = true;
        else if (ch == '"' && inQ) inQ = false;
        else if (ch == ' ' && !inQ) { if (!cur.empty()) { args.push_back(cur); cur.clear(); } }
        else cur += ch;
    }
    if (!cur.empty()) args.push_back(cur);
    return args;
}
bool isExpired(const string& key) { auto it = expiry.find(key); return it != expiry.end() && clockMs >= it->second; }
void cleanIfExpired(const string& key) {
    if (isExpired(key)) { store.erase(key); expiry.erase(key); lists.erase(key); hashes.erase(key); keyType.erase(key); }
}
bool keyExists(const string& key) { cleanIfExpired(key); return keyType.count(key) > 0; }
string checkType(const string& key, const string& expected) {
    if (keyType.count(key) && keyType[key] != expected)
        return ee("WRONGTYPE Operation against a key holding the wrong kind of value");
    return "";
}
bool tryParseInt(const string& s, long long& out) { try { out = stoll(s); return true; } catch(...) { return false; } }
string incrByVal(const string& key, long long delta) {
    long long cur = 0; auto it = store.find(key);
    if (it != store.end()) { if (!tryParseInt(it->second, cur)) return ee("ERR value is not an integer or out of range"); }
    cur += delta; store[key] = to_string(cur); keyType[key] = "string"; return eil(cur);
}
void removeKeyIfEmpty(const string& key) {
    if (lists.count(key) && lists[key].empty()) { lists.erase(key); keyType.erase(key); }
    if (hashes.count(key) && hashes[key].empty()) { hashes.erase(key); keyType.erase(key); }
}

string handle(const vector<string>& args) {
    string cmd = toUpper(args[0]);
    if (cmd == "WAIT") { clockMs += stoll(args[1]); return es("OK"); }
    if (cmd == "PING") { if (args.size() > 2) return ee("ERR wrong number of arguments for 'PING' command"); if (args.size() == 1) return es("PONG"); return eb(args[1]); }
    if (cmd == "ECHO") { if (args.size() != 2) return ee("ERR wrong number of arguments for 'ECHO' command"); return eb(args[1]); }
    if (cmd == "COMMAND" && args.size() > 1 && toUpper(args[1]) == "DOCS") { return es("OK"); }
    if (cmd == "SET") {
        if (args.size() < 3) return ee("ERR wrong number of arguments for 'SET' command");
        string key = args[1], val = args[2]; cleanIfExpired(key);
        bool nx=false, xx=false; long long exMs=-1;
        for (size_t i=3;i<args.size();i++) { string f=toUpper(args[i]); if(f=="NX")nx=true; else if(f=="XX")xx=true; else if(f=="EX"&&i+1<args.size()){exMs=stoll(args[++i])*1000;} else if(f=="PX"&&i+1<args.size()){exMs=stoll(args[++i]);} }
        bool exists = keyType.count(key)>0; if(nx&&exists)return ebnull(); if(xx&&!exists)return ebnull();
        lists.erase(key); hashes.erase(key); store[key]=val; keyType[key]="string"; expiry.erase(key);
        if(exMs>0) expiry[key]=clockMs+exMs; return es("OK");
    }
    if (cmd == "GET") { cleanIfExpired(args[1]); if(!store.count(args[1]))return ebnull(); return eb(store[args[1]]); }
    if (cmd == "DBSIZE") { int c=0; for(auto&p:keyType) if(!isExpired(p.first))c++; return ei(c); }
    if (cmd == "INCR") { return incrByVal(args[1], 1); }
    if (cmd == "DECR") { return incrByVal(args[1], -1); }
    if (cmd == "INCRBY") { long long v; if(!tryParseInt(args[2],v)) return ee("ERR value is not an integer or out of range"); return incrByVal(args[1],v); }
    if (cmd == "DECRBY") { long long v; if(!tryParseInt(args[2],v)) return ee("ERR value is not an integer or out of range"); return incrByVal(args[1],-v); }
    if (cmd == "EXPIRE") { if(!keyExists(args[1]))return ei(0); expiry[args[1]]=clockMs+stoll(args[2])*1000; return ei(1); }
    if (cmd == "TTL") { cleanIfExpired(args[1]); if(!keyExists(args[1]))return ei(-2); if(!expiry.count(args[1]))return ei(-1); return eil((expiry[args[1]]-clockMs+999)/1000); }
    if (cmd == "PTTL") { cleanIfExpired(args[1]); if(!keyExists(args[1]))return ei(-2); if(!expiry.count(args[1]))return ei(-1); return eil(expiry[args[1]]-clockMs); }
    if (cmd == "PERSIST") { if(expiry.count(args[1])&&!isExpired(args[1])){expiry.erase(args[1]);return ei(1);} return ei(0); }
    if (cmd == "EXISTS") { int c=0; for(size_t i=1;i<args.size();i++) if(keyExists(args[i]))c++; return ei(c); }
    if (cmd == "LPUSH") { string k=args[1]; cleanIfExpired(k); string t=checkType(k,"list"); if(!t.empty())return t; if(!lists.count(k))lists[k]=deque<string>(); for(size_t i=2;i<args.size();i++)lists[k].push_front(args[i]); keyType[k]="list"; return ei((int)lists[k].size()); }
    if (cmd == "RPUSH") { string k=args[1]; cleanIfExpired(k); string t=checkType(k,"list"); if(!t.empty())return t; if(!lists.count(k))lists[k]=deque<string>(); for(size_t i=2;i<args.size();i++)lists[k].push_back(args[i]); keyType[k]="list"; return ei((int)lists[k].size()); }
    if (cmd == "LPOP") { string k=args[1]; cleanIfExpired(k); if(!lists.count(k)||lists[k].empty())return ebnull(); string v=lists[k].front(); lists[k].pop_front(); removeKeyIfEmpty(k); return eb(v); }
    if (cmd == "RPOP") { string k=args[1]; cleanIfExpired(k); if(!lists.count(k)||lists[k].empty())return ebnull(); string v=lists[k].back(); lists[k].pop_back(); removeKeyIfEmpty(k); return eb(v); }
    if (cmd == "LLEN") { string k=args[1]; cleanIfExpired(k); if(!lists.count(k))return ei(0); return ei((int)lists[k].size()); }
    if (cmd == "LRANGE") {
        string k=args[1]; cleanIfExpired(k); if(!lists.count(k))return "*0\r\n";
        auto&d=lists[k]; int len=(int)d.size(); int start=stoi(args[2]),stop=stoi(args[3]);
        if(start<0)start+=len; if(stop<0)stop+=len; if(start<0)start=0; if(stop>=len)stop=len-1;
        if(start>stop) return "*0\r\n";
        vector<string> res; for(int i=start;i<=stop;i++)res.push_back(d[i]); return ea(res);
    }
    if (cmd == "HSET") {
        string k=args[1]; cleanIfExpired(k);
        string t=checkType(k,"hash"); if(!t.empty())return t;
        if(!hashes.count(k)) hashes[k]=map<string,string>();
        int added=0;
        for(size_t i=2;i+1<args.size();i+=2) {
            if(!hashes[k].count(args[i])) added++;
            hashes[k][args[i]]=args[i+1];
        }
        keyType[k]="hash"; return ei(added);
    }
    if (cmd == "HGET") {
        string k=args[1]; cleanIfExpired(k);
        if(!hashes.count(k)||!hashes[k].count(args[2])) return ebnull();
        return eb(hashes[k][args[2]]);
    }


    if (cmd == "HDEL") {
        if (args.size() < 3) return ee("ERR wrong number of arguments for 'HDEL' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "hash");
        if (!terr.empty()) return terr;

        if (!hashes.count(key)) return ei(0);

        int removed = 0;
        for (size_t i = 2; i < args.size(); i++) {
            if (hashes[key].erase(args[i])) {
                removed++;
            }
        }

        removeKeyIfEmpty(key);
        return ei(removed);
    }

    if (cmd == "HGETALL") {
        if (args.size() != 2) return ee("ERR wrong number of arguments for 'HGETALL' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "hash");
        if (!terr.empty()) return terr;

        if (!hashes.count(key)) return "*0\r\n";

        vector<string> flat;
        for (const auto& [field, value] : hashes[key]) {
            flat.push_back(field);
            flat.push_back(value);
        }
        return ea(flat);
    }

    if (cmd == "HEXISTS") {
        if (args.size() != 3) return ee("ERR wrong number of arguments for 'HEXISTS' command");
        string key = args[1];
        string field = args[2];
        cleanIfExpired(key);

        string terr = checkType(key, "hash");
        if (!terr.empty()) return terr;

        if (hashes.count(key) && hashes[key].count(field)) {
            return ei(1);
        }
        return ei(0);
    }

    if (cmd == "HLEN") {
        if (args.size() != 2) return ee("ERR wrong number of arguments for 'HLEN' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "hash");
        if (!terr.empty()) return terr;

        if (!hashes.count(key)) return ei(0);

        return eil(hashes[key].size());
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
