# Generates tokens which are defined in hazel/grammar/Tokens
# The files are (relative to the root) are:
#   1. hazel/compiler/tokens/token.h
#   2. hazel/compiler/tokens/token.c

NT_OFFSET = 256 

def load_tokens(path):
    token_names = [] 
    string_to_token = {}
    ERRORTOKEN = None 

    with open(path) as fp:
        for line in fp:
            line = line.strip()
            # Strip comments 
            i = line.find('#')

            if i>=0:
                line = line[:i].strip()
            if not line:
                continue 

            fields = line.split()
            name = fields[0]
            value = len(token_names)

            if name == 'ERRORTOKEN':
                ERRORTOKEN = value 
            
            string = fields[1] if len(fields) > 1 else None 
            if string:
                string = eval(string)
                string_to_token[string] = value 
            
            token_names.append(name)
    
    return token_names, ERRORTOKEN, string_to_token 


def update_file(file, content):
    try:
        with open(file, 'r') as fobj:
            if fobj.read() == content:
                return False 
            
    except (OSError, ValueError):
        pass 

    with open(file, 'w') as fobj:
        fobj.write(content)
    return True 


token_h_template = """\
// Auto-generated by tools/scripts/generate_tokens.py
// NOT for inclusion in any program

#if 0
// Token Types 
#ifndef HAZEL_TOKEN_H
#define HAZEL_TOKEN_H

#ifdef __cplusplus
extern "C" {
#endif

%s\
#define N_TOKENS        %d
#define NT_OFFSET       %d

////////////////////////////////// SPECIAL DEFINITIONS FOR COOP WITH THE PARSER //////////////////////////////////
#define ISTERMINAL (x)        ((x) < NT_OFFSET)
#define ISNONTERMINAL (x)     ((x) >= NT_OFFSET)
#define ISEOF(x)              ((x) == ENDMARKER)
#define ISWHITESPACE(x)       ((x) == ENDMARKER || \\
                               (x) == NEWLINE   || \\
                               (x) == INDENT    || \\
                               (x) == DEDENT)

const char* const TokenNames[]; 
int Token_OneChar(int);
int Token_TwoChars(int, int);
int Token_ThreeChars(int, int, int);

TokenNames* token_init(int type, char* value);

#ifdef __cplusplus
}
#endif

#endif 0 // if 0
#endif // HAZEL_TOKEN_H
"""


def make_token_header(infile, outfile='hazel/compiler/tokens/token.h'):
    token_names, ERRORTOKEN, _ = load_tokens(infile)

    defines = [] 
    for value, name in enumerate(token_names[:ERRORTOKEN + 1]):
        defines.append("#define %-15s %d\n" % (name, value))
    
    if update_file(outfile, token_h_template % (
            ''.join(defines),
            len(token_names),
            NT_OFFSET
        )):
        print("%s regenerated from %s" % (outfile, infile))



token_c_template = """\
// Auto-generated by tools/scripts/generate_tokens.py
// NOT for inclusion in any program


#if 0
#include "hazel/compiler/tokens/tokens.h"

// Token Names
const char* const TokenNames = {
%s\
};

// Return the token corresponding to a single character
int Token_OneChar(int) {
%s\
    return OP;
}
int
Token_TwoChars(int c1, int c2)
{
%s\
    return OP;
}
int
Token_ThreeChars(int c1, int c2, int c3)
{
%s\
    return OP;
}
#endif // if 0
"""

def generate_chars_to_token(mapping, n=1):
    result = []
    write = result.append
    indent = '    ' * n
    write(indent)
    write('switch (c%d) {\n' % (n,))

    for c in sorted(mapping):
        write(indent)
        value = mapping[c]
        if isinstance(value, dict):
            write("case '%s':\n" % (c,))
            write(generate_chars_to_token(value, n + 1))
            write(indent)
            write('    break;\n')
        else:
            write("case '%s': return %s;\n" % (c, value))

    write(indent)
    write('}\n')
    return ''.join(result)


def make_token_c(infile, outfile='hazel/compiler/tokens/token.c'):
    tok_names, ERRORTOKEN, string_to_tok = load_tokens(infile)
    string_to_tok['<>'] = string_to_tok['!=']
    chars_to_token = {}

    for string, value in string_to_tok.items():
        assert 1 <= len(string) <= 3
        name = tok_names[value]
        m = chars_to_token.setdefault(len(string), {})
        for c in string[:-1]:
            m = m.setdefault(c, {})
        m[string[-1]] = name

    names = []
    for value, name in enumerate(tok_names):
        if value >= ERRORTOKEN:
            name = '<%s>' % name
        names.append('    "%s",\n' % name)
    names.append('    "<N_TOKENS>",\n')

    if update_file(outfile, token_c_template % (
            ''.join(names),
            generate_chars_to_token(chars_to_token[1]),
            generate_chars_to_token(chars_to_token[2]),
            generate_chars_to_token(chars_to_token[3])
        )):
        print("%s regenerated from %s" % (outfile, infile))


token_py_template = '''\
"""Token constants."""
# Auto-generated by Tools/scripts/generate_token.py

__all__ = ['tok_name', 'ISTERMINAL', 'ISNONTERMINAL', 'ISEOF']

%s
N_TOKENS = %d
# Special definitions for cooperation with parser
NT_OFFSET = %d
tok_name = {value: name
            for name, value in globals().items()
            if isinstance(value, int) and not name.startswith('_')}
__all__.extend(tok_name.values())

EXACT_TOKEN_TYPES = {
%s
}


def ISTERMINAL(x):
    return x < NT_OFFSET
def ISNONTERMINAL(x):
    return x >= NT_OFFSET
def ISEOF(x):
    return x == ENDMARKER
'''

def make_token_py(infile, outfile='hazel/compiler/tokens/tokens.py'):
    tok_names, ERRORTOKEN, string_to_tok = load_tokens(infile)

    constants = []
    for value, name in enumerate(tok_names):
        constants.append('%s = %d' % (name, value))
    constants.insert(ERRORTOKEN,
        "# These aren't used by the C tokenizer but are needed for tokenize.py")

    token_types = []
    for s, value in sorted(string_to_tok.items()):
        token_types.append('    %r: %s,' % (s, tok_names[value]))

    if update_file(outfile, token_py_template % (
            '\n'.join(constants),
            len(tok_names),
            NT_OFFSET,
            '\n'.join(token_types),
        )):
        print("%s regenerated from %s" % (outfile, infile))


def mainfunc(op, infile='hazel/compiler/tokens', *args):
    make = globals()['make_' + op]
    make(infile, *args)

#pylint:disable=no-value-for-parameter
if __name__ == '__main__':
    import sys
    mainfunc(*sys.argv[1:])