#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <charconv>

namespace prismcraft {

class JsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    Type type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::unordered_map<std::string, JsonValue> objectValue;

    static const JsonValue s_nullValue;

    JsonValue() : type(Type::Null) {}
    explicit JsonValue(bool val) : type(Type::Boolean), boolValue(val) {}
    explicit JsonValue(double val) : type(Type::Number), numberValue(val) {}
    explicit JsonValue(int val) : type(Type::Number), numberValue(static_cast<double>(val)) {}
    explicit JsonValue(const std::string& val) : type(Type::String), stringValue(val) {}
    explicit JsonValue(const char* val) : type(Type::String), stringValue(val) {}

    [[nodiscard]] bool isNull() const { return type == Type::Null; }
    [[nodiscard]] bool isBool() const { return type == Type::Boolean; }
    [[nodiscard]] bool isNumber() const { return type == Type::Number; }
    [[nodiscard]] bool isString() const { return type == Type::String; }
    [[nodiscard]] bool isArray() const { return type == Type::Array; }
    [[nodiscard]] bool isObject() const { return type == Type::Object; }

    [[nodiscard]] bool asBool(bool defaultVal = false) const {
        return (type == Type::Boolean) ? boolValue : defaultVal;
    }

    [[nodiscard]] double asDouble(double defaultVal = 0.0) const {
        return (type == Type::Number) ? numberValue : defaultVal;
    }

    [[nodiscard]] float asFloat(float defaultVal = 0.0f) const {
        return (type == Type::Number) ? static_cast<float>(numberValue) : defaultVal;
    }

    [[nodiscard]] int asInt(int defaultVal = 0) const {
        return (type == Type::Number) ? static_cast<int>(numberValue) : defaultVal;
    }

    [[nodiscard]] const std::string& asString(const std::string& defaultVal = "") const {
        return (type == Type::String) ? stringValue : defaultVal;
    }

    [[nodiscard]] const std::vector<JsonValue>& asArray() const {
        return arrayValue;
    }

    [[nodiscard]] const std::unordered_map<std::string, JsonValue>& asObject() const {
        return objectValue;
    }

    [[nodiscard]] bool contains(const std::string& key) const {
        return (type == Type::Object) && (objectValue.find(key) != objectValue.end());
    }

    const JsonValue& operator[](const std::string& key) const {
        if (type != Type::Object) return s_nullValue;
        auto it = objectValue.find(key);
        return (it != objectValue.end()) ? it->second : s_nullValue;
    }

    const JsonValue& operator[](size_t index) const {
        if (type != Type::Array || index >= arrayValue.size()) return s_nullValue;
        return arrayValue[index];
    }

    // Parser
    static JsonValue parse(const std::string& text, std::string& outError) {
        size_t pos = 0;
        skipWhitespaceAndComments(text, pos);
        if (pos >= text.size()) {
            outError = "Empty JSON input";
            return JsonValue();
        }
        JsonValue val = parseValue(text, pos, outError);
        return val;
    }

    static JsonValue parseFile(const std::string& filePath, std::string& outError) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            outError = "Could not open file: " + filePath;
            return JsonValue();
        }
        std::stringstream ss;
        ss << file.rdbuf();
        return parse(ss.str(), outError);
    }

private:
    static void skipWhitespaceAndComments(const std::string& text, size_t& pos) {
        while (pos < text.size()) {
            if (std::isspace(static_cast<unsigned char>(text[pos]))) {
                pos++;
            } else if (text[pos] == '/' && pos + 1 < text.size() && text[pos + 1] == '/') {
                // Line comment
                pos += 2;
                while (pos < text.size() && text[pos] != '\n') pos++;
            } else if (text[pos] == '/' && pos + 1 < text.size() && text[pos + 1] == '*') {
                // Block comment
                pos += 2;
                while (pos + 1 < text.size() && !(text[pos] == '*' && text[pos + 1] == '/')) pos++;
                if (pos + 1 < text.size()) pos += 2;
            } else {
                break;
            }
        }
    }

    static JsonValue parseValue(const std::string& text, size_t& pos, std::string& outError) {
        skipWhitespaceAndComments(text, pos);
        if (pos >= text.size()) {
            outError = "Unexpected end of JSON";
            return JsonValue();
        }

        char c = text[pos];
        if (c == '{') return parseObject(text, pos, outError);
        if (c == '[') return parseArray(text, pos, outError);
        if (c == '"') return parseString(text, pos, outError);
        if (c == 't' || c == 'f') return parseBool(text, pos, outError);
        if (c == 'n') return parseNull(text, pos, outError);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(text, pos, outError);

        outError = std::string("Unexpected token '") + c + "' at position " + std::to_string(pos);
        return JsonValue();
    }

    static JsonValue parseObject(const std::string& text, size_t& pos, std::string& outError) {
        JsonValue val;
        val.type = Type::Object;
        pos++; // Skip '{'

        while (pos < text.size()) {
            skipWhitespaceAndComments(text, pos);
            if (pos >= text.size()) {
                outError = "Unterminated object";
                return val;
            }
            if (text[pos] == '}') {
                pos++; // Skip '}'
                return val;
            }

            if (text[pos] != '"') {
                outError = "Expected string key in object at position " + std::to_string(pos);
                return val;
            }

            JsonValue keyVal = parseString(text, pos, outError);
            if (!outError.empty()) return val;

            skipWhitespaceAndComments(text, pos);
            if (pos >= text.size() || text[pos] != ':') {
                outError = "Expected ':' after key at position " + std::to_string(pos);
                return val;
            }
            pos++; // Skip ':'

            JsonValue childVal = parseValue(text, pos, outError);
            if (!outError.empty()) return val;

            val.objectValue[keyVal.stringValue] = std::move(childVal);

            skipWhitespaceAndComments(text, pos);
            if (pos < text.size() && text[pos] == ',') {
                pos++; // Skip ','
            } else if (pos < text.size() && text[pos] == '}') {
                pos++; // Skip '}'
                return val;
            } else {
                outError = "Expected ',' or '}' in object at position " + std::to_string(pos);
                return val;
            }
        }

        outError = "Unterminated object at end of file";
        return val;
    }

    static JsonValue parseArray(const std::string& text, size_t& pos, std::string& outError) {
        JsonValue val;
        val.type = Type::Array;
        pos++; // Skip '['

        while (pos < text.size()) {
            skipWhitespaceAndComments(text, pos);
            if (pos >= text.size()) {
                outError = "Unterminated array";
                return val;
            }
            if (text[pos] == ']') {
                pos++; // Skip ']'
                return val;
            }

            JsonValue childVal = parseValue(text, pos, outError);
            if (!outError.empty()) return val;

            val.arrayValue.push_back(std::move(childVal));

            skipWhitespaceAndComments(text, pos);
            if (pos < text.size() && text[pos] == ',') {
                pos++; // Skip ','
            } else if (pos < text.size() && text[pos] == ']') {
                pos++; // Skip ']'
                return val;
            } else {
                outError = "Expected ',' or ']' in array at position " + std::to_string(pos);
                return val;
            }
        }

        outError = "Unterminated array at end of file";
        return val;
    }

    static JsonValue parseString(const std::string& text, size_t& pos, std::string& outError) {
        JsonValue val;
        val.type = Type::String;
        pos++; // Skip opening '"'

        std::string s;
        while (pos < text.size()) {
            char c = text[pos++];
            if (c == '"') {
                val.stringValue = std::move(s);
                return val;
            }
            if (c == '\\' && pos < text.size()) {
                char esc = text[pos++];
                switch (esc) {
                    case '"':  s.push_back('"'); break;
                    case '\\': s.push_back('\\'); break;
                    case '/':  s.push_back('/'); break;
                    case 'b':  s.push_back('\b'); break;
                    case 'f':  s.push_back('\f'); break;
                    case 'n':  s.push_back('\n'); break;
                    case 'r':  s.push_back('\r'); break;
                    case 't':  s.push_back('\t'); break;
                    default:   s.push_back(esc); break;
                }
            } else {
                s.push_back(c);
            }
        }

        outError = "Unterminated string literal";
        return val;
    }

    static JsonValue parseNumber(const std::string& text, size_t& pos, std::string& outError) {
        size_t start = pos;
        if (text[pos] == '-') pos++;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) pos++;
        if (pos < text.size() && text[pos] == '.') {
            pos++;
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) pos++;
        }
        if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
            pos++;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) pos++;
            while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos]))) pos++;
        }

        std::string numStr = text.substr(start, pos - start);
        try {
            double d = std::stod(numStr);
            return JsonValue(d);
        } catch (...) {
            outError = "Invalid number format: " + numStr;
            return JsonValue();
        }
    }

    static JsonValue parseBool(const std::string& text, size_t& pos, std::string& outError) {
        if (text.compare(pos, 4, "true") == 0) {
            pos += 4;
            return JsonValue(true);
        }
        if (text.compare(pos, 5, "false") == 0) {
            pos += 5;
            return JsonValue(false);
        }
        outError = "Invalid boolean literal at position " + std::to_string(pos);
        return JsonValue();
    }

    static JsonValue parseNull(const std::string& text, size_t& pos, std::string& outError) {
        if (text.compare(pos, 4, "null") == 0) {
            pos += 4;
            return JsonValue();
        }
        outError = "Invalid null literal at position " + std::to_string(pos);
        return JsonValue();
    }
};

inline const JsonValue JsonValue::s_nullValue{};

} // namespace prismcraft
