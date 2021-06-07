/*
_ _    _           ______   _______        
| |  | |    /\    /___  /   |  ____|| |    
| |__| |   /  \      / /    | |__   | |       Hazel - The Fast, Expressive * Elegant Programming Language
|  __  |  / /\ \    / /     |  __|  | |       Languages: C, C++, and Assembly
| |  | | / ____ \  / /___   | |____ | |____   https://github.com/HazelLang/hazel/
|_|_ |_|/_/    \_\/_______\ |______|_\______|

Licensed under the MIT License <http://opensource.org/licenses/MIT>
SPDX-License-Identifier: MIT
Copyright (c) 2021 Jason Dsouza <http://github.com/jasmcaus>
*/

#include <hazel/compiler/lexer/lexer.h>

// These macros are used in the switch() statements below during the Lexing of Hazel source files.
#define WHITESPACE \
    ' ': case '\r': case '\t': case '\n'

#define DIGIT_NO_ZERO  \
    '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9'

#define DIGIT \
    '0': case DIGIT

#define HEX_AND_DIGIT \
    'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
    case DIGIT

#define ALPHA_EXCEPT_HEX \
         'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': \
    case 'r': case 's': case 't': case 'u': case 'v': case 'w': case 'x': case 'y': case 'z': case 'G': case 'H': \
    case 'I': case 'J': case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': \
    case 'T': case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z'


Lexer* lexer_init(const char* buffer) {
    Lexer* lexer = calloc(1, sizeof(Lexer));
    lexer->buffer = buffer; 
    lexer->buffer_capacity = strlen(buffer);
    lexer->offset = 0;
    lexer->lineno = 1;
    lexer->colno = 1;
    lexer->fname = "";
    lexer->is_inside_str = false;
    return lexer;
}

static void lexer_print_stats(Lexer* lexer) {
    printf("    Char: %c\n", lexer->buffer[lexer->offset]);
    printf("    Offset: %d\n", lexer->offset);
    printf("    Lineno: %d\n", lexer->lineno);
    printf("    Colno: %d\n", lexer->colno);
}

// Returns the curent character in the Lexical Buffer and advances to the next element
// It does this by incrementing the buffer offset.
static inline char lexer_advance(Lexer* lexer) {
    if(lexer->offset >= lexer->buffer_capacity) {
        return nullchar; 
    } else {
        LEXER_INCREMENT_COLNO;
        return (char)lexer->buffer[lexer->offset++];
    }
}

// Advance `n` characters in the Lexical Buffer
static inline char lexer_advance_n(Lexer* lexer, UInt32 n) {
    // The '>=' is here because offset and buffer_capacity are off by 1 (0-index vs 1-index respectively)
    if(lexer->offset + n >= lexer->buffer_capacity) {
        return nullchar;
    } else {
        lexer->colno += n;
        lexer->offset += n;
        return (char)lexer->buffer[lexer->offset];
    }
}

// Returns the previous `n` elements in the Lexical buffer.
// This is non-destructive -- the buffer offset is not updated.
static inline char lexer_prev(Lexer* lexer, UInt32 n) {
    if(lexer->offset == 0) {
        return nullchar;
    } else {
        return (char)lexer->buffer[lexer->offset-1];
    }
}

// Returns the current element in the Lexical Buffer.
static inline char lexer_peek(Lexer* lexer) {
    return (char)lexer->buffer[lexer->offset];
}

// "Look ahead" `n` characters in the Lexical buffer.
// It _does not_ increment the buffer offset.
static inline char lexer_peek_n(Lexer* lexer, UInt32 n) {
    if(lexer->offset + n >= lexer->buffer_capacity) {
        return nullchar;
    } else {
        return (char)lexer->buffer[lexer->offset+n];
    }
}

#define MAX_IDENTIFIER_SIZE     250
// Scan an identifier
inline char* lex_identifier(Lexer* lexer) {
    char ident[MAX_IDENTIFIER_SIZE];
    char ch = lexer->buffer[lexer->offset];
    int prev_offset = lexer->offset;

    while(isLetter(ch) || isDigit(ch)) {
        ch = lexer_advance(lexer);
    }

    // Remember to null-terminate!
    strncpy(ident, lexer->buffer, lexer->offset - prev_offset);
    return ident;
}

inline char lexer_number(Lexer* lexer) {
    char num[MAX_IDENTIFIER_SIZE];
    char ch = lexer->buffer[lexer->offset];
    int prev_offset = lexer->offset;

    while(isDigit(ch)) {
        ch = lexer_advance(lexer);
    }

    // Remember to null-terminate!
    strncpy(num, lexer->buffer, lexer->offset - prev_offset);
    return num;
}