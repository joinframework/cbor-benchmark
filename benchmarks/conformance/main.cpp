#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include ADAPTER_HEADER
#include "common/utils.hpp"

namespace fs = std::filesystem;

enum Category
{
    Core,
    Indefinite,
    Tags,
    NonStringKeys,
    Malformed
};

static const char* const category_keys[] = {"core", "indefinite", "tags", "non_string_keys"};

struct Score
{
    int pass = 0;
    int fail = 0;
};

struct Result
{
    Score accept[Malformed];
    Score roundtrip[Malformed];
    Score reject;
};

struct Case
{
    Category category;
    std::string name;
    std::string bytes;
};

static std::string hex (std::string_view text)
{
    std::string bytes;
    int high = -1;
    for (char c : text)
    {
        int digit;
        if (c >= '0' && c <= '9')
            digit = c - '0';
        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;
        else
            continue;
        if (high < 0)
            high = digit;
        else
        {
            bytes.push_back (static_cast<char> (high * 16 + digit));
            high = -1;
        }
    }
    return bytes;
}

static std::vector<Case> valid_cases ()
{
    return {
        {Core, "unsigned 0", hex ("00")},
        {Core, "unsigned 23", hex ("17")},
        {Core, "uint8 24", hex ("18 18")},
        {Core, "uint8 max", hex ("18 ff")},
        {Core, "uint16 max", hex ("19 ff ff")},
        {Core, "uint32 max", hex ("1a ff ff ff ff")},
        {Core, "uint64 max", hex ("1b ff ff ff ff ff ff ff ff")},
        {Core, "uint8 non-minimal", hex ("18 01")},
        {Core, "uint64 non-minimal", hex ("1b 00 00 00 00 00 00 00 01")},
        {Core, "negative -1", hex ("20")},
        {Core, "negative -24", hex ("37")},
        {Core, "nint8 min", hex ("38 ff")},
        {Core, "nint16 min", hex ("39 ff ff")},
        {Core, "nint32 min", hex ("3a ff ff ff ff")},
        {Core, "nint64 int64 min", hex ("3b 7f ff ff ff ff ff ff ff")},
        {Core, "nint64 min", hex ("3b ff ff ff ff ff ff ff ff")},
        {Core, "float16 0.0", hex ("f9 00 00")},
        {Core, "float16 -0.0", hex ("f9 80 00")},
        {Core, "float16 1.0", hex ("f9 3c 00")},
        {Core, "float16 1.5", hex ("f9 3e 00")},
        {Core, "float16 max", hex ("f9 7b ff")},
        {Core, "float16 min subnormal", hex ("f9 00 01")},
        {Core, "float16 inf", hex ("f9 7c 00")},
        {Core, "float16 -inf", hex ("f9 fc 00")},
        {Core, "float16 nan", hex ("f9 7e 00")},
        {Core, "float32 0.1", hex ("fa 3d cc cc cd")},
        {Core, "float32 max", hex ("fa 7f 7f ff ff")},
        {Core, "float32 min subnormal", hex ("fa 00 00 00 01")},
        {Core, "float32 1.5 non-preferred", hex ("fa 3f c0 00 00")},
        {Core, "float64 1.1", hex ("fb 3f f1 99 99 99 99 99 9a")},
        {Core, "float64 -0.0 non-preferred", hex ("fb 80 00 00 00 00 00 00 00")},
        {Core, "float64 max", hex ("fb 7f ef ff ff ff ff ff ff")},
        {Core, "float64 min subnormal", hex ("fb 00 00 00 00 00 00 00 01")},
        {Core, "float64 inf non-preferred", hex ("fb 7f f0 00 00 00 00 00 00")},
        {Core, "float64 nan non-preferred", hex ("fb 7f f8 00 00 00 00 00 00")},
        {Core, "false", hex ("f4")},
        {Core, "true", hex ("f5")},
        {Core, "null", hex ("f6")},
        {Core, "undefined", hex ("f7")},
        {Core, "simple 0", hex ("e0")},
        {Core, "simple 19", hex ("f3")},
        {Core, "simple 32", hex ("f8 20")},
        {Core, "simple 255", hex ("f8 ff")},
        {Core, "text empty", hex ("60")},
        {Core, "text 23", hex ("77") + std::string (23, 'a')},
        {Core, "text8", hex ("78 18") + std::string (24, 'a')},
        {Core, "text16", hex ("79 01 00") + std::string (256, 'a')},
        {Core, "text32", hex ("7a 00 01 00 00") + std::string (65536, 'a')},
        {Core, "text8 non-minimal", hex ("78 01 61")},
        {Core, "text nul", hex ("63 61 00 62")},
        {Core, "text utf-8", hex ("64 f0 9d 84 9e")},
        {Core, "bytes empty", hex ("40")},
        {Core, "bytes", hex ("43 01 02 03")},
        {Core, "bytes16", hex ("59 01 00") + std::string (256, '\xfe')},
        {Core, "bytes32", hex ("5a 00 01 00 00") + std::string (65536, '\xfe')},
        {Core, "empty array", hex ("80")},
        {Core, "empty map", hex ("a0")},
        {Core, "nested empty arrays", hex ("81 81 80")},
        {Core, "array8", hex ("98 18") + std::string (24, '\x01')},
        {Core, "array16 non-minimal", hex ("99 00 01 f6")},
        {Core, "array32", hex ("9a 00 00 00 02 01 02")},
        {Core, "map8", hex ("b8 01 61 61 01")},
        {Core, "map32", hex ("ba 00 00 00 01 61 61 01")},
        {Core, "mixed array", hex ("86 f6 f5 01 20 61 61 fb 3f f8 00 00 00 00 00 00")},
        {Core, "nested map", hex ("a1 61 61 a1 61 62 82 01 a0")},
        {Core, "deep nesting", std::string (100, '\x81') + hex ("f6")},
        {Indefinite, "indefinite text", hex ("7f 61 61 62 62 63 ff")},
        {Indefinite, "indefinite text empty", hex ("7f ff")},
        {Indefinite, "indefinite text empty chunk", hex ("7f 60 61 61 ff")},
        {Indefinite, "indefinite bytes", hex ("5f 41 01 42 02 03 ff")},
        {Indefinite, "indefinite bytes empty", hex ("5f ff")},
        {Indefinite, "indefinite array", hex ("9f 01 02 ff")},
        {Indefinite, "indefinite array empty", hex ("9f ff")},
        {Indefinite, "indefinite map", hex ("bf 61 61 01 ff")},
        {Indefinite, "indefinite map empty", hex ("bf ff")},
        {Indefinite, "nested indefinite", hex ("9f 9f ff bf ff ff")},
        {Indefinite, "indefinite in definite", hex ("82 9f ff bf ff")},
        {Indefinite, "indefinite text in array", hex ("9f 7f 61 61 ff ff")},
        {Indefinite, "indefinite text key", hex ("bf 7f 61 61 ff 01 ff")},
        {Tags, "tag 0 date", hex ("c0 74") + "2013-03-21T20:04:00Z"},
        {Tags, "tag 1 epoch int", hex ("c1 1a 51 4b 67 b0")},
        {Tags, "tag 1 epoch float", hex ("c1 fb 41 d4 52 d9 ec 20 00 00")},
        {Tags, "tag 2 bignum", hex ("c2 49 01 00 00 00 00 00 00 00 00")},
        {Tags, "tag 3 negative bignum", hex ("c3 49 01 00 00 00 00 00 00 00 00")},
        {Tags, "tag 24 embedded", hex ("d8 18 45 64 49 45 54 46")},
        {Tags, "tag 32 uri", hex ("d8 20 76") + "http://www.example.com"},
        {Tags, "self-described", hex ("d9 d9 f7 f6")},
        {Tags, "unknown tag", hex ("d9 03 e8 01")},
        {Tags, "tag64", hex ("db 00 00 00 01 00 00 00 00 01")},
        {Tags, "nested tags", hex ("c6 c7 01")},
        {Tags, "tags in array", hex ("82 c1 01 c1 02")},
        {Tags, "tag on map key", hex ("a1 c0 61 61 01")},
        {Tags, "tag on map value", hex ("a1 61 61 c1 01")},
        {NonStringKeys, "map int key", hex ("a1 01 61 61")},
        {NonStringKeys, "map negative key", hex ("a1 20 61 61")},
        {NonStringKeys, "map uint64 key", hex ("a1 1b ff ff ff ff ff ff ff ff f6")},
        {NonStringKeys, "map null key", hex ("a1 f6 01")},
        {NonStringKeys, "map bool keys", hex ("a2 f4 00 f5 01")},
        {NonStringKeys, "map float key", hex ("a1 fb 3f f8 00 00 00 00 00 00 01")},
        {NonStringKeys, "map bytes key", hex ("a1 41 00 01")},
        {NonStringKeys, "map array key", hex ("a1 82 01 02 03")},
        {NonStringKeys, "map map key", hex ("a1 a1 61 61 01 02")},
    };
}

static std::vector<Case> invalid_cases ()
{
    return {
        {Malformed, "empty input", ""},
        {Malformed, "reserved info 28", hex ("1c")},
        {Malformed, "reserved info 29", hex ("3d")},
        {Malformed, "reserved info 30", hex ("5e")},
        {Malformed, "reserved info 28 major 7", hex ("fc")},
        {Malformed, "indefinite unsigned", hex ("1f")},
        {Malformed, "indefinite negative", hex ("3f")},
        {Malformed, "indefinite tag", hex ("df")},
        {Malformed, "break alone", hex ("ff")},
        {Malformed, "break in definite array", hex ("81 ff")},
        {Malformed, "break as map value", hex ("bf 61 61 ff")},
        {Malformed, "truncated uint8", hex ("18")},
        {Malformed, "truncated uint16", hex ("19 01")},
        {Malformed, "truncated uint32", hex ("1a 01 02 03")},
        {Malformed, "truncated uint64", hex ("1b 01 02 03 04 05 06 07")},
        {Malformed, "truncated float16", hex ("f9 00")},
        {Malformed, "truncated float32", hex ("fa 00 00 00")},
        {Malformed, "truncated float64", hex ("fb 00 00 00 00 00 00 00")},
        {Malformed, "truncated simple", hex ("f8")},
        {Malformed, "two-byte simple below 32", hex ("f8 18")},
        {Malformed, "truncated text", hex ("63 61 62")},
        {Malformed, "truncated text8 header", hex ("78")},
        {Malformed, "truncated bytes", hex ("43 01")},
        {Malformed, "huge text32", hex ("7a ff ff ff ff 61")},
        {Malformed, "huge bytes64", hex ("5b ff ff ff ff ff ff ff ff 01")},
        {Malformed, "huge array32", hex ("9a ff ff ff ff 01")},
        {Malformed, "huge map32", hex ("ba ff ff ff ff 61 61 01")},
        {Malformed, "truncated array", hex ("82 01")},
        {Malformed, "truncated map value", hex ("a1 61 61")},
        {Malformed, "truncated tag", hex ("c1")},
        {Malformed, "unterminated indefinite array", hex ("9f 01")},
        {Malformed, "unterminated indefinite map", hex ("bf 61 61 01")},
        {Malformed, "unterminated indefinite text", hex ("7f 61 61")},
        {Malformed, "wrong chunk type in text", hex ("7f 41 61 ff")},
        {Malformed, "indefinite chunk in text", hex ("7f 7f ff ff")},
        {Malformed, "wrong chunk type in bytes", hex ("5f 61 61 ff")},
        {Malformed, "integer chunk in bytes", hex ("5f 01 ff")},
        {Malformed, "trailing byte", hex ("f5 00")},
    };
}

static bool doubles_bitwise_eq (double a, double b)
{
    uint64_t ua, ub;
    std::memcpy (&ua, &a, 8);
    std::memcpy (&ub, &b, 8);
    return ua == ub;
}

static double half_to_double (uint16_t half)
{
    int exponent = (half >> 10) & 0x1f;
    int mantissa = half & 0x3ff;
    double value;
    if (exponent == 0)
        value = std::ldexp (mantissa, -24);
    else if (exponent != 31)
        value = std::ldexp (mantissa + 1024, exponent - 25);
    else
        value = mantissa == 0 ? INFINITY : NAN;
    return (half & 0x8000) ? -value : value;
}

struct Node
{
    enum Kind
    {
        Uint,
        Nint,
        Bytes,
        Text,
        Array,
        Map,
        Tag,
        Simple,
        Float
    };

    Kind kind = Simple;
    uint64_t number = 0;
    double real = 0;
    std::string bytes;
    std::vector<Node> items;
    std::vector<std::pair<Node, Node>> members;
};

class Decoder
{
public:
    explicit Decoder (std::string_view in)
    : _in (in)
    {
    }

    std::optional<Node> decode ()
    {
        Node node;
        if (!value (node, 0) || _pos != _in.size ())
            return std::nullopt;
        return node;
    }

private:
    bool take (uint64_t size, std::string_view& out)
    {
        if (size > _in.size () - _pos)
            return false;
        out = _in.substr (_pos, size);
        _pos += size;
        return true;
    }

    bool number (size_t width, uint64_t& out)
    {
        std::string_view raw;
        if (!take (width, raw))
            return false;
        out = 0;
        for (char c : raw)
            out = (out << 8) | static_cast<uint8_t> (c);
        return true;
    }

    bool argument (uint8_t info, uint64_t& out)
    {
        if (info < 24)
        {
            out = info;
            return true;
        }
        if (info > 27)
            return false;
        return number (size_t (1) << (info - 24), out);
    }

    bool at_break ()
    {
        if (_pos < _in.size () && static_cast<uint8_t> (_in[_pos]) == 0xff)
        {
            ++_pos;
            return true;
        }
        return false;
    }

    bool chunks (Node& node, uint8_t major)
    {
        while (!at_break ())
        {
            uint64_t head, size;
            std::string_view raw;
            if (!number (1, head) || (head >> 5) != major || !argument (head & 0x1f, size) || !take (size, raw))
                return false;
            node.bytes += raw;
        }
        return true;
    }

    bool value (Node& node, int depth)
    {
        uint64_t head, arg;
        if (depth > 1000 || !number (1, head))
            return false;
        uint8_t major = static_cast<uint8_t> (head >> 5);
        uint8_t info = static_cast<uint8_t> (head & 0x1f);
        if (info == 31)
        {
            switch (major)
            {
                case 2:
                case 3:
                    node.kind = major == 2 ? Node::Bytes : Node::Text;
                    return chunks (node, major);
                case 4:
                    node.kind = Node::Array;
                    while (!at_break ())
                    {
                        node.items.emplace_back ();
                        if (!value (node.items.back (), depth + 1))
                            return false;
                    }
                    return true;
                case 5:
                    node.kind = Node::Map;
                    while (!at_break ())
                    {
                        node.members.emplace_back ();
                        if (!value (node.members.back ().first, depth + 1) ||
                            !value (node.members.back ().second, depth + 1))
                            return false;
                    }
                    return true;
                default:
                    return false;
            }
        }
        if (major == 7)
        {
            if (info < 24)
            {
                node.kind = Node::Simple;
                node.number = info;
                return true;
            }
            if (info == 24)
            {
                node.kind = Node::Simple;
                return number (1, node.number) && node.number >= 32;
            }
            uint64_t raw;
            if (!argument (info, raw))
                return false;
            node.kind = Node::Float;
            if (info == 25)
                node.real = half_to_double (static_cast<uint16_t> (raw));
            else if (info == 26)
            {
                uint32_t bits = static_cast<uint32_t> (raw);
                float f;
                std::memcpy (&f, &bits, 4);
                node.real = f;
            }
            else
                std::memcpy (&node.real, &raw, 8);
            return true;
        }
        if (!argument (info, arg))
            return false;
        switch (major)
        {
            case 0:
            case 1:
                node.kind = major == 0 ? Node::Uint : Node::Nint;
                node.number = arg;
                return true;
            case 2:
            case 3:
            {
                std::string_view raw;
                node.kind = major == 2 ? Node::Bytes : Node::Text;
                if (!take (arg, raw))
                    return false;
                node.bytes = raw;
                return true;
            }
            case 4:
                if (arg > _in.size () - _pos)
                    return false;
                node.kind = Node::Array;
                node.items.resize (arg);
                for (auto& item : node.items)
                {
                    if (!value (item, depth + 1))
                        return false;
                }
                return true;
            case 5:
                if (arg > (_in.size () - _pos) / 2)
                    return false;
                node.kind = Node::Map;
                node.members.resize (arg);
                for (auto& [key, val] : node.members)
                {
                    if (!value (key, depth + 1) || !value (val, depth + 1))
                        return false;
                }
                return true;
            default:
                node.kind = Node::Tag;
                node.number = arg;
                node.items.resize (1);
                return value (node.items[0], depth + 1);
        }
    }

    std::string_view _in;
    size_t _pos = 0;
};

static bool same_value (const Node& a, const Node& b)
{
    if (a.kind != b.kind)
        return false;
    switch (a.kind)
    {
        case Node::Uint:
        case Node::Nint:
        case Node::Simple:
            return a.number == b.number;
        case Node::Float:
            return doubles_bitwise_eq (a.real, b.real);
        case Node::Bytes:
        case Node::Text:
            return a.bytes == b.bytes;
        case Node::Tag:
            return a.number == b.number && same_value (a.items[0], b.items[0]);
        case Node::Array:
            if (a.items.size () != b.items.size ())
                return false;
            for (size_t i = 0; i < a.items.size (); ++i)
            {
                if (!same_value (a.items[i], b.items[i]))
                    return false;
            }
            return true;
        case Node::Map:
            if (a.members.size () != b.members.size ())
                return false;
            for (auto& [key, val] : a.members)
            {
                bool found = false;
                for (size_t j = 0; j < b.members.size () && !found; ++j)
                    found = same_value (key, b.members[j].first) && same_value (val, b.members[j].second);
                if (!found)
                    return false;
            }
            return true;
    }
    return false;
}

static void node_stat (Stat& stat, const Node& node)
{
    switch (node.kind)
    {
        case Node::Map:
            stat.object_count++;
            for (auto& [key, val] : node.members)
            {
                ++stat.member_count;
                node_stat (stat, key);
                node_stat (stat, val);
            }
            break;
        case Node::Array:
            stat.array_count++;
            for (auto& item : node.items)
            {
                ++stat.element_count;
                node_stat (stat, item);
            }
            break;
        case Node::Text:
            stat.string_count++;
            stat.string_length += node.bytes.size ();
            break;
        case Node::Uint:
        case Node::Nint:
        case Node::Float:
            stat.number_count++;
            break;
        case Node::Simple:
            if (node.number == 20)
                stat.false_count++;
            else if (node.number == 21)
                stat.true_count++;
            else if (node.number == 22)
                stat.null_count++;
            break;
        case Node::Tag:
            node_stat (stat, node.items[0]);
            break;
        default:
            break;
    }
}

static bool same_document (std::string_view expected, std::string_view actual)
{
    auto a = Decoder (expected).decode ();
    auto b = Decoder (actual).decode ();
    return a && b && same_value (*a, *b);
}

template <typename Adapter, typename Output>
std::string_view output_view (const Output& output)
{
    if constexpr (std::is_convertible_v<const Output&, std::string_view>)
        return output;
    else
        return Adapter::view (output);
}

static void count (Score& score, bool ok, bool verbose, const char* what, const std::string& name)
{
    if (ok)
        score.pass++;
    else
    {
        score.fail++;
        if (verbose)
            std::cerr << "  [FAIL] " << what << ": " << name << "\n";
    }
}

template <typename Adapter>
void run_valid (Result& r, const std::vector<Case>& cases, bool verbose)
{
    for (auto& c : cases)
    {
        count (r.accept[c.category], Adapter::accepts (c.bytes), verbose, "should accept", c.name);
        auto doc = Adapter::parse (c.bytes);
        bool same = doc && same_document (c.bytes, output_view<Adapter> (Adapter::serialize (*doc)));
        count (r.roundtrip[c.category], same, verbose, "roundtrip", c.name);
    }
}

template <typename Adapter>
void run_invalid (Result& r, const std::vector<Case>& cases, bool verbose)
{
    for (auto& c : cases)
        count (r.reject, !Adapter::accepts (c.bytes), verbose, "should reject", c.name);
}

template <typename Adapter>
bool run_output_check (const std::vector<std::string>& files)
{
    bool ok = true;
    auto fail = [&] (const std::string& path, const char* what) {
        ok = false;
        std::cerr << "  [FAIL] " << Adapter::name << " " << what << ": " << fs::path (path).filename ().string ()
                  << "\n";
    };
    for (size_t i = 0; i < files.size (); ++i)
    {
        auto cbor = load_file (files[i]);
        auto doc = Adapter::parse (cbor);
        if (!doc)
        {
            fail (files[i], "parse");
            continue;
        }
        auto stat = Adapter::statistics (*doc);
        Stat expected;
        if (auto node = Decoder (cbor).decode ())
            node_stat (expected, *node);
        if (!(stat == expected))
            fail (files[i], "statistics");
        auto result = Adapter::serialize (*doc);
        auto output = output_view<Adapter> (result);
        auto reparsed = Adapter::parse (output);
        if (!reparsed || !(Adapter::statistics (*reparsed) == stat) || !same_document (cbor, output))
            fail (files[i], "serialize");
    }
    return ok;
}

static void print_header ()
{
    std::cout << std::left << std::setw (25) << "Library" << std::right;
    for (const char* label : {"Core", "Core RT", "Indef", "Indef RT", "Tags", "Tags RT", "Keys", "Keys RT", "Reject"})
        std::cout << std::setw (12) << label;
    std::cout << "\n" << std::string (133, '-') << "\n";
}

static std::string fmt (const Score& score)
{
    return std::to_string (score.pass) + "/" + std::to_string (score.pass + score.fail);
}

static void print_row (const std::string& name, const Result& r)
{
    std::cout << std::left << std::setw (25) << name << std::right;
    for (int c = Core; c < Malformed; ++c)
        std::cout << std::setw (12) << fmt (r.accept[c]) << std::setw (12) << fmt (r.roundtrip[c]);
    std::cout << std::setw (12) << fmt (r.reject) << "\n";
}

static void write_json (const std::string& output_path, const std::vector<std::pair<std::string, Result>>& all)
{
    std::ofstream out (output_path);
    out << "{\n  \"results\": [\n";
    for (size_t i = 0; i < all.size (); ++i)
    {
        auto& [name, r] = all[i];
        auto kv = [&] (const std::string& key, const Score& score) {
            out << ",\n      \"" << key << "\": {\"pass\": " << score.pass
                << ", \"total\": " << (score.pass + score.fail) << "}";
        };
        out << "    {\n"
            << "      \"library\": \"" << name << "\"";
        for (int c = Core; c < Malformed; ++c)
        {
            kv (std::string (category_keys[c]) + "_accept", r.accept[c]);
            kv (std::string (category_keys[c]) + "_roundtrip", r.roundtrip[c]);
        }
        kv ("reject", r.reject);
        out << "\n    }";
        if (i + 1 < all.size ())
            out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}

int main (int argc, char* argv[])
{
    bool verbose = false;
    bool header = false;
    std::string output_path;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg (argv[i]);
        if (arg == "-v")
            verbose = true;
        else if (arg == "--header")
            header = true;
        else if (arg == "--output" && i + 1 < argc)
            output_path = argv[++i];
    }

    const fs::path data_dir = DATA_DIR;
    auto valid = valid_cases ();
    auto invalid = invalid_cases ();

    std::vector<std::string> check_files;
    for (const char* name : {"canada.cbor", "citm_catalog.cbor", "twitter.cbor"})
        check_files.push_back ((data_dir / name).string ());
    bool outputs_ok = true;

    if (header)
    {
        std::cout << "\n=== CBOR Conformance ===\n\n";
        print_header ();
    }

    std::vector<std::pair<std::string, Result>> all_results;

    auto run = [&] (auto adapter) {
        using Adapter = decltype (adapter);
        std::string name{Adapter::name};
        if (verbose)
            std::cout << "\n[" << name << "]\n";

        Result r;
        run_valid<Adapter> (r, valid, verbose);
        run_invalid<Adapter> (r, invalid, verbose);
        outputs_ok &= run_output_check<Adapter> (check_files);
        print_row (name, r);
        all_results.emplace_back (name, r);
    };

#define X(Adapter) run (Adapter{});
    CONFORMANCE_ADAPTERS
#undef X

    if (!output_path.empty ())
        write_json (output_path, all_results);

    return outputs_ok ? 0 : 1;
}
