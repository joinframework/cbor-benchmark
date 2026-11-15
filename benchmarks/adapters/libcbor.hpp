#pragma once

#include <cstdlib>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include <cbor.h>

#include "common/stat.hpp"

namespace libcbor_detail
{
    STAT_WALK inline void gen_stat (Stat& stat, const cbor_item_t* item)
    {
        switch (cbor_typeof (item))
        {
            case CBOR_TYPE_MAP:
                {
                    stat.object_count++;
                    cbor_pair* pairs = cbor_map_handle (item);
                    for (size_t i = 0; i < cbor_map_size (item); ++i)
                    {
                        ++stat.member_count;
                        gen_stat (stat, pairs[i].key);
                        gen_stat (stat, pairs[i].value);
                    }
                    break;
                }

            case CBOR_TYPE_ARRAY:
                {
                    stat.array_count++;
                    cbor_item_t** items = cbor_array_handle (item);
                    for (size_t i = 0; i < cbor_array_size (item); ++i)
                    {
                        ++stat.element_count;
                        gen_stat (stat, items[i]);
                    }
                    break;
                }

            case CBOR_TYPE_STRING:
                stat.string_count++;
                if (cbor_string_is_definite (item))
                {
                    stat.string_length += cbor_string_length (item);
                }
                else
                {
                    cbor_item_t** chunks = cbor_string_chunks_handle (item);
                    for (size_t i = 0; i < cbor_string_chunk_count (item); ++i)
                    {
                        stat.string_length += cbor_string_length (chunks[i]);
                    }
                }
                break;

            case CBOR_TYPE_UINT:
            case CBOR_TYPE_NEGINT:
                stat.number_count++;
                break;

            case CBOR_TYPE_TAG:
                {
                    cbor_item_t* tagged = cbor_tag_item (item);
                    gen_stat (stat, tagged);
                    cbor_decref (&tagged);
                    break;
                }

            case CBOR_TYPE_FLOAT_CTRL:
                if (!cbor_float_ctrl_is_ctrl (item))
                {
                    stat.number_count++;
                }
                else if (cbor_is_bool (item))
                {
                    if (cbor_get_bool (item))
                    {
                        stat.true_count++;
                    }
                    else
                    {
                        stat.false_count++;
                    }
                }
                else if (cbor_is_null (item))
                {
                    stat.null_count++;
                }
                break;

            default:
                break;
        }
    }
}

struct LibCborDocument
{
    explicit LibCborDocument (cbor_item_t* root) noexcept
    : item (root)
    {
    }

    LibCborDocument (LibCborDocument&& other) noexcept
    : item (std::exchange (other.item, nullptr))
    {
    }

    LibCborDocument& operator= (LibCborDocument&& other) noexcept
    {
        std::swap (item, other.item);
        return *this;
    }

    ~LibCborDocument ()
    {
        if (item != nullptr)
        {
            cbor_decref (&item);
        }
    }

    cbor_item_t* item = nullptr;
};

struct LibCborOutput
{
    struct FreeDeleter
    {
        void operator() (unsigned char* data) const noexcept
        {
            std::free (data);
        }
    };

    std::unique_ptr<unsigned char, FreeDeleter> data;
    size_t size = 0;
};

struct LibCborAdapter
{
    static constexpr std::string_view name = "libcbor";

    using Document = LibCborDocument;

    static std::optional<Document> parse (std::string_view cbor)
    {
        cbor_load_result result;
        cbor_item_t* item = cbor_load (reinterpret_cast<cbor_data> (cbor.data ()), cbor.size (), &result);
        if (item == nullptr)
        {
            return std::nullopt;
        }
        Document doc (item);
        if (result.error.code != CBOR_ERR_NONE || result.read != cbor.size ())
        {
            return std::nullopt;
        }
        return doc;
    }

    static LibCborOutput serialize (const Document& doc)
    {
        LibCborOutput out;
        unsigned char* buffer = nullptr;
        size_t capacity = 0;
        out.size = cbor_serialize_alloc (doc.item, &buffer, &capacity);
        out.data.reset (buffer);
        return out;
    }

    static std::string_view view (const LibCborOutput& out)
    {
        return {reinterpret_cast<const char*> (out.data.get ()), out.size};
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        libcbor_detail::gen_stat (stat, doc.item);
        return stat;
    }

    static bool accepts (std::string_view cbor)
    {
        return parse (cbor).has_value ();
    }
};
