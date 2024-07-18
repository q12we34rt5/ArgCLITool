#pragma once

#include "CLILexer.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <variant>
#include <stdexcept>
#include <cassert>

namespace ArgCLITool {

// Hook the input stream and record the consumed characters
class CLIInputStreamHook : public CLIInputStream {
public:
    CLIInputStreamHook(CLIInputStream& stream)
        : stream_(stream), stream_position_(0), position_(0), line_number_(1), current_line_number_(1) {}

    char peek() override {
        return stream_.peek();
    }

    bool get(char& c) override {
        if (stream_.get(c)) {
            ++stream_position_;
            consumed_chars_.push_back(c);
            if (c == '\n') {
                ++current_line_number_;
            }
            return true;
        }
        return false;
    }

    void unget() override {
        if (consumed_chars_.empty()) {
            throw std::runtime_error("Cannot unget " + std::string(__FILE__) + ":" + std::to_string(__LINE__));
        }
        stream_.unget();
        --stream_position_;
        if (consumed_chars_.back() == '\n') {
            --current_line_number_;
        }
        consumed_chars_.pop_back();
    }

    int64_t tellg() override {
        return stream_position_;
    }

    void clearConsumedTokens() {
        position_ = stream_position_;
        line_number_ = current_line_number_;
        consumed_chars_.clear();
    }

    std::string getConsumedTokens() const {
        return std::string(consumed_chars_.begin(), consumed_chars_.end());
    }

    int64_t getPosition() const {
        return position_;
    }

    int64_t getLineNumber() const {
        return line_number_;
    }

private:
    CLIInputStream& stream_;
    int64_t stream_position_; // Input stream may not support tellg() (for example, std::cin)
    std::vector<char> consumed_chars_;
    int64_t position_;
    int64_t line_number_; // Beginning line number of the consumed tokens
    int64_t current_line_number_; // Current line number
};

// Reports more human-readable error messages for the parser
class ErrorReporter {
public:
    constexpr static const char* RED     = "\033[31m";
    constexpr static const char* GREEN   = "\033[32m";
    constexpr static const char* YELLOW  = "\033[33m";
    constexpr static const char* BLUE    = "\033[34m";
    constexpr static const char* MAGENTA = "\033[35m";
    constexpr static const char* CYAN    = "\033[36m";
    constexpr static const char* RESET   = "\033[0m";

public:
    ErrorReporter(const CLIInputStreamHook& stream_hook, bool color_output = true, bool show_source = true)
        : stream_hook_(stream_hook), color_output_(color_output), show_source_(show_source) {}

    /**
     * @brief Unexpected token error (with expected token)
     */
    inline std::runtime_error unexpectedTokenError(const CLIToken::Type& expected, const CLIToken& actual) {
        std::string report =
            colorString("Error: ", RED) +
            "expected " + CLIToken::toString(expected) +
            " at position " + std::to_string(actual.begin) +
            " but got " + CLIToken::toString(actual.type) +
            (actual.type == CLIToken::Type::EndOfLine ? "" : " '" + actual.value + "'");
        if (show_source_) {
            report += "\n" + getSourceSnippetReport(actual.begin, actual.end);
        }
        return std::runtime_error(std::move(report));
    }

    /**
     * @brief Unexpected token error (custom message)
     */
    inline std::runtime_error unexpectedTokenError(const std::string& expected, const CLIToken& actual) {
        std::string report =
            colorString("Error: ", RED) +
            "expected " + expected +
            " at position " + std::to_string(actual.begin) +
            " but got " + CLIToken::toString(actual.type) +
            (actual.type == CLIToken::Type::EndOfLine ? "" : " '" + actual.value + "'");
        if (show_source_) {
            report += "\n" + getSourceSnippetReport(actual.begin, actual.end);
        }
        return std::runtime_error(std::move(report));
    }

    /**
     * @brief Unexpected token error (without expected token)
     */
    inline std::runtime_error unexpectedTokenError(const CLIToken& unexpected) {
        std::string report =
            colorString("Error: ", RED) +
            "unexpected " + CLIToken::toString(unexpected.type) +
            " at position " + std::to_string(unexpected.begin) +
            (unexpected.type == CLIToken::Type::EndOfLine ? "" : " '" + unexpected.value + "'");
        if (show_source_) {
            report += "\n" + getSourceSnippetReport(unexpected.begin, unexpected.end);
        }
        return std::runtime_error(std::move(report));
    }

    /**
     * @brief Mismatched bracket error ('()' or '[]' or '{}')
     */
    inline std::runtime_error mismatchedTokenError(const CLIToken& unexpected) {
        std::string report =
            colorString("Error: ", RED) +
            "mismatched " + CLIToken::toString(unexpected.type) +
            " at position " + std::to_string(unexpected.begin) +
            (unexpected.type == CLIToken::Type::EndOfLine ? "" : " '" + unexpected.value + "'");
        if (show_source_) {
            report += "\n" + getSourceSnippetReport(unexpected.begin, unexpected.end);
        }
        return std::runtime_error(std::move(report));
    }

    /**
     * @brief Unknown token error
     */
    inline std::runtime_error unknownTokenError(const CLIToken& unknown) {
        std::string report =
            colorString("Error: ", RED) +
            "unknown token at position " + std::to_string(unknown.begin) +
            " '" + unknown.value + "'";
        if (show_source_) {
            report += "\n" + getSourceSnippetReport(unknown.begin, unknown.end);
        }
        return std::runtime_error(std::move(report));
    }

private:
    // Note: Both begin and end are inclusive
    std::string getSourceSnippetReport(int64_t begin, int64_t end) const {
        std::string source = stream_hook_.getConsumedTokens();
        int64_t source_position = stream_hook_.getPosition();
        // [begin, end] -> [highlight_begin, highlight_end)
        int64_t highlight_begin = std::max<int64_t>(begin - source_position, 0);
        int64_t highlight_end = std::min<int64_t>(end - source_position + 1, source.size());
        // Check highlight range
        if (highlight_begin > highlight_end) { return ""; } // Invalid range
        // Highlight the range
        std::string report;
        report += source.substr(0, highlight_begin);
        report += colorString(source.substr(highlight_begin, highlight_end - highlight_begin), RED);
        report += source.substr(highlight_end);
        // Add line number
        std::string line_number_str = "  " + std::to_string(stream_hook_.getLineNumber()) + " ";
        std::string prefix = std::string(line_number_str.size(), ' ');
        return line_number_str + "| " + addPrefix(prefix + "| ", report);
    }

    inline std::string colorString(std::string str, const char* color) const {
        // If str contains newline, then color each line
        std::string result;
        size_t pos = 0;
        while (pos < str.size()) {
            size_t newline_pos = str.find('\n', pos);
            if (newline_pos == std::string::npos) {
                result += color + str.substr(pos) + RESET;
                break;
            }
            result += color + str.substr(pos, newline_pos - pos) + RESET + '\n';
            pos = newline_pos + 1;
        }
        return result;
    }

    static inline std::string addPrefix(const std::string& prefix, const std::string& str) {
        std::string result;
        for (const auto& c : str) {
            if (c == '\n') {
                result += '\n';
                result += prefix;
            } else {
                result += c;
            }
        }
        return result;
    }

private:
    const CLIInputStreamHook& stream_hook_;
    bool color_output_; // Enable color output
    bool show_source_; // Show source code snippet
};

/*
Grammar:

<command>
    : <identifier> <argument_list> <end_of_line>
    ;

<argument_list>
    : <arguments>
    | <argument_list> <arguments>
    ;

<arguments>
    : <single_line_arguments>
    | { }
    | { <end_of_lines> }
    | { <muti_line_arguments> }
    | { <end_of_lines> <muti_line_arguments> <end_of_lines> }
    ;

<muti_line_arguments>
    : <single_line_arguments>
    | <muti_line_arguments> <end_of_lines> <single_line_arguments>
    ;

<single_line_arguments>
    : <argument>
    | <single_line_arguments> <argument>
    ;

<argument>
    : <identifier>
    | <string>
    | <number>
    | <vector>
    ;

<vector>
    : <number_list>
    | ( <number_list> )
    | [ <number_list> ]
    ;

<number_list>
    : <number>
    | <number_list> , <number>
    ;

<number>
    : <integer>
    | <float>
    ;
*/

struct Data {};

template <typename T>
struct ValueData : Data {
    T value;
    ValueData() = default;
    ValueData(T value) : value(std::move(value)) {}
};

using IdentifierData = ValueData<std::string>;
using StringData = ValueData<std::string>;
using IntegerData = ValueData<int64_t>;
using FloatData = ValueData<double>;
using IntegerVectorData = ValueData<std::vector<int64_t>>;
using FloatVectorData = ValueData<std::vector<double>>;

struct Argument {
    enum class Type {
        Identifier,    // IdentifierData
        String,        // StringData
        Integer,       // IntegerData
        Float,         // FloatData
        IntegerVector, // IntegerVectorData
        FloatVector,   // FloatVectorData
    };
    Type type;
    std::variant<
        StringData, // Same as IdentifierData
        IntegerData,
        FloatData,
        IntegerVectorData,
        FloatVectorData
    > data;
};

struct Command {
    std::string name;
    std::vector<Argument> arguments;
};

class CLIParser {
public:
    CLIParser(CLIInputStream& stream) : stream_hook_(stream), error_reporter_(stream_hook_), lexer_(stream_hook_) {}

    bool hasMoreCommands() {
        return lexer_.hasMoreTokens();
    }

    /**
     *  <command>
     *      : <identifier> <argument_list> <end_of_line>
     *      ;
     */
    Command parseCommand() {
        Command command;
        CLIToken token;

        while (true) {
            switch (lexer_.peekToken().type) {
                case CLIToken::Type::Identifier:
                    if (command.name.empty()) {
                        token = lexer_.nextToken();
                        command.name = token.value;
                    } else {
                        command.arguments = parseArgumentList();
                        stream_hook_.clearConsumedTokens();
                        return command;
                    }
                    break;
                case CLIToken::Type::String:
                case CLIToken::Type::Integer:
                case CLIToken::Type::Float:
                case CLIToken::Type::LeftParen:
                case CLIToken::Type::RightParen:
                case CLIToken::Type::LeftBracket:
                case CLIToken::Type::RightBracket:
                case CLIToken::Type::LeftCurly:
                case CLIToken::Type::RightCurly:
                case CLIToken::Type::Comma:
                    if (command.name.empty()) {
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.unexpectedTokenError(CLIToken::Type::Identifier, token);
                    } else {
                        command.arguments = parseArgumentList();
                        stream_hook_.clearConsumedTokens();
                        return command;
                    }
                case CLIToken::Type::EndOfLine:
                    if (command.name.empty()) {
                        lexer_.nextToken(); // Discard identifier
                        stream_hook_.clearConsumedTokens();
                    } else {
                        command.arguments = parseArgumentList();
                        stream_hook_.clearConsumedTokens();
                        return command;
                    }
                    break;
                case CLIToken::Type::Comment:
                    lexer_.nextToken(); // Discard comment
                    break;
                case CLIToken::Type::EndOfFile:
                    stream_hook_.clearConsumedTokens();
                    return command;
                case CLIToken::Type::Unknown:
                default:
                    token = lexer_.nextToken(); // Discard unexpected token
                    throw error_reporter_.unknownTokenError(token);
            }
        }
    }

private:
    /**
     * <argument_list>
     *     : <arguments>
     *     | <argument_list> <arguments>
     *     ;
     *
     * <arguments>
     *     : <single_line_arguments>
     *     | { }
     *     | { <end_of_lines> }
     *     | { <muti_line_arguments> }
     *     | { <end_of_lines> <muti_line_arguments> <end_of_lines> }
     *     ;
     *
     * <muti_line_arguments>
     *     : <single_line_arguments>
     *     | <muti_line_arguments> <end_of_lines> <single_line_arguments>
     *     ;
     *
     * <single_line_arguments>
     *     : <argument>
     *     | <single_line_arguments> <argument>
     *     ;
     */
    std::vector<Argument> parseArgumentList() {
        std::vector<Argument> arguments;
        CLIToken token;

        bool multiline = false;

        while (true) {
            switch (lexer_.peekToken().type) {
                case CLIToken::Type::Identifier:
                case CLIToken::Type::String:
                case CLIToken::Type::Integer:
                case CLIToken::Type::Float:
                case CLIToken::Type::LeftParen:
                case CLIToken::Type::RightParen:
                case CLIToken::Type::LeftBracket:
                case CLIToken::Type::RightBracket:
                    arguments.push_back(parseArgument());
                    break;
                case CLIToken::Type::LeftCurly:
                    if (multiline) {
                        // TODO: Handle nested {} blocks
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.mismatchedTokenError(token);
                    }

                    lexer_.nextToken(); // Discard left curly
                    multiline = true;
                    break;
                case CLIToken::Type::RightCurly:
                    if (!multiline) {
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.mismatchedTokenError(token);
                    }

                    lexer_.nextToken(); // Discard right curly
                    multiline = false;
                    break;
                case CLIToken::Type::Comma:
                    token = lexer_.nextToken(); // Discard unexpected token
                    throw error_reporter_.unexpectedTokenError(token);
                case CLIToken::Type::EndOfLine:
                    lexer_.nextToken(); // Discard end of line
                    if (!multiline) {
                        return arguments;
                    }
                    break;
                case CLIToken::Type::Comment:
                    lexer_.nextToken(); // Discard comment
                    break;
                case CLIToken::Type::EndOfFile:
                    if (multiline) {
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.unexpectedTokenError(CLIToken::Type::RightCurly, token);
                    }
                    return arguments;
                case CLIToken::Type::Unknown:
                default:
                    token = lexer_.nextToken(); // Discard unexpected token
                    throw error_reporter_.unknownTokenError(token);
            }
        }

        throw std::runtime_error("No way to reach here " + std::string(__FILE__) + ":" + std::to_string(__LINE__));
    }

    /**
     * <argument>
     *     : <identifier>
     *     | <string>
     *     | <number>
     *     | <vector>
     *     ;
     */
    Argument parseArgument() {
        Argument arg;
        CLIToken token;

        switch (lexer_.peekToken().type) {
            case CLIToken::Type::Identifier:
                token = lexer_.nextToken();
                arg.type = Argument::Type::Identifier;
                arg.data = IdentifierData(token.value);
                break;
            case CLIToken::Type::String:
                token = lexer_.nextToken();
                arg.type = Argument::Type::String;
                arg.data = StringData(token.value);
                break;
            case CLIToken::Type::Integer: // Integer or NumberVector
                // TODO: merge with Float case (DRY principle)
                token = lexer_.nextToken();
                if (lexer_.peekToken().type == CLIToken::Type::Comma) { // If comma is present after integer, then it's an IntegerVector or FloatVector
                    lexer_.nextToken(); // Discard comma
                    arg = parseNumberList(); // IntegerVector or FloatVector
                    // Insert the first integer into the vector
                    if (arg.type == Argument::Type::IntegerVector) {
                        auto& integer_vector_data = std::get<IntegerVectorData>(arg.data);
                        integer_vector_data.value.insert(integer_vector_data.value.begin(), std::stoll(token.value));
                    } else { // FloatVector is ok, because integer can be converted to float
                        auto& float_vector_data = std::get<FloatVectorData>(arg.data);
                        float_vector_data.value.insert(float_vector_data.value.begin(), static_cast<double>(std::stoll(token.value)));
                    }
                } else { // Integer
                    arg.type = Argument::Type::Integer;
                    arg.data = IntegerData(std::stoll(token.value));
                }
                break;
            case CLIToken::Type::Float: // Float or NumberVector
                token = lexer_.nextToken();
                if (lexer_.peekToken().type == CLIToken::Type::Comma) { // If comma is present after float, then it's must be a FloatVector
                    lexer_.nextToken(); // Discard comma
                    arg = parseNumberList(); // IntegerVector or FloatVector
                    // Insert the first float into the vector
                    if (arg.type == Argument::Type::FloatVector) {
                        auto& float_vector_data = std::get<FloatVectorData>(arg.data);
                        float_vector_data.value.insert(float_vector_data.value.begin(), std::stod(token.value));
                    } else { // IntegerVector is not ok, because float cannot be converted to integer
                        auto& integer_vector_data = std::get<IntegerVectorData>(arg.data);
                        // Convert integer vector to float vector
                        FloatVectorData float_vector_data;
                        float_vector_data.value.push_back(std::stod(token.value));
                        for (const auto& value : integer_vector_data.value) {
                            float_vector_data.value.push_back(static_cast<double>(value));
                        }
                        // Update argument
                        arg.type = Argument::Type::FloatVector;
                        arg.data = std::move(float_vector_data);
                    }
                } else { // Float
                    arg.type = Argument::Type::Float;
                    arg.data = FloatData(std::stod(token.value));
                }
                break;
            case CLIToken::Type::LeftParen:
            case CLIToken::Type::LeftBracket:
                // Handle vectors
                arg = parseVector();
                break;
            case CLIToken::Type::RightParen:
            case CLIToken::Type::RightBracket:
                token = lexer_.nextToken(); // Discard unexpected token
                throw error_reporter_.unexpectedTokenError(token);
            case CLIToken::Type::LeftCurly:
            case CLIToken::Type::RightCurly:
            case CLIToken::Type::Comma:
            case CLIToken::Type::EndOfLine:
            case CLIToken::Type::Comment:
            case CLIToken::Type::EndOfFile:
            case CLIToken::Type::Unknown:
            default:
                throw std::runtime_error("No way to reach here " + std::string(__FILE__) + ":" + std::to_string(__LINE__));
        }
        return arg;
    }

    /**
     * <vector>
     *     : <number_list>
     *     | ( <number_list> )
     *     | [ <number_list> ]
     *     ;
     */
    Argument parseVector() {
        Argument arg;
        CLIToken token;

        switch (lexer_.peekToken().type) {
            case CLIToken::Type::Integer:
            case CLIToken::Type::Float:
                arg = parseNumberList(); // IntegerVector or FloatVector
                break;
            case CLIToken::Type::LeftParen:
                lexer_.nextToken(); // Discard left paren
                arg = parseNumberList(); // IntegerVector or FloatVector
                token = lexer_.nextToken();
                if (token.type != CLIToken::Type::RightParen) {
                    throw error_reporter_.unexpectedTokenError(CLIToken::Type::RightParen, token);
                }
                break;
            case CLIToken::Type::LeftBracket:
                lexer_.nextToken(); // Discard left bracket
                arg = parseNumberList(); // IntegerVector or FloatVector
                token = lexer_.nextToken();
                if (token.type != CLIToken::Type::RightBracket) {
                    throw error_reporter_.unexpectedTokenError(CLIToken::Type::RightBracket, token);
                }
                break;
            default:
                throw std::runtime_error("No way to reach here " + std::string(__FILE__) + ":" + std::to_string(__LINE__));
        }
        return arg;
    }

    /**
     * <number_list>
     *     : <number>
     *     | <number_list> , <number>
     *     ;
     */
    Argument parseNumberList() {
        Argument arg;
        CLIToken token;

        bool comma = true; // Disallow comma at the beginning
        bool first_number = true;
        std::vector<CLIToken> tokens;
        bool integer_vector = true; // If only integers are present, then it's an integer vector

        // Parse tokens into integer vector
        auto parseIntegerVector = [&]() {
            IntegerVectorData data;
            for (const auto& token : tokens) {
                assert(token.type == CLIToken::Type::Integer || token.type == CLIToken::Type::Float);
                data.value.push_back(token.type == CLIToken::Type::Integer ? std::stoll(token.value) : static_cast<int64_t>(std::stod(token.value)));
            }
            arg.type = Argument::Type::IntegerVector;
            arg.data = std::move(data);
        };
        // Parse tokens into float vector
        auto parseFloatVector = [&]() {
            FloatVectorData data;
            for (const auto& token : tokens) {
                assert(token.type == CLIToken::Type::Integer || token.type == CLIToken::Type::Float);
                data.value.push_back(token.type == CLIToken::Type::Integer ? static_cast<double>(std::stoll(token.value)) : std::stod(token.value));
            }
            arg.type = Argument::Type::FloatVector;
            arg.data = std::move(data);
        };

        while (true) {
            switch (lexer_.peekToken().type) {
                case CLIToken::Type::Integer:
                    if (!comma) {
                        if (tokens.size() == 1) {
                            token = lexer_.nextToken(); // Discard unexpected token
                            throw error_reporter_.unexpectedTokenError(CLIToken::Type::Comma, token);
                        }
                        // Return the argument
                        if (integer_vector) {
                            parseIntegerVector();
                        } else {
                            parseFloatVector();
                        }
                        return arg;
                    }
                    comma = false;

                    if (first_number) {
                        first_number = false;
                    }
                    token = lexer_.nextToken();
                    tokens.push_back(std::move(token));
                    break;
                case CLIToken::Type::Float:
                    if (!comma) {
                        if (tokens.size() == 1) {
                            token = lexer_.nextToken(); // Discard unexpected token
                            throw error_reporter_.unexpectedTokenError(CLIToken::Type::Comma, token);
                        }
                        // Return the argument
                        if (integer_vector) {
                            parseIntegerVector();
                        } else {
                            parseFloatVector();
                        }
                        return arg;
                    }
                    comma = false;

                    if (first_number) {
                        first_number = false;
                    }
                    token = lexer_.nextToken();
                    tokens.push_back(std::move(token));
                    integer_vector = false; // If a float is present, then it's a float vector
                    break;
                case CLIToken::Type::Comma:
                    if (comma) {
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.unexpectedTokenError("number", token);
                    }

                    lexer_.nextToken(); // Discard comma
                    comma = true;
                    break;
                default:
                    if (first_number) {
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.unexpectedTokenError("number", token);
                    }
                    if (comma) {
                        token = lexer_.nextToken(); // Discard unexpected token
                        throw error_reporter_.unexpectedTokenError("number", token);
                    }

                    if (integer_vector) {
                        parseIntegerVector();
                    } else {
                        parseFloatVector();
                    }
                    return arg;
            }
        }

        return arg;
    }

private:
    CLIInputStreamHook stream_hook_;
    ErrorReporter error_reporter_;
    CLILexer lexer_;
};

}
