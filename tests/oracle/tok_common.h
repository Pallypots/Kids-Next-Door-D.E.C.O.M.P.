// Shared by oracle_tok (x86, original beside recovered) and test_at_tok (any host, recovered against the original's recorded digests).
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "at/AtIoTokenizer.h"

// A keyword table for a file: the game's own tables belong to code not yet recovered, so this takes every second distinct
// identifier in the file, which means both hits and misses. At most 300: an almost full table would make probing endless.
struct KeywordSet {
    std::vector<std::string> names;
    std::vector<AtIoKeyword> table;
    void build(const std::vector<unsigned char>& d) {
        std::vector<std::string> seen;
        size_t i = 0;
        const auto alpha = [](unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
        const auto rest = [&](unsigned char c) { return alpha(c) || (c >= '0' && c <= '9') || c == '_'; };
        while (i < d.size() && names.size() < 300) {
            if (!alpha(d[i])) { i++; continue; }
            size_t e = i;
            while (e < d.size() && rest(d[e])) e++;
            std::string w((const char*)&d[i], e - i);
            for (char& c : w) if (c >= 'a' && c <= 'z') c = (char)(c - 0x20);
            i = e;
            if (w.size() >= 0x3F) continue;
            bool dup = false;
            for (const std::string& s : seen) dup = dup || s == w;
            if (dup) continue;
            seen.push_back(w);
            if (seen.size() % 2) names.push_back(w);
        }
        table.reserve(names.size());
        for (size_t k = 0; k < names.size(); k++) table.push_back({(int)(100 + k), names[k].c_str()});
    }
};

struct Digest {
    uint64_t h = 0xCBF29CE484222325ull;
    uint64_t tokens = 0;
    void add(const void* p, size_t n) {
        const unsigned char* b = (const unsigned char*)p;
        for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001B3ull; }
    }
    void token(int ret, const AtIoTextTokenizer& t) {
        tokens++;
        add(&ret, 4);
        add(&t.m_type, 4);
        add(&t.m_int, 4);
        uint32_t real;
        std::memcpy(&real, &t.m_real, 4);
        add(&real, 4);
        add(t.m_string, std::strlen(t.m_string));
        add(t.m_identifier, std::strlen(t.m_identifier));
        add(&t.m_stream->m_line, 4);
    }
};
