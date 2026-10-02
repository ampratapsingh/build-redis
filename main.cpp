#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <deque>
#include <algorithm>
using namespace std;

unordered_map<string,string> store;
unordered_map<string,long long> expiry;
unordered_map<string,deque<string>> lists;
unordered_map<string,map<string,string>> hashes;
unordered_map<string,unordered_set<string>> sets;
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
    vector<string> args; string cur; bool inQ=false;
    for (char ch:line) { if(ch=='"'&&!inQ)inQ=true; else if(ch=='"'&&inQ)inQ=false; else if(ch==' '&&!inQ){if(!cur.empty()){args.push_back(cur);cur.clear();}} else cur+=ch; }
    if (!cur.empty()) args.push_back(cur); return args;
}
bool isExpired(const string& key) { auto it=expiry.find(key); return it!=expiry.end()&&clockMs>=it->second; }
void cleanIfExpired(const string& key) { if(isExpired(key)){store.erase(key);expiry.erase(key);lists.erase(key);hashes.erase(key);sets.erase(key);keyType.erase(key);} }
bool keyExists(const string& key) { cleanIfExpired(key); return keyType.count(key)>0; }
string checkType(const string& key, const string& expected) {
    if(keyType.count(key)&&keyType[key]!=expected) return ee("WRONGTYPE Operation against a key holding the wrong kind of value"); return "";
}
bool tryParseInt(const string& s, long long& out) { try{out=stoll(s);return true;}catch(...){return false;} }
string incrByVal(const string& key, long long delta) {
    long long cur=0; auto it=store.find(key);
    if(it!=store.end()){if(!tryParseInt(it->second,cur))return ee("ERR value is not an integer or out of range");}
    cur+=delta; store[key]=to_string(cur); keyType[key]="string"; return eil(cur);
}
void removeKeyIfEmpty(const string& key) {
    if(lists.count(key)&&lists[key].empty()){lists.erase(key);keyType.erase(key);}
    if(hashes.count(key)&&hashes[key].empty()){hashes.erase(key);keyType.erase(key);}
    if(sets.count(key)&&sets[key].empty()){sets.erase(key);keyType.erase(key);}
}

string handle(const vector<string>& args) {
    string cmd = toUpper(args[0]);
    if (cmd=="WAIT"){clockMs+=stoll(args[1]);return es("OK");}
    if (cmd=="PING"){if(args.size()>2)return ee("ERR wrong number of arguments for 'PING' command"); if(args.size()==1)return es("PONG"); return eb(args[1]);}
    if (cmd=="ECHO"){if(args.size()!=2)return ee("ERR wrong number of arguments for 'ECHO' command"); return eb(args[1]);}
    if (cmd=="COMMAND"&&args.size()>1&&toUpper(args[1])=="DOCS"){return es("OK");}
    if (cmd=="SET"){
        if(args.size()<3)return ee("ERR wrong number of arguments for 'SET' command");
        string key=args[1],val=args[2]; cleanIfExpired(key);
        bool nx=false,xx=false; long long exMs=-1;
        for(size_t i=3;i<args.size();i++){string f=toUpper(args[i]); if(f=="NX")nx=true; else if(f=="XX")xx=true; else if(f=="EX"&&i+1<args.size()){exMs=stoll(args[++i])*1000;} else if(f=="PX"&&i+1<args.size()){exMs=stoll(args[++i]);}}
        bool exists=keyType.count(key)>0; if(nx&&exists)return ebnull(); if(xx&&!exists)return ebnull();
        lists.erase(key);hashes.erase(key);sets.erase(key); store[key]=val; keyType[key]="string"; expiry.erase(key);
        if(exMs>0)expiry[key]=clockMs+exMs; return es("OK");
    }
    if (cmd=="GET"){cleanIfExpired(args[1]); if(!store.count(args[1]))return ebnull(); return eb(store[args[1]]);}
    if (cmd=="DBSIZE"){int c=0; for(auto&p:keyType) if(!isExpired(p.first))c++; return ei(c);}
    if (cmd=="INCR"){return incrByVal(args[1],1);}
    if (cmd=="DECR"){return incrByVal(args[1],-1);}
    if (cmd=="INCRBY"){long long v; if(!tryParseInt(args[2],v))return ee("ERR value is not an integer or out of range"); return incrByVal(args[1],v);}
    if (cmd=="DECRBY"){long long v; if(!tryParseInt(args[2],v))return ee("ERR value is not an integer or out of range"); return incrByVal(args[1],-v);}
    if (cmd=="EXPIRE"){if(!keyExists(args[1]))return ei(0); expiry[args[1]]=clockMs+stoll(args[2])*1000; return ei(1);}
    if (cmd=="TTL"){cleanIfExpired(args[1]); if(!keyExists(args[1]))return ei(-2); if(!expiry.count(args[1]))return ei(-1); return eil((expiry[args[1]]-clockMs+999)/1000);}
    if (cmd=="PTTL"){cleanIfExpired(args[1]); if(!keyExists(args[1]))return ei(-2); if(!expiry.count(args[1]))return ei(-1); return eil(expiry[args[1]]-clockMs);}
    if (cmd=="PERSIST"){if(expiry.count(args[1])&&!isExpired(args[1])){expiry.erase(args[1]);return ei(1);} return ei(0);}
    if (cmd=="EXISTS"){int c=0; for(size_t i=1;i<args.size();i++) if(keyExists(args[i]))c++; return ei(c);}
    if (cmd=="LPUSH"){string k=args[1];cleanIfExpired(k);string t=checkType(k,"list");if(!t.empty())return t;if(!lists.count(k))lists[k]=deque<string>();for(size_t i=2;i<args.size();i++)lists[k].push_front(args[i]);keyType[k]="list";return ei((int)lists[k].size());}
    if (cmd=="RPUSH"){string k=args[1];cleanIfExpired(k);string t=checkType(k,"list");if(!t.empty())return t;if(!lists.count(k))lists[k]=deque<string>();for(size_t i=2;i<args.size();i++)lists[k].push_back(args[i]);keyType[k]="list";return ei((int)lists[k].size());}
    if (cmd=="LPOP"){string k=args[1];cleanIfExpired(k);if(!lists.count(k)||lists[k].empty())return ebnull();string v=lists[k].front();lists[k].pop_front();removeKeyIfEmpty(k);return eb(v);}
    if (cmd=="RPOP"){string k=args[1];cleanIfExpired(k);if(!lists.count(k)||lists[k].empty())return ebnull();string v=lists[k].back();lists[k].pop_back();removeKeyIfEmpty(k);return eb(v);}
    if (cmd=="LLEN"){string k=args[1];cleanIfExpired(k);if(!lists.count(k))return ei(0);return ei((int)lists[k].size());}
    if (cmd=="LRANGE"){
        string k=args[1];cleanIfExpired(k);if(!lists.count(k))return "*0\r\n";
        auto&d=lists[k];int len=(int)d.size();int start=stoi(args[2]),stop=stoi(args[3]);
        if(start<0)start+=len;if(stop<0)stop+=len;if(start<0)start=0;if(stop>=len)stop=len-1;
        if(start>stop)return "*0\r\n"; vector<string>res; for(int i=start;i<=stop;i++)res.push_back(d[i]); return ea(res);
    }
    if (cmd=="HSET"){
        string k=args[1];cleanIfExpired(k);string t=checkType(k,"hash");if(!t.empty())return t;
        if(!hashes.count(k))hashes[k]=map<string,string>(); int added=0;
        for(size_t i=2;i+1<args.size();i+=2){if(!hashes[k].count(args[i]))added++;hashes[k][args[i]]=args[i+1];}
        keyType[k]="hash"; return ei(added);
    }
    if (cmd=="HGET"){string k=args[1];cleanIfExpired(k);if(!hashes.count(k)||!hashes[k].count(args[2]))return ebnull();return eb(hashes[k][args[2]]);}
    if (cmd=="HDEL"){
        string k=args[1];cleanIfExpired(k);if(!hashes.count(k))return ei(0);
        int c=0; for(size_t i=2;i<args.size();i++){if(hashes[k].erase(args[i]))c++;}
        removeKeyIfEmpty(k); return ei(c);
    }
    if (cmd=="HGETALL"){
        string k=args[1];cleanIfExpired(k);if(!hashes.count(k))return "*0\r\n";
        vector<string>res; for(auto&p:hashes[k]){res.push_back(p.first);res.push_back(p.second);} return ea(res);
    }
    if (cmd=="HEXISTS"){string k=args[1];cleanIfExpired(k);if(!hashes.count(k)||!hashes[k].count(args[2]))return ei(0);return ei(1);}
    if (cmd=="HLEN"){string k=args[1];cleanIfExpired(k);if(!hashes.count(k))return ei(0);return ei((int)hashes[k].size());}

    // TODO: Implement SADD key member [member ...]
    //   - Check type: if key exists and is not "set" -> WRONGTYPE error
    //   - Count how many members are NEW (not already in the set)
    //   - Insert each member into sets[key]
    //   - Set keyType[key] = "set"
    if (cmd == "SADD") {
        if (args.size() < 3) return ee("ERR wrong number of arguments for 'SADD' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "set");
        if (!terr.empty()) return terr;

        int newCount = 0;
        for (size_t i = 2; i < args.size(); i++) {
            // insert().second returns true if the element was newly inserted
            if (sets[key].insert(args[i]).second) {
                newCount++;
            }
        }

        keyType[key] = "set";
        return ei(newCount);
    }

    // TODO: Implement SMEMBERS key
    //   - Return all members as RESP array (sorted alphabetically for consistent output)
    //   - If key doesn't exist -> "*0\r\n"
    if (cmd == "SMEMBERS") {
        if (args.size() != 2) return ee("ERR wrong number of arguments for 'SMEMBERS' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "set");
        if (!terr.empty()) return terr;

        if (!sets.count(key)) return "*0\r\n";

        vector<string> members(sets[key].begin(), sets[key].end());
        sort(members.begin(), members.end());
        return ea(members);
    }

    // TODO: Implement SISMEMBER key member -> ei(1) or ei(0)
    if (cmd == "SISMEMBER") {
        if (args.size() != 3) return ee("ERR wrong number of arguments for 'SISMEMBER' command");
        string key = args[1];
        string member = args[2];
        cleanIfExpired(key);

        string terr = checkType(key, "set");
        if (!terr.empty()) return terr;

        if (sets.count(key) && sets[key].count(member)) {
            return ei(1);
        }
        return ei(0);
    }

    // TODO: Implement SCARD key -> ei(set size) or ei(0)
    if (cmd == "SCARD") {
        if (args.size() != 2) return ee("ERR wrong number of arguments for 'SCARD' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "set");
        if (!terr.empty()) return terr;

        if (!sets.count(key)) return ei(0);

        return eil(sets[key].size());
    }

    // TODO: Implement SREM key member [member ...]
    //   - Remove members, count how many were actually removed
    //   - Call removeKeyIfEmpty(key) after
    //   - Return ei(count)
    if (cmd == "SREM") {
        if (args.size() < 3) return ee("ERR wrong number of arguments for 'SREM' command");
        string key = args[1];
        cleanIfExpired(key);

        string terr = checkType(key, "set");
        if (!terr.empty()) return terr;

        if (!sets.count(key)) return ei(0);

        int removedCount = 0;
        for (size_t i = 2; i < args.size(); i++) {
            if (sets[key].erase(args[i])) {
                removedCount++;
            }
        }

        removeKeyIfEmpty(key);
        return ei(removedCount);
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
