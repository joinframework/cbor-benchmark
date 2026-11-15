#pragma once

#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <tinycbor/cbor.h>

#include "common/stat.hpp"

namespace tinycbor_detail
{

    inline const uint8_t* bytes (std::string_view cbor)
    {
        return reinterpret_cast<const uint8_t*> (cbor.data ());
    }

    inline bool copy (CborValue& it, CborEncoder& enc)
    {
        switch (cbor_value_get_type (&it))
        {
            case CborIntegerType:
            {
                uint64_t raw;
                if (cbor_value_get_raw_integer (&it, &raw) != CborNoError)
                    return false;
                CborError err = cbor_value_is_unsigned_integer (&it) ? cbor_encode_uint (&enc, raw)
                                                                     : cbor_encode_negative_int (&enc, raw + 1);
                return err == CborNoError && cbor_value_advance_fixed (&it) == CborNoError;
            }

            case CborByteStringType:
            {
                uint8_t* data = nullptr;
                size_t size = 0;
                if (cbor_value_dup_byte_string (&it, &data, &size, &it) != CborNoError)
                    return false;
                CborError err = cbor_encode_byte_string (&enc, data, size);
                std::free (data);
                return err == CborNoError;
            }

            case CborTextStringType:
            {
                char* data = nullptr;
                size_t size = 0;
                if (cbor_value_dup_text_string (&it, &data, &size, &it) != CborNoError)
                    return false;
                CborError err = cbor_encode_text_string (&enc, data, size);
                std::free (data);
                return err == CborNoError;
            }

            case CborArrayType:
            case CborMapType:
            {
                bool map = cbor_value_is_map (&it);
                size_t length = CborIndefiniteLength;
                if (cbor_value_is_length_known (&it))
                {
                    if ((map ? cbor_value_get_map_length (&it, &length) : cbor_value_get_array_length (&it, &length)) !=
                        CborNoError)
                        return false;
                }
                CborEncoder sub;
                if ((map ? cbor_encoder_create_map (&enc, &sub, length) : cbor_encoder_create_array (&enc, &sub, length)) !=
                    CborNoError)
                    return false;
                CborValue child;
                if (cbor_value_enter_container (&it, &child) != CborNoError)
                    return false;
                while (!cbor_value_at_end (&child))
                {
                    if (!copy (child, sub))
                        return false;
                }
                return cbor_value_leave_container (&it, &child) == CborNoError &&
                       cbor_encoder_close_container (&enc, &sub) == CborNoError;
            }

            case CborTagType:
            {
                CborTag tag;
                if (cbor_value_get_tag (&it, &tag) != CborNoError || cbor_encode_tag (&enc, tag) != CborNoError ||
                    cbor_value_advance_fixed (&it) != CborNoError)
                    return false;
                return copy (it, enc);
            }

            case CborSimpleType:
            {
                uint8_t value;
                return cbor_value_get_simple_type (&it, &value) == CborNoError &&
                       cbor_encode_simple_value (&enc, value) == CborNoError &&
                       cbor_value_advance_fixed (&it) == CborNoError;
            }

            case CborBooleanType:
            {
                bool value;
                return cbor_value_get_boolean (&it, &value) == CborNoError &&
                       cbor_encode_boolean (&enc, value) == CborNoError &&
                       cbor_value_advance_fixed (&it) == CborNoError;
            }

            case CborNullType:
                return cbor_encode_null (&enc) == CborNoError && cbor_value_advance_fixed (&it) == CborNoError;

            case CborUndefinedType:
                return cbor_encode_undefined (&enc) == CborNoError && cbor_value_advance_fixed (&it) == CborNoError;

            case CborHalfFloatType:
            {
                uint16_t value;
                return cbor_value_get_half_float (&it, &value) == CborNoError &&
                       cbor_encode_half_float (&enc, &value) == CborNoError &&
                       cbor_value_advance_fixed (&it) == CborNoError;
            }

            case CborFloatType:
            {
                float value;
                return cbor_value_get_float (&it, &value) == CborNoError &&
                       cbor_encode_float (&enc, value) == CborNoError && cbor_value_advance_fixed (&it) == CborNoError;
            }

            case CborDoubleType:
            {
                double value;
                return cbor_value_get_double (&it, &value) == CborNoError &&
                       cbor_encode_double (&enc, value) == CborNoError &&
                       cbor_value_advance_fixed (&it) == CborNoError;
            }

            default:
                return false;
        }
    }

    inline bool gen_stat (Stat& stat, CborValue& it)
    {
        switch (cbor_value_get_type (&it))
        {
            case CborArrayType:
            case CborMapType:
            {
                bool map = cbor_value_is_map (&it);
                if (map)
                    stat.object_count++;
                else
                    stat.array_count++;
                CborValue child;
                if (cbor_value_enter_container (&it, &child) != CborNoError)
                    return false;
                for (size_t i = 0; !cbor_value_at_end (&child); ++i)
                {
                    if (map && (i % 2) == 0)
                        ++stat.member_count;
                    else if (!map)
                        ++stat.element_count;
                    if (!gen_stat (stat, child))
                        return false;
                }
                return cbor_value_leave_container (&it, &child) == CborNoError;
            }

            case CborTextStringType:
            {
                size_t length;
                if (cbor_value_calculate_string_length (&it, &length) != CborNoError)
                    return false;
                stat.string_count++;
                stat.string_length += length;
                return cbor_value_advance (&it) == CborNoError;
            }

            case CborIntegerType:
            case CborHalfFloatType:
            case CborFloatType:
            case CborDoubleType:
                stat.number_count++;
                break;

            case CborBooleanType:
            {
                bool value;
                cbor_value_get_boolean (&it, &value);
                if (value)
                    stat.true_count++;
                else
                    stat.false_count++;
                break;
            }

            case CborNullType:
                stat.null_count++;
                break;

            case CborTagType:
                if (cbor_value_advance_fixed (&it) != CborNoError)
                    return false;
                return gen_stat (stat, it);

            default:
                break;
        }
        return cbor_value_advance (&it) == CborNoError;
    }

}

struct TinyCborAdapter
{
    static constexpr std::string_view name = "tinycbor";

    using Document = std::string;

    static std::optional<Document> parse (std::string_view cbor)
    {
        CborParser parser;
        CborValue it;
        if (cbor_parser_init (tinycbor_detail::bytes (cbor), cbor.size (), 0, &parser, &it) != CborNoError ||
            cbor_value_validate (&it, CborValidateBasic) != CborNoError)
            return std::nullopt;
        while (cbor_value_is_tag (&it))
        {
            if (cbor_value_advance_fixed (&it) != CborNoError)
                return std::nullopt;
        }
        if (cbor_value_advance (&it) != CborNoError ||
            cbor_value_get_next_byte (&it) != tinycbor_detail::bytes (cbor) + cbor.size ())
            return std::nullopt;
        return Document (cbor);
    }

    static std::string serialize (const Document& doc)
    {
        std::vector<uint8_t> buffer (doc.size () * 2 + 64);
        for (;;)
        {
            CborParser parser;
            CborValue it;
            CborEncoder enc;
            cbor_parser_init (tinycbor_detail::bytes (doc), doc.size (), 0, &parser, &it);
            cbor_encoder_init (&enc, buffer.data (), buffer.size (), 0);
            if (!tinycbor_detail::copy (it, enc))
            {
                size_t extra = cbor_encoder_get_extra_bytes_needed (&enc);
                if (extra == 0)
                    return {};
                buffer.resize (buffer.size () + extra);
                continue;
            }
            return std::string (reinterpret_cast<const char*> (buffer.data ()),
                                cbor_encoder_get_buffer_size (&enc, buffer.data ()));
        }
    }

    static Stat statistics (const Document& doc)
    {
        Stat stat;
        CborParser parser;
        CborValue it;
        cbor_parser_init (tinycbor_detail::bytes (doc), doc.size (), 0, &parser, &it);
        tinycbor_detail::gen_stat (stat, it);
        return stat;
    }

    static bool accepts (std::string_view cbor)
    {
        return parse (cbor).has_value ();
    }
};
