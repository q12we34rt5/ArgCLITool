#pragma once

#include <cstdint>
#include <string>
#include <istream>
#include <sstream>

namespace ArgCLITool {

struct CLIToken {
    enum class Type : int {
        Identifier,
        String,
        Integer,
        Float,
        LeftParen,
        RightParen,
        LeftSquare,
        RightSquare,
        LeftCurly,
        RightCurly,
        Comma,
        EndOfLine,
        Comment,
        EndOfFile,
        Unknown
    };
    static std::string toString(Type type) {
        switch (type) {
            case Type::Identifier:
                return "Identifier";
            case Type::String:
                return "String";
            case Type::Integer:
                return "Integer";
            case Type::Float:
                return "Float";
            case Type::LeftParen:
                return "LeftParen";
            case Type::RightParen:
                return "RightParen";
            case Type::LeftSquare:
                return "LeftSquare";
            case Type::RightSquare:
                return "RightSquare";
            case Type::LeftCurly:
                return "LeftCurly";
            case Type::RightCurly:
                return "RightCurly";
            case Type::Comma:
                return "Comma";
            case Type::EndOfLine:
                return "EndOfLine";
            case Type::Comment:
                return "Comment";
            case Type::EndOfFile:
                return "EndOfFile";
            case Type::Unknown:
                return "Unknown";
        }
        return "Unknown";
    }

    Type type;
    std::string value;
};

class CLILexer {
public:
    CLILexer(std::istream& stream) : stream_(stream) {}

    CLIToken nextToken() {
        char c;

        while (stream_.get(c)) {
            switch (c) {
                case 'A':
                case 'B':
                case 'C':
                case 'D':
                case 'E':
                case 'F':
                case 'G':
                case 'H':
                case 'I':
                case 'J':
                case 'K':
                case 'L':
                case 'M':
                case 'N':
                case 'O':
                case 'P':
                case 'Q':
                case 'R':
                case 'S':
                case 'T':
                case 'U':
                case 'V':
                case 'W':
                case 'X':
                case 'Y':
                case 'Z':
                case '_':
                case 'a':
                case 'b':
                case 'c':
                case 'd':
                case 'e':
                case 'f':
                case 'g':
                case 'h':
                case 'i':
                case 'j':
                case 'k':
                case 'l':
                case 'm':
                case 'n':
                case 'o':
                case 'p':
                case 'q':
                case 'r':
                case 's':
                case 't':
                case 'u':
                case 'v':
                case 'w':
                case 'x':
                case 'y':
                case 'z':
                    stream_.unget();
                    return readIdentifier();
                case '"':
                    return readString();
                case '-':
                case '+':
                case '.':
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    stream_.unget();
                    return readNumber();
                case '(':
                    return CLIToken{CLIToken::Type::LeftParen, "("};
                case ')':
                    return CLIToken{CLIToken::Type::RightParen, ")"};
                case '[':
                    return CLIToken{CLIToken::Type::LeftSquare, "["};
                case ']':
                    return CLIToken{CLIToken::Type::RightSquare, "]"};
                case '{':
                    return CLIToken{CLIToken::Type::LeftCurly, "{"};
                case '}':
                    return CLIToken{CLIToken::Type::RightCurly, "}"};
                case ',':
                    return CLIToken{CLIToken::Type::Comma, ","};
                case '\n':
                    return CLIToken{CLIToken::Type::EndOfLine, ""};
                case '#':
                    stream_.unget();
                    return readComment();
                case ' ':
                case '\t':
                case '\r':
                    // Ignore whitespace
                    continue;
                default:
                    // Unknown token
                    return CLIToken{CLIToken::Type::Unknown, std::string(1, c)};
            }
        }

        return CLIToken{CLIToken::Type::EndOfFile, ""};
    }

private:
    static inline constexpr bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    static inline constexpr bool isDigit(char c) { return c >= '0' && c <= '9'; }
    static inline constexpr bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

    /**
     * @brief Reads an identifier from the input stream.
     *
     * @return CLIToken
     */
    CLIToken readIdentifier() {
        std::string value;
        char c;

        while ((c = stream_.peek())) {
            if (isAlpha(c) || isDigit(c) || c == '_') {
                stream_.get(c);
                value += c;
            } else {
                break;
            }
        }

        return CLIToken{CLIToken::Type::Identifier, value};
    }

    /**
     * @brief Reads a string from the input stream.
     *
     * @return CLIToken
     *
     * @note The escape character is '\'. If it appears on the end of line, the new line (\n|\r\n) is ignored.
     */
    CLIToken readString() {
        std::string value;
        char c;
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
        return CLIToken{CLIToken::Type::String, value};
    }

    /**
     * @brief Reads an integer or a float from the input stream.
     *
     * @return CLIToken
     */
    CLIToken readNumber() {
        std::string value;
        char c;

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
                    return CLIToken{CLIToken::Type::Unknown, value};
                }
                return CLIToken{CLIToken::Type::Integer, std::to_string(integer)};
            }
        }

        // Check float
        {
            float floating;
            std::istringstream iss(has_suffix ? value.substr(0, value.length() - 1) : value);
            iss >> floating;
            if (iss.eof() && !iss.fail()) {
                return CLIToken{CLIToken::Type::Float, std::to_string(floating)};
            }
        }

        return CLIToken{CLIToken::Type::Unknown, value};
    }

    /**
     * @brief Reads a comment from the input stream.
     *
     * @return CLIToken
     */
    CLIToken readComment() {
        std::string value;
        char c;

        while ((c = stream_.peek())) {
            if (c == '\n') {
                break;
            }
            stream_.get(c);
            value += c;
        }

        return CLIToken{CLIToken::Type::Comment, value};
    }
private:
    std::istream& stream_;
};

}
