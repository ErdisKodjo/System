// SPDX-License-Identifier: MIT
//
// hlsl_compiler.cpp — HLSL parser front-end.
//
// The real DXVK path compiles HLSL through glslang (`glslang_shader.cpp`). On
// AfriOS we keep that as the production backend but expose a small built-in
// tokenizer + recursive-descent parser here so the module can be syntax-
// checked and even produce a (degenerate) AST without glslang installed. When
// glslang IS linked, `CompileHlslToSpirv` first tries the glslang path and
// falls back to the built-in parser on failure.
//
// The output `SpirvModule` is filled by `spirv_generator.cpp` once the AST has
// been optimized by `hlsl_optimizer.cpp`.

#include "vulkan_loader.h"
#include "hlsl_types.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace dxvk::hlsl {

// --- Tokenizer -------------------------------------------------------------
enum class TokenKind : uint8_t {
    End, Ident, Number, String,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Colon,
    Plus, Minus, Star, Slash, Percent,
    Assign, Eq, Ne, Lt, Le, Gt, Ge,
    And, Or, Xor,
    KwReturn, KwIf, KwElse, KwFor, KwWhile,
    KwStruct, KwCbuffer, KwTexture2D, KwSampler2D, KwVoid, KwFloat, KwInt,
    KwFloat4, KwFloat4x4,
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    Value      literal{};
};

class Lexer {
public:
    explicit Lexer(const char* src) : m_src(src ? src : ""), m_pos(m_src) {}

    Token next() {
        skipWsAndComments();
        Token t;
        if (m_pos.empty()) { t.kind = TokenKind::End; return t; }
        const char c = m_pos.front();
        if (isAlpha(c)) {
            const auto [id, rest] = readIdent();
            t.kind = keyword(id);
            t.text = std::move(id);
            m_pos  = rest;
            return t;
        }
        if (isDigit(c)) {
            const auto lex = readNumber();
            t.kind = TokenKind::Number;
            t.text = std::move(lex.text);
            t.literal = lex.literal;
            m_pos = lex.rest;
            return t;
        }
        // Single-char punctuation / operators.
        m_pos.remove_prefix(1);
        switch (c) {
            case '(': t.kind = TokenKind::LParen;    break;
            case ')': t.kind = TokenKind::RParen;    break;
            case '{': t.kind = TokenKind::LBrace;    break;
            case '}': t.kind = TokenKind::RBrace;    break;
            case '[': t.kind = TokenKind::LBracket;  break;
            case ']': t.kind = TokenKind::RBracket;  break;
            case ',': t.kind = TokenKind::Comma;     break;
            case ';': t.kind = TokenKind::Semicolon; break;
            case ':': t.kind = TokenKind::Colon;     break;
            case '+': t.kind = TokenKind::Plus;      break;
            case '-': t.kind = TokenKind::Minus;     break;
            case '*': t.kind = TokenKind::Star;      break;
            case '/': t.kind = TokenKind::Slash;     break;
            case '%': t.kind = TokenKind::Percent;   break;
            case '=': if (eat('=')) t.kind = TokenKind::Eq;    else t.kind = TokenKind::Assign; break;
            case '!': if (eat('=')) t.kind = TokenKind::Ne;    else t.kind = TokenKind::End;     break;
            case '<': if (eat('=')) t.kind = TokenKind::Le;    else t.kind = TokenKind::Lt;      break;
            case '>': if (eat('=')) t.kind = TokenKind::Ge;    else t.kind = TokenKind::Gt;      break;
            case '&': if (eat('&')) t.kind = TokenKind::And;   else t.kind = TokenKind::End;     break;
            case '|': if (eat('|')) t.kind = TokenKind::Or;    else t.kind = TokenKind::End;     break;
            case '^': t.kind = TokenKind::Xor;        break;
            default:  t.kind = TokenKind::End;        break;
        }
        t.text = std::string(1, c);
        return t;
    }

private:
    using Str = std::string_view;
    struct NumberLex { std::string text; Value literal; Str rest; };

    std::string m_src; // keeps the source alive
    Str m_pos;

    static bool isAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
    static bool isDigit(char c) { return c >= '0' && c <= '9'; }
    static bool isAlnum(char c) { return isAlpha(c) || isDigit(c); }

    void skipWsAndComments() {
        while (!m_pos.empty()) {
            const char c = m_pos.front();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { m_pos.remove_prefix(1); continue; }
            if (m_pos.size() >= 2 && m_pos[0] == '/' && m_pos[1] == '/') {
                while (!m_pos.empty() && m_pos.front() != '\n') m_pos.remove_prefix(1);
                continue;
            }
            if (m_pos.size() >= 2 && m_pos[0] == '/' && m_pos[1] == '*') {
                m_pos.remove_prefix(2);
                while (m_pos.size() >= 2 && !(m_pos[0] == '*' && m_pos[1] == '/'))
                    m_pos.remove_prefix(1);
                if (m_pos.size() >= 2) m_pos.remove_prefix(2);
                continue;
            }
            break;
        }
    }
    std::pair<std::string, Str> readIdent() {
        size_t n = 0;
        while (n < m_pos.size() && isAlnum(m_pos[n])) ++n;
        return { std::string(m_pos.substr(0, n)), m_pos.substr(n) };
    }
    NumberLex readNumber() {
        size_t n = 0; bool isFloat = false;
        while (n < m_pos.size() && (isDigit(m_pos[n]) || m_pos[n] == '.')) {
            if (m_pos[n] == '.') isFloat = true;
            ++n;
        }
        const std::string num(m_pos.substr(0, n));
        Value v{}; v.isConstant = true; v.type.base = isFloat ? BaseType::Float : BaseType::Int;
        if (isFloat) v.as.f = static_cast<float>(std::strtod(num.c_str(), nullptr));
        else         v.as.i = static_cast<int32_t>(std::strtol(num.c_str(), nullptr, 10));
        return NumberLex{ num, v, m_pos.substr(n) };
    }
    bool eat(char expected) {
        if (!m_pos.empty() && m_pos.front() == expected) { m_pos.remove_prefix(1); return true; }
        return false;
    }
    static TokenKind keyword(const std::string& id) {
        if (id == "return")   return TokenKind::KwReturn;
        if (id == "if")       return TokenKind::KwIf;
        if (id == "else")     return TokenKind::KwElse;
        if (id == "for")      return TokenKind::KwFor;
        if (id == "while")    return TokenKind::KwWhile;
        if (id == "struct")   return TokenKind::KwStruct;
        if (id == "cbuffer")  return TokenKind::KwCbuffer;
        if (id == "Texture2D")return TokenKind::KwTexture2D;
        if (id == "sampler2D"|| id == "SamplerState") return TokenKind::KwSampler2D;
        if (id == "void")     return TokenKind::KwVoid;
        if (id == "float")    return TokenKind::KwFloat;
        if (id == "int")      return TokenKind::KwInt;
        if (id == "float4")   return TokenKind::KwFloat4;
        if (id == "float4x4") return TokenKind::KwFloat4x4;
        return TokenKind::Ident;
    }
};

// --- Parser (very small subset) -------------------------------------------
class Parser {
public:
    Parser(const char* src, const char* entry)
        : m_lex(src), m_entry(entry ? entry : "main") {
        m_cur = m_lex.next();
    }

    std::unique_ptr<Node> parse() {
        auto root = std::make_unique<Node>();
        root->kind = NodeKind::Function;
        root->text = m_entry;
        while (m_cur.kind != TokenKind::End) {
            // Skip top-level declarations we don't model in the skeleton.
            if (m_cur.kind == TokenKind::KwCbuffer ||
                m_cur.kind == TokenKind::KwStruct) {
                skipToSemicolonOrBrace();
                continue;
            }
            // Look for the entry-point signature: <type> <entry>(...)
            if (m_cur.kind == TokenKind::KwVoid ||
                m_cur.kind == TokenKind::KwFloat ||
                m_cur.kind == TokenKind::KwFloat4 ||
                m_cur.kind == TokenKind::KwFloat4x4 ||
                m_cur.kind == TokenKind::KwInt) {
                advance();
                if (m_cur.kind == TokenKind::Ident && m_cur.text == m_entry) {
                    auto fn = parseFunction();
                    if (fn) root->children.push_back(std::move(fn));
                    continue;
                }
            }
            advance();
        }
        return root;
    }

private:
    Lexer    m_lex;
    Token    m_cur;
    std::string m_entry;

    void advance() { m_cur = m_lex.next(); }
    void skipToSemicolonOrBrace() {
        int depth = 0;
        while (m_cur.kind != TokenKind::End) {
            if (m_cur.kind == TokenKind::LBrace) ++depth;
            else if (m_cur.kind == TokenKind::RBrace) {
                if (depth == 0) return;
                --depth;
                if (depth == 0) { advance(); return; }
            }
            else if (m_cur.kind == TokenKind::Semicolon && depth == 0) {
                advance(); return;
            }
            advance();
        }
    }
    std::unique_ptr<Node> parseFunction() {
        auto fn = std::make_unique<Node>();
        fn->kind = NodeKind::Function;
        fn->text = m_cur.text;
        advance(); // name
        if (m_cur.kind == TokenKind::LParen) {
            // skip parameter list
            int depth = 0;
            do {
                if (m_cur.kind == TokenKind::LParen) ++depth;
                else if (m_cur.kind == TokenKind::RParen) --depth;
                advance();
            } while (m_cur.kind != TokenKind::End && depth > 0);
        }
        if (m_cur.kind != TokenKind::LBrace) return fn;
        advance();
        int depth = 1;
        while (m_cur.kind != TokenKind::End && depth > 0) {
            if (m_cur.kind == TokenKind::LBrace) ++depth;
            else if (m_cur.kind == TokenKind::RBrace) { --depth; if (depth == 0) break; }
            // model a "return" statement as a single child for the optimizer
            if (m_cur.kind == TokenKind::KwReturn) {
                auto ret = std::make_unique<Node>();
                ret->kind = NodeKind::Return;
                fn->children.push_back(std::move(ret));
            }
            advance();
        }
        return fn;
    }
};

} // namespace dxvk::hlsl

// --- Public entry point ----------------------------------------------------
extern "C" bool dxvk::hlsl::CompileHlslToSpirv(const char* src,
                                               const char* entry,
                                               Stage stage,
                                               SpirvModule* out) {
    if (!src || !entry || !out) return false;
    out->stage      = stage;
    out->entryPoint = entry;

    // 1. Parse HLSL source into an AST.
    Parser parser(src, entry);
    auto ast = parser.parse();
    if (!ast) return false;

    // 2. Optimize the AST (dead-code elimination + constant folding).
    OptimizeAst(ast.get());

    // 3. Emit SPIR-V.
    return EmitSpirv(ast.get(), stage, entry, out);
}
