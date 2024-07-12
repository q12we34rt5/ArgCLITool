#pragma once

#include <cstdint>
#include <string>
#include <istream>
#include <sstream>
#include <optional>

namespace ArgCLITool {

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
    int64_t position;
};

class CLILexer {
public:
    CLILexer(std::istream& stream) : stream_(stream) {}

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
        int64_t position = stream_.tellg();

        while (stream_.get(c)) {
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
                    return CLIToken{CLIToken::Type::LeftParen, "(", position};
                case ')':
                    return CLIToken{CLIToken::Type::RightParen, ")", position};
                case '[':
                    return CLIToken{CLIToken::Type::LeftBracket, "[", position};
                case ']':
                    return CLIToken{CLIToken::Type::RightBracket, "]", position};
                case '{':
                    return CLIToken{CLIToken::Type::LeftCurly, "{", position};
                case '}':
                    return CLIToken{CLIToken::Type::RightCurly, "}", position};
                case ',':
                    return CLIToken{CLIToken::Type::Comma, ",", position};
                case '\n':
                    return CLIToken{CLIToken::Type::EndOfLine, "\n", position};
                case '#':
                    stream_.unget();
                    return readComment();
                case ' ': case '\t': case '\r':
                    // Ignore whitespace
                    continue;
                default:
                    // Unknown token
                    return CLIToken{CLIToken::Type::Unknown, std::string(1, c), position};
            }
        }

        return CLIToken{CLIToken::Type::EndOfFile, ""};
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
        int64_t position = stream_.tellg();

        while ((c = stream_.peek())) {
            if (isAlpha(c) || isDigit(c) || c == '_') {
                stream_.get(c);
                value += c;
            } else {
                break;
            }
        }

        return CLIToken{CLIToken::Type::Identifier, value, position};
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
        int64_t position = stream_.tellg();
        bool escape = false;

        while (stream_.get(c)) {
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

        // Return the token
        return CLIToken{CLIToken::Type::String, value, position};
    }

    /**
     * @brief Reads an integer or a float from the input stream.
     *
     * @return CLIToken
     */
    inline CLIToken readNumber() {
        std::string value;
        char c;
        int64_t position = stream_.tellg();

        while ((c = stream_.peek())) {
            if (isDigit(c) || isAlpha(c) || c == '.' || c == '-' || c == '+') {
                stream_.get(c);
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
                    return CLIToken{CLIToken::Type::Unknown, value, position};
                }
                return CLIToken{CLIToken::Type::Integer, std::to_string(integer), position};
            }
        }

        // Check float
        {
            float floating;
            std::istringstream iss(has_suffix ? value.substr(0, value.length() - 1) : value);
            iss >> floating;
            if (iss.eof() && !iss.fail()) {
                return CLIToken{CLIToken::Type::Float, std::to_string(floating), position};
            }
        }

        return CLIToken{CLIToken::Type::Unknown, value, position};
    }

    /**
     * @brief Reads a comment from the input stream.
     *
     * @return CLIToken
     */
    inline CLIToken readComment() {
        std::string value;
        char c;
        int64_t position = stream_.tellg();

        while ((c = stream_.peek())) {
            if (c == '\n') {
                break;
            }
            stream_.get(c);
            value += c;
        }

        return CLIToken{CLIToken::Type::Comment, value, position};
    }
private:
    std::istream& stream_;
    std::optional<CLIToken> peeked_token_;
};

}
