#pragma once

#include <cstdint>
#include <string>
#include <istream>
#include <sstream>
#include <optional>

namespace ArgCLITool {

// Abstract input stream
class CLIInputStream {
public:
    virtual ~CLIInputStream() = default;

    virtual char peek() = 0;
    virtual bool get(char& c) = 0;
    virtual void unget() = 0;
    virtual int64_t tellg() = 0;
};

// Input stream for std::istream
class CLIStdInputStream : public CLIInputStream {
public:
    CLIStdInputStream(std::istream& stream) : stream_(stream) {}

    char peek() override {
        return stream_.peek();
    }

    bool get(char& c) override {
        return static_cast<bool>(stream_.get(c));
    }

    void unget() override {
        stream_.unget();
    }

    int64_t tellg() override {
        return stream_.tellg();
    }

private:
    std::istream& stream_;
};

struct CLIToken {
    enum class Type {
        Identifier,
        String,
        Integer,
        Float,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket,
        LeftCurly,
        RightCurly,
        Comma,
        EndOfLine,
        Comment,
        EndOfFile,
        Unknown
    };
    static inline std::string toString(Type type) {
        switch (type) {
            case Type::Identifier:   return "identifier";
            case Type::String:       return "string";
            case Type::Integer:      return "integer";
            case Type::Float:        return "float";
            case Type::LeftParen:    return "left paren";
            case Type::RightParen:   return "right paren";
            case Type::LeftBracket:  return "left bracket";
            case Type::RightBracket: return "right bracket";
            case Type::LeftCurly:    return "left curly";
            case Type::RightCurly:   return "right curly";
            case Type::Comma:        return "comma";
            case Type::EndOfLine:    return "end of line";
            case Type::Comment:      return "comment";
            case Type::EndOfFile:    return "end of file";
            case Type::Unknown:      return "unknown";
        }
        return "Unknown";
    }

    Type type;
    std::string value;
    int64_t begin;
    int64_t end;
};

class CLILexer {
public:
    CLILexer(CLIInputStream& stream) : stream_(stream) {}

    bool hasMoreTokens() {
        return stream_.peek() != std::char_traits<char>::eof();
    }

    CLIToken nextToken() {
        if (peeked_token_) {
            CLIToken token = std::move(*peeked_token_);
            peeked_token_.reset();
            return token;
        }
        return readNextToken();
    }

    const CLIToken& peekToken() {
        if (!peeked_token_) {
            peeked_token_ = readNextToken();
        }
        return *peeked_token_;
    }

private:
    CLIToken readNextToken() {
        char c;

        while (stream_.get(c)) {
            int64_t begin = stream_.tellg() - 1;
            switch (c) {
                case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
                case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
                case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
                case '_':
                case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
                case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
                case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
                    stream_.unget();
                    return readIdentifier();
                case '"':
                    return readString();
                case '-': case '+': case '.':
                case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                    stream_.unget();
                    return readNumber();
                case '(':
                    return CLIToken{CLIToken::Type::LeftParen, "(", begin, begin + 1};
                case ')':
                    return CLIToken{CLIToken::Type::RightParen, ")", begin, begin + 1};
                case '[':
                    return CLIToken{CLIToken::Type::LeftBracket, "[", begin, begin + 1};
                case ']':
                    return CLIToken{CLIToken::Type::RightBracket, "]", begin, begin + 1};
                case '{':
                    return CLIToken{CLIToken::Type::LeftCurly, "{", begin, begin + 1};
                case '}':
                    return CLIToken{CLIToken::Type::RightCurly, "}", begin, begin + 1};
                case ',':
                    return CLIToken{CLIToken::Type::Comma, ",", begin, begin + 1};
                case '\n':
                    return CLIToken{CLIToken::Type::EndOfLine, "\n", begin, begin + 1};
                case '#':
                    stream_.unget();
                    return readComment();
                case ' ': case '\t': case '\r':
                    // Ignore whitespace
                    continue;
                default:
                    // Unknown token
                    return CLIToken{CLIToken::Type::Unknown, std::string(1, c), begin, begin + 1};
            }
        }

        int64_t position = stream_.tellg();
        return CLIToken{CLIToken::Type::EndOfFile, "", position, position};
    }

    static inline constexpr bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    static inline constexpr bool isDigit(char c) { return c >= '0' && c <= '9'; }
    static inline constexpr bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

    /**
     * @brief Reads an identifier from the input stream.
     *
     * @return CLIToken
     */
    inline CLIToken readIdentifier() {
        std::string value;
        char c;
        int64_t begin = stream_.tellg();
        int64_t end = begin;

        while (true) {
            c = stream_.peek();
            if (isAlpha(c) || isDigit(c) || c == '_') {
                stream_.get(c);
                ++end;
                value += c;
            } else {
                break;
            }
        }

        return CLIToken{CLIToken::Type::Identifier, value, begin, end};
    }

    /**
     * @brief Reads a string from the input stream.
     *
     * @return CLIToken
     *
     * @note The escape character is '\'. If it appears on the end of line, the new line (\n|\r\n) is ignored.
     */
    inline CLIToken readString() {
        std::string value;
        char c;
        int64_t begin = stream_.tellg();
        int64_t end = begin;
        bool escape = false;

        while (stream_.get(c)) {
            ++end;
            if (escape) {
                // Handle escaped characters
                if (c == '\r') {
                    // Ignore carriage return
                    continue;
                } else if (c == '\n') {
                    // Ignore new line
                    escape = false;
                    continue;
                }
                value += c;
                escape = false;
            } else if (c == '\\') {
                // Set the escape flag
                escape = true;
            } else if (c == '"') {
                // End of string
                break;
            } else {
                value += c;
            }
        }

        return CLIToken{CLIToken::Type::String, value, begin - 1, end}; // Include the opening quote
    }

    /**
     * @brief Reads an integer or a float from the input stream.
     *
     * @return CLIToken
     */
    inline CLIToken readNumber() {
        std::string value;
        char c;
        int64_t begin = stream_.tellg();
        int64_t end = begin;

        while ((c = stream_.peek())) {
            if (isDigit(c) || isAlpha(c) || c == '_' || c == '.' || c == '-' || c == '+') {
                stream_.get(c);
                ++end;
                value += c;
            } else {
                break;
            }
        }

        // Check f|F suffix and remove it
        bool has_suffix = value.length() > 0 && (value.back() == 'f' || value.back() == 'F');

        // Check integer
        {
            int64_t integer;
            std::istringstream iss(has_suffix ? value.substr(0, value.length() - 1) : value);
            iss >> integer;
            if (iss.eof() && !iss.fail()) {
                if (has_suffix) {
                    return CLIToken{CLIToken::Type::Unknown, value, begin, end};
                }
                return CLIToken{CLIToken::Type::Integer, std::to_string(integer), begin, end};
            }
        }

        // Check float
        {
            float floating;
            std::istringstream iss(has_suffix ? value.substr(0, value.length() - 1) : value);
            iss >> floating;
            if (iss.eof() && !iss.fail()) {
                return CLIToken{CLIToken::Type::Float, std::to_string(floating), begin, end};
            }
        }

        return CLIToken{CLIToken::Type::Unknown, value, begin, end};
    }

    /**
     * @brief Reads a comment from the input stream.
     *
     * @return CLIToken
     */
    inline CLIToken readComment() {
        std::string value;
        char c;
        int64_t begin = stream_.tellg();
        int64_t end = begin;

        while (true) {
            c = stream_.peek();
            if (c == '\n') {
                break;
            }
            stream_.get(c);
            ++end;
            value += c;
        }

        return CLIToken{CLIToken::Type::Comment, value, begin, end};
    }
private:
    CLIInputStream& stream_;
    std::optional<CLIToken> peeked_token_;
};

}
