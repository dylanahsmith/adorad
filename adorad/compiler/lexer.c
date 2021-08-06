/*
          _____   ____  _____            _____  
    /\   |  __ \ / __ \|  __ \     /\   |  __ \ 
   /  \  | |  | | |  | | |__) |   /  \  | |  | | Adorad - The Fast, Expressive & Elegant Programming Language
  / /\ \ | |  | | |  | |  _  /   / /\ \ | |  | | Languages: C, C++, and Assembly
 / ____ \| |__| | |__| | | \ \  / ____ \| |__| | https://github.com/adorad/adorad/
/_/    \_\_____/ \____/|_|  \_\/_/    \_\_____/ 

Licensed under the MIT License <http://opensource.org/licenses/MIT>
SPDX-License-Identifier: MIT
Copyright (c) 2021 Jason Dsouza <@jasmcaus>
*/

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <adorad/compiler/lexer.h>

// Get the current character in the Lexical buffer
// NB: This does not increase the offset
#define LEXER_CURR_CHAR           buff_at(lexer->buffer, lexer->offset)

// Reset the line
#define LEXER_RESET_LINENO        lexer->loc->line = 0
// Reset the column number 
#define LEXER_RESET_COLNO         lexer->loc->col = 0

// Increment the line number
#define LEXER_INCREMENT_LINENO    ++lexer->loc->line; LEXER_RESET_COLNO
// Decrement the line
#define LEXER_DECREMENT_LINENO    --lexer->loc->line; LEXER_RESET_COLNO
// Increment the column number
#define LEXER_INCREMENT_COLNO     ++lexer->loc->col 
// Decrement the col
#define LEXER_DECREMENT_COLNO     --lexer->loc->col

// Increment the Lexical Buffer offset
#define LEXER_INCREMENT_OFFSET    ++lexer->offset; LEXER_INCREMENT_COLNO
// Decrement the Lexical Buffer offset
#define LEXER_DECREMENT_OFFSET    --lexer->offset; LEXER_DECREMENT_COLNO

// Increment the Lexical Buffer offset without affecting `col`
#define LEXER_INCREMENT_OFFSET_ONLY   ++lexer->offset;
// Decrement the Lexical Buffer offset without affecting `col`
#define LEXER_DECREMENT_OFFSET_ONLY   --lexer->offset;

// Reset the buffer 
#define LEXER_RESET_BUFFER      \
    buff_reset(lexer->buffer)

// Reset the Lexer state
#define LEXER_RESET             \
    buff_reset(lexer->buffer);  \
    lexer->offset = 0;          \
    loc_reset(lexer->loc)

// String representation of a TokenKind
// To access the string representation of a TokenKind object, simply use `tokenHash[tokenkind]`
// NB: This is here as opposed to in `tokens.[hc]` because it isn't used there and simply leads to more headaches
// (multiple redefinition errors yada yada)
static const char* tokenHash[] = {
    #define TOKENKIND(kind, str)    str
        ALLTOKENS
    #undef TOKENKIND
};

// These macros are used in the switch() statements below during the Lexing of Adorad source files.
#define WHITESPACE_NO_NEWLINE \
    ' ': case '\r': case '\t': case '\v': case '\f'

#define DIGIT_NON_ZERO  \
    '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9'

#define DIGIT \
    '0': case DIGIT_NON_ZERO

#define HEX_DIGIT \
    'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
    case DIGIT

#define ALPHA_EXCEPT_B_O_X \
         'a': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': \
    case 'm': case 'n': case 'p': case 'q': case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'y': \
    case 'z': case 'A': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J': case 'K': \
    case 'L': case 'M': case 'N':case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U': case 'V': case 'W':  \
    case 'Y': case 'Z'  \

#define ALPHA \
         'b': case 'o': case 'x': case 'B': case 'O': case 'X': case ALPHA_EXCEPT_B_O_X


Lexer* lexer_init(char* buffer, const char* fname) {
    Lexer* lexer = (Lexer*)calloc(1, sizeof(Lexer));

    lexer->offset = 0;
    lexer->buffer = buff_new(buffer);
    lexer->toklist = vec_new(Token, TOKENLIST_ALLOC_CAPACITY);
    lexer->loc = loc_new(fname);

    return lexer;
}

static void lexer_toklist_push(Lexer* lexer, Token* token) {
    vec_push(lexer->toklist, token);
}

static void lexer_free(Lexer* lexer) {
    if(lexer) {
        vec_free(lexer->toklist);
        buff_free(lexer->buffer);
        loc_free(lexer->loc);
        free(lexer);
    }
}

// Report an error and exit
void lexer_error(Lexer* lexer, Error e, const char* format, ...) {
    va_list vl;
    va_start(vl, format);
    switch(e) {
        case SyntaxError:
            fprintf(stderr, "%sSyntaxError: ", "\033[1;31m");
            break;
        case ParseError:
            fprintf(stderr, "%sSyntaxError: ", "\033[1;31m");
            break;
        default:
            fprintf(stderr, "%sError: ", "\033[1;31m");
            break;
    }
    vfprintf(stderr, format, vl);
    fprintf(stderr, " at %s:%d:%d%s\n", lexer->loc->fname->data, lexer->loc->line,lexer->loc->col, "\033[0m");
    va_end(vl);
    exit(1);
}

// Returns the curent character in the Lexical Buffer and advances to the next element
// It does this by incrementing the buffer offset.
static inline char lexer_advance(Lexer* lexer) {
    if(lexer->offset >= buff_len(lexer->buffer))
        return nullchar;
    
    LEXER_INCREMENT_COLNO;
    // Do _not_ use `buff_at(lexer->buffer, lexer->offset++)` here
    return lexer->buffer->data[lexer->offset++];
}

// Advance `n` characters in the Lexical Buffer
static inline char lexer_advancen(Lexer* lexer, UInt32 n) {
    // The '>=' is here because offset and buffer_capacity are off by 1 (0-index vs 1-index respectively)
    if(lexer->offset + n >= buff_len(lexer->buffer))
        return nullchar;
    
    lexer->loc->col += n;
    lexer->offset += n;
    return lexer->buffer->data[lexer->offset];
}

// Returns the previous element in the Lexical buffer.
// This is non-destructive --> the buffer offset is not updated.
static inline char lexer_prev(Lexer* lexer) {
    if(lexer->offset <= 0)
        return nullchar;

    return (char)lexer->buffer->data[lexer->offset - 1];
}

// Returns the previous `n` elements in the Lexical buffer.
// This is non-destructive --> the buffer offset is not updated.
static inline char lexer_prevn(Lexer* lexer, UInt32 n) {
    if(lexer->offset - n + 1 <= 0)
        return nullchar;

    return (char)lexer->buffer->data[lexer->offset - n];
}

// Returns the current element in the Lexical Buffer.
static inline char lexer_peek(Lexer* lexer) {
    return buff_at(lexer->buffer, lexer->offset);
}

// "Look ahead" `n` characters in the Lexical buffer.
// It _does not_ increment the buffer offset.
static inline char lexer_peekn(Lexer* lexer, UInt32 n) {
    if(lexer->offset + n >= buff_len(lexer->buffer))
        return nullchar;
    
    return (char)lexer->buffer->data[lexer->offset + n];
}

static void lexer_maketoken(Lexer* lexer, TokenKind kind, Buff* value, UInt32 offset, UInt32 line, UInt32 col) {  
    Token* token = token_init();
    CSTL_CHECK_NOT_NULL(token, "Could not allocate memory. Memory full.");

    if(value == null) {
        printf("VALUE IS NONE!!!\n");
    }
    if(!value->data) {
        value = token_to_buff(kind);

        if(kind == STRING || kind == IDENTIFIER || kind == INTEGER || kind == HEX_INT || kind == BIN_INT ||
           kind == OCT_INT)
            CSTL_WARN("Expected a token value. Got `null`\n");
    }

    token->kind = kind;
    token->offset = offset;
    token->loc->col = col;
    token->loc->line = line;
    buff_set(token->value, value->data);
    buff_set(token->loc->fname, lexer->loc->fname->data);
    lexer_toklist_push(lexer, token);
}

// Scan a comment (single line)
// We store comments in the lexing phase. The Parser will decide which comments are actually useful and which
// aren't
static inline void lexer_lex_sl_comment(Lexer* lexer) {
    char ch = lexer_advance(lexer);
    int comment_length = 0;
    UInt32 prev_offset = lexer->offset - 1;
    UInt32 line = lexer->loc->line;
    UInt32 col = lexer->loc->col;

    while(ch && ch != '\n') {
        ch = lexer_advance(lexer);
        ++comment_length;
    }

    CSTL_CHECK_EQ(ch, '\n');
    
    // Do not store empty comments
    if(comment_length == 0) 
        return;

    Buff* comment_value = buff_slice(lexer->buffer, prev_offset, comment_length - 1);
    CSTL_CHECK_NOT_NULL(comment_value, "`comment_value` must not be null");
    if(!comment_value)
        printf("comment_value = null\n");
    lexer_maketoken(lexer, COMMENT, comment_value, prev_offset, line, col);

    LEXER_DECREMENT_OFFSET;
}

// Scan a comment (multi-line)
// We have no reason, at the moment, to store a multi-line comment as a Token
static inline void lexer_lex_ml_comment(Lexer* lexer) {
    char ch = lexer_advance(lexer);
    bool asterisk_found = false; 
    while(ch && !(ch == '/' && asterisk_found)) {
        asterisk_found = false; 
        while(ch && ch!= '*')
            ch = lexer_advance(lexer);
        
        if(ch == '*')
            asterisk_found = true;

        ch = lexer_advance(lexer);
    }
    ch = lexer_advance(lexer);
}

// Scan a character
static inline void lexer_lex_char(Lexer* lexer) {
    char ch = lexer_advance(lexer);
    if(ch) {
        LEXER_INCREMENT_OFFSET;
        if(ch == '\n') {
            LEXER_INCREMENT_LINENO;
            LEXER_RESET_COLNO;
        }
    }
}

// Scan an escape char
static inline void lexer_lex_esc_char(Lexer* lexer) {
}

// Scan a macro (begins with `@`)
static inline void lexer_lex_macro(Lexer* lexer) {
    char ch = lexer_advance(lexer);
    int macro_length = 0;

    // Don't include the `@` in the macro symbol name
    UInt32 prev_offset = lexer->offset;
    UInt32 line = lexer->loc->line;
    UInt32 col = lexer->loc->col;

    while(isLetter(ch) || isDigit(ch)) {
        ch = lexer_advance(lexer);
        ++macro_length;
    }

    if(macro_length > MAX_TOKEN_LENGTH)
        CSTL_WARN(A number can never have more than 256 characters);

    UInt32 offset_diff = lexer->offset - prev_offset;
    printf("Offset_diff = %d\n", offset_diff);

    Buff* macro_value = buff_slice(lexer->buffer, prev_offset - 1, offset_diff);
    CSTL_CHECK_NOT_NULL(macro_value, "`macro_value` must not be null");
    if(!macro_value)
        printf("macro_value = null\n");
    lexer_maketoken(lexer, MACRO, macro_value, prev_offset - 1, line, col - 1);

    LEXER_DECREMENT_OFFSET;
}

// Scan a string
static inline void lexer_lex_string(Lexer* lexer) {
    // We already know that the curr char is _not_ a quote (`"`) since an empty string token (`""`) is
    // handled by `lexer_lex()`
    CSTL_CHECK_NE(LEXER_CURR_CHAR, '"');
    char ch = lexer_advance(lexer);
    int str_length = 0;
    UInt32 prev_offset = lexer->offset - 1;
    UInt32 line = lexer->loc->line;
    UInt32 col = lexer->loc->col;
    lexer->is_inside_str = true;

    while(ch != '"') {
        if(ch == '\\') {
            // lexer_lex_esc_char(lexer);
            ch = lexer_advance(lexer);
        } else {
            ch = lexer_advance(lexer);
        }
        ++str_length;
    }
    lexer->is_inside_str = false;

    CSTL_CHECK_EQ(ch, '"');
    UInt32 offset_diff = lexer->offset - prev_offset;

    // `offset_diff - 1` so as to ignore the closing quote `"`
    Buff* str_value = buff_slice(lexer->buffer, prev_offset, offset_diff - 1);
    CSTL_CHECK_NOT_NULL(str_value, "`str_value` must not be null");
    if(!str_value)
        printf("str_value = null\n");
    lexer_maketoken(lexer, STRING, str_value, prev_offset - 1, line, col - 1);
}

// Returns whether `value` is a keyword or an identifier
static inline TokenKind lexer_is_keyword_or_identifier(char* value) {
    // Search `tokenHash` for a match for `value`. 
    // If we can't find one, we assume an identifier
    for(TokenKind tokenkind = TOK___KEYWORDS_BEGIN + 1; tokenkind < TOK___KEYWORDS_END; tokenkind++)
        if(strcmp(tokenHash[tokenkind], value) == 0)
            return tokenkind; // Found a match

    // If we're still here, we haven't found a keyword match
    return IDENTIFIER;
}

// Scan an identifier
static inline void lexer_lex_identifier(Lexer* lexer) {
    // When this function is called, we alread know that the first character statisfies the `case ALPHA`.
    // So, the remaining characters are ALPHA, DIGIT, or `_`
    // Still, we check it either way to ensure sanity.
    CSTL_CHECK(isLetter(lexer_prev(lexer)) || isDigit(lexer_prev(lexer)),
               "This message means you've encountered a serious bug within Adorad. Please file an issue on "
               "Adorad's Github repo.\nError: `lexer_lex_identifier()` hasn't been called with a valid identifier character");

    UInt32 prev_offset = lexer->offset;
    UInt32 line = lexer->loc->line;
    UInt32 col = lexer->loc->col;
    int ident_length = 0;
    char ch = lexer_advance(lexer);

    while(isLetter(ch) || isDigit(ch)) {
        ch = lexer_advance(lexer);
        ++ident_length;
    }

    if(ident_length > MAX_TOKEN_LENGTH)
        CSTL_WARN(An identifier can never have more than 256 characters);

    UInt32 offset_diff = lexer->offset - prev_offset;
    Buff* ident_value = buff_slice(lexer->buffer, prev_offset - 1, offset_diff);
    CSTL_CHECK_NOT_NULL(ident_value, "`ident_value` must not be null");
    if(!ident_value)
        printf("Ident value = null\n");

    // Determine if a keyword or just a regular identifier
    TokenKind tokenkind = lexer_is_keyword_or_identifier(ident_value->data);
    lexer_maketoken(lexer, tokenkind, ident_value, prev_offset - 1, line, col - 1);

    LEXER_DECREMENT_OFFSET;
}

static inline void lexer_lex_digit(Lexer* lexer) {
    // 0x... --> Hexadecimal ("0x"|"0X")[0-9A-Fa-f_]+
    // 0o... --> Octal       ("0o"|"0O")[0-7_]+
    // 0b... --> Binary      ("0b"|"0B")[01_]+
    // This cannot be `lexer_advance(lexer)` because we enter here from `lexer_lex()` where we already
    // know that the first char is a digit value. 
    // This value needs to be captured as well in `token->value`
    char ch = lexer_prev(lexer);
    UInt32 prev_offset = lexer->offset - 1;
    UInt32 line = lexer->loc->line;
    UInt32 col = lexer->loc->col - 1;
    TokenKind tokenkind = TOK_ILLEGAL;
    int digit_length = 0; // no. of digits in the number

    CSTL_CHECK_TRUE(isDigit(ch));
    while(isDigit(ch)) {
        // Hex, Octal, or Binary?
        if(ch == '0') {
            ch = lexer_advance(lexer);
            switch(ch) {
                // Hex
                case 'x': case 'X':;
                    // Skip [xX]
                    ch = lexer_advance(lexer);
                    int hexcount = 0;
                    while(isHexDigit(ch)) {
                        ++hexcount; 
                        ch = lexer_advance(lexer);
                    }
                    if(hexcount == 0)
                        lexer_error(lexer, SyntaxError, "Expected hexadecimal digits [0-9A-Fa-f] after `0x`");
                    
                    tokenkind = HEX_INT;
                    digit_length = hexcount + 1; // Account for the '0'
                    break;
                // Binary
                case 'b': case 'B':
                    // Skip [bB]
                    ch = lexer_advance(lexer);
                    int bincount = 0;
                    while(isBinaryDigit(ch)) {
                        ++bincount; 
                        ch = lexer_advance(lexer);
                    }
                    if(bincount == 0)
                        lexer_error(lexer, SyntaxError, "Expected binary digit [0-1] after `0b`");
                    
                    tokenkind = BIN_INT;
                    digit_length = bincount + 1; // Account for the '0'
                    break;
                // Octal
                // Depart from the (error-prone) C-style octals with an inital zero e.g 0123
                // Instead, we support the `0o` or `0O` prefix, like 0o123
                case 'o': case 'O':
                    // Skip [oO]
                    ch = lexer_advance(lexer);
                    int octcount = 0;
                    while(isOctalDigit(ch)) {
                        ++octcount; 
                        ch = lexer_advance(lexer);
                    }
                    if(octcount == 0)
                        lexer_error(lexer, SyntaxError, "Expected octal digits [0-7] after `0o`");
                    
                    tokenkind = OCT_INT;
                    digit_length = octcount + 1; // Account for the '0'
                    break;
                case ALPHA_EXCEPT_B_O_X:
                    // Error
                    lexer_error(lexer, SyntaxError, "Invalid character `%c`. Adorad currently supports [xXbBoO] after `0`", ch);
                    break;
                default:
                    // An integer?
                    // lexer_error(lexer, SyntaxError, "Cannot have an integer beginning with `0`");     
                    break;               
            } // switch(ch)
        }
        // Fractions, or Integer?
        else {
            // Normal Floats
            if(ch == '.' || ch == '_')
                ch = lexer_advance(lexer);
            
            // Exponents (Float)
            else if(ch == 'e' || ch == 'E') {
                // Skip over [eE]
                ch = lexer_advance(lexer);
                if(ch == '+' || ch == '-') { 
                    ch = lexer_advance(lexer);
                } else {
                    lexer_error(lexer, SyntaxError, "Expected [+-] after exponent `e`. Got `%c`", ch);
                }
                
                int exp_digits = 0;
                while(isDigit(ch)) {
                    ch = lexer_advance(lexer);
                    ++exp_digits;
                }
                if(exp_digits == 0)
                    lexer_error(lexer, SyntaxError, "Invalid character after exponent `e`. Expected a digit, got `%c`", ch);
                
                // (TODO) Verify this is correct
                digit_length = exp_digits + 1; // Account for the prev digit
            }
            // Imaginary
            else if(ch == 'j' || ch == 'J') {
                // digit_length = imag_count;
            }
        }
        // ch = lexer_advance(lexer);
    }

    CSTL_CHECK(tokenkind != TOK_ILLEGAL);

    UInt32 offset_diff = lexer->offset - prev_offset;
    CSTL_CHECK_NE(offset_diff, 0);

    if(digit_length > MAX_TOKEN_LENGTH)
        CSTL_WARN(A number can never have more than 256 characters);

    // digit_length can be 0 and we can still have a number. Reasoning is as follows:
    // This function is guaranteed to be called when there's at least one "number-like". We simply check if
    // there are more digits to lex.
    // If digit_length = 0, this means that there's only one digit in the number (eg. 0, 2, 9)
    Buff* digit_value = buff_slice(lexer->buffer, prev_offset, offset_diff - 1);
    CSTL_CHECK_NOT_NULL(digit_value, "`digit_value` must not be null");
    if(!digit_value)
        printf("digit_value = null\n");
    lexer_maketoken(lexer, tokenkind, digit_value, prev_offset, line, col);

    LEXER_DECREMENT_OFFSET;
}

// Lex the Source files
static void lexer_lex(Lexer* lexer) {
    // Some UTF8 text may start with a 3-byte 'BOM' marker sequence. If it exists, skip over them because they 
    // are useless bytes. Generally, it is not recommended to add BOM markers to UTF8 texts, but it's not 
    // uncommon (especially on Windows).
    if(lexer->buffer->data[0] == (char)0xef && lexer->buffer->data[1] == (char)0xbb && lexer->buffer->data[2] == (char)0xbf)
        lexer_advancen(lexer, 3);

    char next = nullchar;
    char curr = nullchar;
    TokenKind tokenkind = TOK_ILLEGAL;

    while(true) {
        // `lexer_advance()` returns the current character and moves forward, and `lexer_peek()` returns the current
        // character (after the advance).
        // For example, if we start from buff[0], 
        //      curr = buff[0]
        //      next = buff[1]
        curr = lexer_advance(lexer);
        next = lexer_peek(lexer);
        tokenkind = TOK_ILLEGAL;

        switch(curr) {
            case nullchar: goto lex_eof;
            // The `-1` is there to prevent an ILLEGAL token kind from being appended to `lexer->toklist`
            // NB: Whitespace as a token is useless for our case (will this change later?)
            case WHITESPACE_NO_NEWLINE: tokenkind = -1; break;
            case '\n':
                LEXER_INCREMENT_LINENO;
                LEXER_RESET_COLNO;
                tokenkind = -1;
                break;
            // Identifier
            case ALPHA: case '_': tokenkind = -1; lexer_lex_identifier(lexer); break;
            case DIGIT: tokenkind = -1; lexer_lex_digit(lexer); break;
            case '"':
                switch(next) {
                    // Empty String literal 
                    case '"': LEXER_INCREMENT_OFFSET; lexer_maketoken(lexer, STRING, buff_new("\"\""), lexer->offset - 1, 
                                                                      lexer->loc->line, lexer->loc->col - 1); 
                              break;
                    default: tokenkind = -1; lexer_lex_string(lexer); break;
                }
                break;
            case ';':  tokenkind = SEMICOLON; break;
            case ',':  tokenkind = COMMA; break;
            case '\\': tokenkind = BACKSLASH; break;
            case '[':  tokenkind = LSQUAREBRACK; break;
            case ']':  tokenkind = RSQUAREBRACK; break;
            case '{':  lexer->nest_level++; tokenkind = LBRACE; break;
            case '}':  lexer->nest_level--; tokenkind = RBRACE; break;
            case '(':  tokenkind = LPAREN; break;
            case ')':  tokenkind = RPAREN; break;
            case '=':
                switch(next) {
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = EQUALS_EQUALS; break;
                    case '>': LEXER_INCREMENT_OFFSET; tokenkind = EQUALS_ARROW; break;
                    default: tokenkind = EQUALS; break;
                }
                break;
            case '+':
                switch(next) {
                    // This might be removed at some point. 
                    // '++' serves no purpose since Adorad doesn't (and won't) support pointer arithmetic.
                    case '+': LEXER_INCREMENT_OFFSET; tokenkind = PLUS_PLUS; break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind  = PLUS_EQUALS; break;
                    default: tokenkind = PLUS; break;
                }
                break;
            case '-':
                switch(next) {
                    // This might be removed at some point. 
                    // '--' serves no purpose since Adorad doesn't (and won't) support pointer arithmetic.
                    case '-': LEXER_INCREMENT_OFFSET; tokenkind = MINUS_MINUS; break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = MINUS_EQUALS; break;
                    case '>': LEXER_INCREMENT_OFFSET; tokenkind = RARROW; break;
                    default: tokenkind = MINUS; break;
                } 
                break;
            case '*':
                switch(next) {
                    case '*': LEXER_INCREMENT_OFFSET; tokenkind = MULT_MULT; break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = MULT_EQUALS; break;
                    default: tokenkind = MULT; break;
                }
                break;
            case '/':
                switch(next) {
                    // Add tokenkind here? 
                    // (TODO) jasmcaus
                    case '/': tokenkind = -1; lexer_lex_sl_comment(lexer); break;
                    case '*': tokenkind = -1; lexer_lex_ml_comment(lexer); break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = SLASH_EQUALS; break;
                    default: tokenkind = SLASH; break;
                }
                break;
            case '#': 
                // Ignore shebang on the first line
                if(lexer->loc->line == 1 && next == '!' && lexer_peekn(lexer, 1) == '/') {
                    tokenkind = -1;
                    // Skip till end of line
                    while(LEXER_CURR_CHAR && (LEXER_CURR_CHAR != '\n' || LEXER_CURR_CHAR != nullchar))
                        curr = lexer_advance(lexer);
                }
                // Comment
                else {
                    tokenkind = -1;
                    lexer_lex_sl_comment(lexer);
                }
                break;
            case '!':
                switch(next) {
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = EXCLAMATION_EQUALS; break;
                    default: tokenkind = MINUS_MINUS; break;
                }
                break;
            case '%':
                switch(next) {
                    case '%': LEXER_INCREMENT_OFFSET; tokenkind = MOD_MOD; break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = MOD_EQUALS; break;
                    default: tokenkind = MOD; break;
                }
                break;
            case '&':
                switch(next) {
                    case '&': LEXER_INCREMENT_OFFSET; tokenkind = AND_AND; break;
                    case '^': LEXER_INCREMENT_OFFSET; tokenkind = AND_NOT; break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = AND_EQUALS; break;
                    default: tokenkind = AND; break;
                }
                break;
            case '|':
                switch(next) {
                    case '|': LEXER_INCREMENT_OFFSET; tokenkind = OR_OR; break;
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = OR_EQUALS; break;
                    default: tokenkind = OR; break;
                }
                break;
            case '^':
                switch(next) {
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = XOR_EQUALS; break;
                    default: tokenkind = XOR; break;
                }
                break;
            case '<':
                switch(next) {
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = LESS_THAN_OR_EQUAL_TO; break;
                    case '-': LEXER_INCREMENT_OFFSET; tokenkind = LARROW; break;
                    case '<': 
                        LEXER_INCREMENT_OFFSET;
                        char c = lexer_peek(lexer);
                        if(c == '=') {
                            LEXER_INCREMENT_OFFSET; tokenkind = LBITSHIFT_EQUALS;
                        } else {
                            tokenkind = LBITSHIFT;
                        }
                        break;
                    default: tokenkind = LESS_THAN; break;
                }
                break;
            case '>':
                switch(next) {
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = GREATER_THAN_OR_EQUAL_TO; break;
                    case '>': 
                        LEXER_INCREMENT_OFFSET;
                        char c = lexer_peek(lexer);
                        if(c == '=') {
                            LEXER_INCREMENT_OFFSET; tokenkind = RBITSHIFT_EQUALS;
                        } else {
                            tokenkind = RBITSHIFT;
                        }
                        break;
                    default: tokenkind = GREATER_THAN; break;
                }
                break;
            case '~':
                switch(next) {
                    case '=': LEXER_INCREMENT_OFFSET; tokenkind = TILDA_EQUALS; break;
                    default: tokenkind = TILDA; break;
                }
                break;
            case '.':
                switch(next) {
                    case '.': 
                        LEXER_INCREMENT_OFFSET;
                        char c = lexer_peek(lexer);
                        if(c == '.') {
                            LEXER_INCREMENT_OFFSET; tokenkind = ELLIPSIS;
                        } else {
                            tokenkind = DDOT;
                        }
                        break;
                    // Fractions are possible here:
                    // Eg: `.0192` or `.9983838`
                    case DIGIT: tokenkind = -1; lexer_lex_digit(lexer); break;
                    default: tokenkind = DOT; break;
                }
                break;
            case ':':
                switch(next) {
                    case ':': LEXER_INCREMENT_OFFSET; tokenkind = COLON_COLON; break;
                    default: tokenkind = COLON; break;
                }
                break;
            case '?': tokenkind = QUESTION; break;
            case '@': tokenkind = -1; lexer_lex_macro(lexer); break;
            default:
                lexer_error(lexer, SyntaxError, "Invalid character `%c`", curr);
                break;
        } // switch(ch)

        if(tokenkind == -1) continue;
        lexer_maketoken(lexer, tokenkind, buff_new(null), lexer->offset - 1, lexer->loc->line, lexer->loc->col - 1);
    } // while

lex_eof:;

    lexer_maketoken(lexer, TOK_EOF, buff_new(null), lexer->offset - 1, lexer->loc->line, lexer->loc->col - 1);
}