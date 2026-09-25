#pragma once

#include <cctype>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ncmm
{
struct manifest_contract_v1 {
    std::string id;
    std::string name;
    std::string version;
    std::string failure_policy;
    std::string ui_hotkey;
    uint32_t loader_api = 0;
    uint32_t api_major = 0;
    uint32_t api_min_minor = 0;
    uint32_t state_schema = 0;
    uint32_t state_min_supported = 0;
    bool api_contract_declared = false;
    bool state_contract_declared = false;
    std::vector<std::string> required_capabilities;
};

namespace manifest_detail
{
inline bool append_utf8( std::string &out, uint32_t cp )
{
    if( cp == 0 || cp > 0x10FFFF || ( cp >= 0xD800 && cp <= 0xDFFF ) ) {
        return false;
    }
    if( cp <= 0x7F ) {
        out.push_back( static_cast<char>( cp ) );
    } else if( cp <= 0x7FF ) {
        out.push_back( static_cast<char>( 0xC0 | ( cp >> 6 ) ) );
        out.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
    } else if( cp <= 0xFFFF ) {
        out.push_back( static_cast<char>( 0xE0 | ( cp >> 12 ) ) );
        out.push_back( static_cast<char>( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
        out.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
    } else {
        out.push_back( static_cast<char>( 0xF0 | ( cp >> 18 ) ) );
        out.push_back( static_cast<char>( 0x80 | ( ( cp >> 12 ) & 0x3F ) ) );
        out.push_back( static_cast<char>( 0x80 | ( ( cp >> 6 ) & 0x3F ) ) );
        out.push_back( static_cast<char>( 0x80 | ( cp & 0x3F ) ) );
    }
    return true;
}

inline int hex_digit( char c )
{
    if( c >= '0' && c <= '9' ) {
        return c - '0';
    }
    if( c >= 'a' && c <= 'f' ) {
        return 10 + c - 'a';
    }
    if( c >= 'A' && c <= 'F' ) {
        return 10 + c - 'A';
    }
    return -1;
}

inline bool valid_utf8_no_controls( const std::string &text )
{
    for( size_t i = 0; i < text.size(); ) {
        const unsigned char c = static_cast<unsigned char>( text[i] );
        if( c < 0x20 ) {
            return false;
        }
        if( c < 0x80 ) {
            ++i;
            continue;
        }
        uint32_t cp = 0;
        size_t need = 0;
        if( ( c & 0xE0 ) == 0xC0 ) {
            cp = c & 0x1F;
            need = 1;
            if( cp == 0 ) {
                return false;
            }
        } else if( ( c & 0xF0 ) == 0xE0 ) {
            cp = c & 0x0F;
            need = 2;
        } else if( ( c & 0xF8 ) == 0xF0 ) {
            cp = c & 0x07;
            need = 3;
        } else {
            return false;
        }
        if( i + need >= text.size() ) {
            return false;
        }
        for( size_t j = 1; j <= need; ++j ) {
            const unsigned char cc = static_cast<unsigned char>( text[i + j] );
            if( ( cc & 0xC0 ) != 0x80 ) {
                return false;
            }
            cp = ( cp << 6 ) | ( cc & 0x3F );
        }
        if( ( need == 1 && cp < 0x80 ) ||
            ( need == 2 && cp < 0x800 ) ||
            ( need == 3 && cp < 0x10000 ) ||
            cp > 0x10FFFF ||
            ( cp >= 0xD800 && cp <= 0xDFFF ) ) {
            return false;
        }
        i += need + 1;
    }
    return true;
}

class parser
{
    public:
        explicit parser( const std::string &text ) : text_( text ) {}

        bool parse( manifest_contract_v1 &out, std::string &reason )
        {
            skip_ws();
            if( !consume( '{' ) ) {
                return fail( reason, "manifest_json_invalid" );
            }

            std::set<std::string> seen;
            bool first = true;
            while( true ) {
                skip_ws();
                if( consume( '}' ) ) {
                    break;
                }
                if( !first ) {
                    if( !consume( ',' ) ) {
                        return fail( reason, "manifest_json_invalid" );
                    }
                    skip_ws();
                }
                first = false;

                std::string key;
                if( !parse_string( key ) ) {
                    return fail( reason, "manifest_json_invalid" );
                }
                if( !seen.insert( key ).second ) {
                    reason = "manifest_duplicate_key:" + key;
                    return false;
                }
                skip_ws();
                if( !consume( ':' ) ) {
                    return fail( reason, "manifest_json_invalid" );
                }
                skip_ws();

                if( key == "id" ) {
                    if( !parse_string( out.id ) ) {
                        return fail( reason, "manifest_type_error:id" );
                    }
                } else if( key == "name" ) {
                    if( !parse_string( out.name ) ) {
                        return fail( reason, "manifest_type_error:name" );
                    }
                } else if( key == "version" ) {
                    if( !parse_string( out.version ) ) {
                        return fail( reason, "manifest_type_error:version" );
                    }
                } else if( key == "failure_policy" ) {
                    if( !parse_string( out.failure_policy ) ) {
                        return fail( reason, "manifest_type_error:failure_policy" );
                    }
                } else if( key == "ui_hotkey" ) {
                    if( !parse_string( out.ui_hotkey ) ) {
                        return fail( reason, "manifest_type_error:ui_hotkey" );
                    }
                } else if( key == "loader_api" ) {
                    if( !parse_uint32( out.loader_api ) ) {
                        return fail( reason, "manifest_type_error:loader_api" );
                    }
                } else if( key == "api_major" ) {
                    if( !parse_uint32( out.api_major ) ) {
                        return fail( reason, "manifest_type_error:api_major" );
                    }
                } else if( key == "api_min_minor" ) {
                    if( !parse_uint32( out.api_min_minor ) ) {
                        return fail( reason, "manifest_type_error:api_min_minor" );
                    }
                } else if( key == "state_schema" ) {
                    if( !parse_uint32( out.state_schema ) ) {
                        return fail( reason, "manifest_type_error:state_schema" );
                    }
                } else if( key == "state_min_supported" ) {
                    if( !parse_uint32( out.state_min_supported ) ) {
                        return fail( reason, "manifest_type_error:state_min_supported" );
                    }
                } else if( key == "requires" ) {
                    if( !parse_string_array( out.required_capabilities ) ) {
                        return fail( reason, "manifest_type_error:requires" );
                    }
                } else {
                    reason = "manifest_unknown_field:" + key;
                    return false;
                }
            }

            skip_ws();
            if( pos_ != text_.size() ) {
                return fail( reason, "manifest_json_trailing" );
            }

            const bool has_api_major = seen.count( "api_major" ) != 0;
            const bool has_api_minor = seen.count( "api_min_minor" ) != 0;
            if( has_api_major != has_api_minor ) {
                reason = "manifest_api_contract_incomplete";
                return false;
            }
            out.api_contract_declared = has_api_major;

            const bool has_state_schema = seen.count( "state_schema" ) != 0;
            const bool has_state_min = seen.count( "state_min_supported" ) != 0;
            if( has_state_schema != has_state_min ) {
                reason = "manifest_state_contract_incomplete";
                return false;
            }
            out.state_contract_declared = has_state_schema;

            const char *required[] = {
                "id", "name", "version", "loader_api", "requires", "failure_policy"
            };
            for( const char *key : required ) {
                if( seen.count( key ) == 0 ) {
                    reason = std::string( "manifest_missing_field:" ) + key;
                    return false;
                }
            }

            reason.clear();
            return true;
        }

    private:
        const std::string &text_;
        size_t pos_ = 0;

        bool fail( std::string &reason, const char *value )
        {
            reason = value;
            return false;
        }

        void skip_ws()
        {
            while( pos_ < text_.size() &&
                   std::isspace( static_cast<unsigned char>( text_[pos_] ) ) ) {
                ++pos_;
            }
        }

        bool consume( char wanted )
        {
            if( pos_ < text_.size() && text_[pos_] == wanted ) {
                ++pos_;
                return true;
            }
            return false;
        }

        bool parse_hex4( uint32_t &value )
        {
            if( pos_ + 4 > text_.size() ) {
                return false;
            }
            value = 0;
            for( int i = 0; i < 4; ++i ) {
                const int digit = hex_digit( text_[pos_++] );
                if( digit < 0 ) {
                    return false;
                }
                value = ( value << 4 ) | static_cast<uint32_t>( digit );
            }
            return true;
        }

        bool parse_string( std::string &out )
        {
            if( !consume( '"' ) ) {
                return false;
            }
            out.clear();
            while( pos_ < text_.size() ) {
                const unsigned char c = static_cast<unsigned char>( text_[pos_++] );
                if( c == '"' ) {
                    return valid_utf8_no_controls( out );
                }
                if( c < 0x20 ) {
                    return false;
                }
                if( c != '\\' ) {
                    out.push_back( static_cast<char>( c ) );
                    continue;
                }
                if( pos_ >= text_.size() ) {
                    return false;
                }
                const char esc = text_[pos_++];
                switch( esc ) {
                    case '"':
                    case '\\':
                    case '/':
                        out.push_back( esc );
                        break;
                    case 'b':
                    case 'f':
                    case 'n':
                    case 'r':
                    case 't':
                        // Control characters are not valid in NCMM manifest identity/display strings.
                        return false;
                    case 'u': {
                        uint32_t cp = 0;
                        if( !parse_hex4( cp ) ) {
                            return false;
                        }
                        if( cp >= 0xD800 && cp <= 0xDBFF ) {
                            if( pos_ + 2 > text_.size() || text_[pos_] != '\\' || text_[pos_ + 1] != 'u' ) {
                                return false;
                            }
                            pos_ += 2;
                            uint32_t low = 0;
                            if( !parse_hex4( low ) || low < 0xDC00 || low > 0xDFFF ) {
                                return false;
                            }
                            cp = 0x10000 + ( ( cp - 0xD800 ) << 10 ) + ( low - 0xDC00 );
                        } else if( cp >= 0xDC00 && cp <= 0xDFFF ) {
                            return false;
                        }
                        if( cp < 0x20 || !append_utf8( out, cp ) ) {
                            return false;
                        }
                        break;
                    }
                    default:
                        return false;
                }
            }
            return false;
        }

        bool parse_uint32( uint32_t &out )
        {
            if( pos_ >= text_.size() || !std::isdigit( static_cast<unsigned char>( text_[pos_] ) ) ) {
                return false;
            }
            if( text_[pos_] == '0' && pos_ + 1 < text_.size() &&
                std::isdigit( static_cast<unsigned char>( text_[pos_ + 1] ) ) ) {
                return false;
            }
            uint64_t value = 0;
            while( pos_ < text_.size() &&
                   std::isdigit( static_cast<unsigned char>( text_[pos_] ) ) ) {
                value = value * 10u + static_cast<uint64_t>( text_[pos_] - '0' );
                if( value > std::numeric_limits<uint32_t>::max() ) {
                    return false;
                }
                ++pos_;
            }
            out = static_cast<uint32_t>( value );
            return true;
        }

        bool parse_string_array( std::vector<std::string> &out )
        {
            if( !consume( '[' ) ) {
                return false;
            }
            out.clear();
            skip_ws();
            if( consume( ']' ) ) {
                return true;
            }
            while( true ) {
                skip_ws();
                std::string item;
                if( !parse_string( item ) ) {
                    return false;
                }
                out.push_back( std::move( item ) );
                skip_ws();
                if( consume( ']' ) ) {
                    return true;
                }
                if( !consume( ',' ) ) {
                    return false;
                }
            }
        }
};

inline bool safe_token( const std::string &value )
{
    if( value.empty() || value.size() > 64 ) {
        return false;
    }
    for( unsigned char c : value ) {
        if( !( std::islower( c ) || std::isdigit( c ) || c == '_' || c == '-' || c == '.' ) ) {
            return false;
        }
    }
    return true;
}
} // namespace manifest_detail

inline bool parse_manifest_contract_v1( const std::string &text, manifest_contract_v1 &out,
        std::string &reason )
{
    if( text.empty() || text.size() > 64 * 1024 ) {
        reason = text.empty() ? "manifest_missing_or_unreadable" : "manifest_too_large";
        return false;
    }
    manifest_detail::parser p( text );
    manifest_contract_v1 parsed;
    if( !p.parse( parsed, reason ) ) {
        return false;
    }
    out = std::move( parsed );
    return true;
}

inline bool valid_module_id_v1( const std::string &id )
{
    return manifest_detail::safe_token( id );
}

inline bool valid_ui_hotkey_v1( const std::string &value )
{
    if( value.empty() ) {
        return true;
    }
    if( value.size() < 2 || value.size() > 3 || value[0] != 'F' ) {
        return false;
    }
    int number = 0;
    for( size_t i = 1; i < value.size(); ++i ) {
        const unsigned char c = static_cast<unsigned char>( value[i] );
        if( !std::isdigit( c ) ) {
            return false;
        }
        number = number * 10 + static_cast<int>( c - '0' );
    }
    return number >= 1 && number <= 12;
}

inline bool validate_manifest_contract_v1( const manifest_contract_v1 &manifest, std::string &reason )
{
    if( !valid_module_id_v1( manifest.id ) || manifest.name.empty() || manifest.name.size() > 128 ||
        manifest.version.empty() || manifest.version.size() > 64 || manifest.loader_api == 0 ) {
        reason = "invalid_manifest";
        return false;
    }
    if( manifest.failure_policy != "disable" ) {
        reason = "unsupported_failure_policy";
        return false;
    }
    if( manifest.required_capabilities.empty() || manifest.required_capabilities.size() > 32 ) {
        reason = "invalid_capability_list";
        return false;
    }

    std::set<std::string> unique;
    bool has_core = false;
    for( const std::string &capability : manifest.required_capabilities ) {
        if( !manifest_detail::safe_token( capability ) || !unique.insert( capability ).second ) {
            reason = "invalid_capability_list";
            return false;
        }
        if( capability == "core.v1" ) {
            has_core = true;
        }
    }
    if( manifest.api_contract_declared ) {
        if( manifest.api_major == 0 ) {
            reason = "invalid_api_contract";
            return false;
        }
        if( unique.count( "api.versioning.v1" ) == 0 ) {
            reason = "api_versioning_capability_required";
            return false;
        }
    }
    if( manifest.state_contract_declared ) {
        if( manifest.state_schema == 0 || manifest.state_min_supported > manifest.state_schema ) {
            reason = "invalid_state_contract";
            return false;
        }
        if( unique.count( "state.migration.v1" ) == 0 ) {
            reason = "state_migration_capability_required";
            return false;
        }
    }
    if( !has_core ) {
        reason = "core_capability_required";
        return false;
    }
    if( !valid_ui_hotkey_v1( manifest.ui_hotkey ) ) {
        reason = "invalid_ui_hotkey";
        return false;
    }
    if( !manifest.ui_hotkey.empty() && unique.count( "module_hotkeys.v1" ) == 0 ) {
        reason = "ui_hotkey_capability_required";
        return false;
    }
    reason.clear();
    return true;
}
} // namespace ncmm
