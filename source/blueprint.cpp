#include "bricks.h"
#include "platform.h"


extern ApplicationState App;

struct SourceLocation {
    u8 *ptr;
    s32 line;
    s32 column;
};

enum {
    TOKEN_END_OF_INPUT,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_INTEGER,

    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_EQUAL,
    TOKEN_COLON,
    TOKEN_DOUBLE_COLON,
    TOKEN_SEMICOLON,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_ASTERISK,
    TOKEN_SLASH,
    TOKEN_AT,
    TOKEN_HASHTAG,

    TOKEN_LEFT_PARENTHESIS,
    TOKEN_RIGHT_PARENTHESIS,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,

    TOKEN_KEYWORD_EXECUTABLE,
    TOKEN_KEYWORD_BRICK,
    TOKEN_KEYWORD_LIBRARY,
    TOKEN_KEYWORD_BLUEPRINT,

    TOKEN_UNKNOWN,

    TOKEN_COUNT
};
struct Token {
    u32 kind;
    SourceLocation loc;

    String content;
};

struct Parser {
    String source_code;
    String current_file;

    SourceLocation loc;

    Token previous_token;
    Token current_token;

    bool has_peek;
    Token peek_token;
};

enum {
    CHAR_CHARACTER,
    CHAR_DIGIT,
    CHAR_WHITESPACE,
    CHAR_CONTROL,
    CHAR_UNUSED,
};

#define A CHAR_CHARACTER
#define B CHAR_DIGIT
#define C CHAR_WHITESPACE
#define D CHAR_CONTROL
#define E CHAR_UNUSED

INTERNAL u8 Lookup[] = {
    E, E, E, E, E, E, E, E, E, C, C, C, E, C, E, E,
    E, E, E, E, E, E, E, E, E, E, E, E, E, E, E, E,
    C, D, D, D, D, D, D, D, D, D, D, D, D, D, D, D,
    B, B, B, B, B, B, B, B, B, B, D, D, D, D, D, D,
    D, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A,
    A, A, A, A, A, A, A, A, A, A, A, D, D, D, D, D,
    D, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A,
    A, A, A, A, A, A, A, A, A, A, A, D, D, D, D, E,

    // TODO: unicode characters and forbidden values
};

#undef A
#undef B
#undef C
#undef D
#undef E



INTERNAL void advance_source(Parser *parser) {
    assert(parser->source_code.size);

    parser->source_code.data += 1;
    parser->source_code.size -= 1;

    parser->loc.ptr = parser->source_code.data;
    parser->loc.column += 1;
}

INTERNAL bool peek_char(Parser *parser, u8 *c) {
    if (parser->source_code.size == 0) return false;

    *c = parser->source_code.data[0];

    return true;
}

INTERNAL bool get_char(Parser *parser, u8 *c) {
    if (parser->source_code.size == 0) return false;

    *c = parser->source_code.data[0];
    advance_source(parser);

    return true;
}

INTERNAL bool match_char(Parser *parser, u8 c) {
    if (parser->source_code.size && parser->source_code[0] == c) {
        advance_source(parser);
        return true;
    }

    return false;
}

INTERNAL bool match_char_type(Parser *parser, u32 type) {
    if (parser->source_code.size && Lookup[parser->source_code[0]] == type) {
        advance_source(parser);
        return true;
    }

    return false;
}

INTERNAL String find_line(SourceLocation loc, String source) {
    u8 *line_begin = loc.ptr - (loc.column - 1);
    s32 length = loc.column;

    s32 source_left = source.size - (loc.ptr - source.data);
    for (; source_left && line_begin[length] != '\n' && line_begin[length] != '\r'; length += 1, source_left -= 1);

    return String(line_begin, length);
}

INTERNAL void parse_error(Parser *parser, SourceLocation loc, String message) {
    String line = find_line(loc, parser->source_code);
    print("Error in blueprint file %S: %d:%d: %S\n", parser->current_file, loc.line, loc.column, message);
    print("%S\n", line);

    // TODO: proper tab handling
    for (s32 i = 1; i < loc.column; i += 1) {
        print(" ");
    }
    print("^\n\n");
}

INTERNAL Token parse_identifier(Parser *parser) {
    u8 *mark = parser->source_code.data;
    s64 size = 0;

    SourceLocation location = parser->loc;

    u8 c;
    while (peek_char(parser, &c)) {
        s32 type = Lookup[c];
        if (type == CHAR_CHARACTER || type == CHAR_DIGIT || c == '_') {
            size += 1;
            advance_source(parser);
        } else {
            break;
        }
    }

    Token token = {
        TOKEN_IDENTIFIER,
        location,
        {mark, size}
    };

    if      (equal(token.content, "Executable")) token.kind = TOKEN_KEYWORD_EXECUTABLE;
    else if (equal(token.content, "Brick"))      token.kind = TOKEN_KEYWORD_BRICK;
    else if (equal(token.content, "Library"))    token.kind = TOKEN_KEYWORD_LIBRARY;
    else if (equal(token.content, "Blueprint"))  token.kind = TOKEN_KEYWORD_BLUEPRINT;

    return token;
}

INTERNAL Token parse_number(Parser *parser) {
    u8 *mark = parser->source_code.data;
    s64 size = 0;

    SourceLocation location = parser->loc;

    u8 c;
    while (peek_char(parser, &c)) {
        s32 type = Lookup[c];
        if (type == CHAR_DIGIT) {
            size += 1;
            advance_source(parser);
        } else {
            break;
        }
    }

    Token token = {
        TOKEN_INTEGER,
        location,
        {mark, size}
    };

    return token;
}


// TODO: this is not right in cases of missing "
INTERNAL Token parse_string(Parser *parser) {
    SourceLocation location = parser->loc;

    u8 *mark = parser->source_code.data;
    s64 size = 0;

    u8 c;
    do {
        if (!get_char(parser, &c)) {
            parse_error(parser, location, "Missing \" in string.");
            break;
        }

        size += 1;
    } while (c != '"');

    size -= 1;

    Token token = {
        TOKEN_STRING,
        location,
        {mark, size}
    };

    return token;
}

INTERNAL void skip_comment(Parser *parser) {
    u8 c;
    while (peek_char(parser, &c)) {
        if (c == '\n' || c == '\r') return;

        advance_source(parser);
    }
}

INTERNAL Token next_token(Parser *parser);
INTERNAL Token parse_control(Parser *parser) {
    Token token = {};
    token.loc = parser->loc;

    u8 c;
    get_char(parser, &c);

    token.content = String(parser->source_code.data - 1, 1);

    switch (c) {
    case '.': { token.kind = TOKEN_DOT;   } break;
    case ',': { token.kind = TOKEN_COMMA; } break;
    case '=': { token.kind = TOKEN_EQUAL; } break;
    case ':': {
        if (match_char(parser, ':')) {
            token.kind = TOKEN_DOUBLE_COLON;
            token.content.size += 1;
            break;
        }
        token.kind = TOKEN_COLON;
    } break;
    case '"': { token = parse_string(parser); } break;

    case ';': { token.kind = TOKEN_SEMICOLON; } break;
    case '*': { token.kind = TOKEN_ASTERISK;  } break;
    case '/': {
        if (match_char(parser, '/')) {
            skip_comment(parser);
            return next_token(parser);
            break;
        }
        token.kind = TOKEN_SLASH;
    } break;
    case '+': { token.kind = TOKEN_PLUS;  } break;
    case '-': { token.kind = TOKEN_MINUS; } break;
    case '@': { token.kind = TOKEN_AT; } break;
    case '#': { token.kind = TOKEN_HASHTAG; } break;

    case '(': { token.kind = TOKEN_LEFT_PARENTHESIS; } break;
    case ')': { token.kind = TOKEN_RIGHT_PARENTHESIS; } break;
    case '{': { token.kind = TOKEN_LEFT_BRACE; } break;
    case '}': { token.kind = TOKEN_RIGHT_BRACE; } break;
    case '[': { token.kind = TOKEN_LEFT_BRACKET; } break;
    case ']': { token.kind = TOKEN_RIGHT_BRACKET; } break;

    default:
              parse_error(parser, token.loc, "Unsupported character.");
    }

    return token;
}

INTERNAL void skip_whitespaces(Parser *parser) {
    s32 const multi_new_line = '\n' + '\r';

    u8 c;
    while (peek_char(parser, &c) && Lookup[c] == CHAR_WHITESPACE) {
        if (c == '\n' || c == '\r') {
            advance_source(parser);

            u8 c2;
            if (peek_char(parser, &c2) && (c + c2) == multi_new_line) {
                advance_source(parser);
            }

            parser->loc.line += 1;
            parser->loc.column = 1;

            continue;
        }

        advance_source(parser);
    }
}


INTERNAL Token next_token(Parser *parser) {
    while (parser->source_code.size) {
        switch (Lookup[parser->source_code.data[0]]) {
        case CHAR_CHARACTER: {
            return parse_identifier(parser);
        } break;

        case CHAR_DIGIT: {
            return parse_number(parser);
        } break;

        case CHAR_WHITESPACE: {
            skip_whitespaces(parser);
            continue;
        } break;

        case CHAR_CONTROL: {
            return parse_control(parser);
        } break;

        case CHAR_UNUSED: {
            Token token;
            token.kind = TOKEN_UNKNOWN;
            token.content = String(parser->source_code.data - 1, 1);

            return token;
        } break;
        }
    }

    Token token = {0};
    token.kind = TOKEN_END_OF_INPUT;
    token.content.data = parser->source_code.data - 1;
    token.content.size = 0;
    token.loc = parser->loc;
    return token;
}

INTERNAL Token peek_token(Parser *parser) {
    parser->peek_token = next_token(parser);
    parser->has_peek = true;

    return parser->peek_token;
}

INTERNAL void advance_token(Parser *parser) {
    parser->previous_token = parser->current_token;
    if (parser->has_peek) {
        parser->current_token = parser->peek_token;
        parser->has_peek = false;
    } else {
        parser->current_token  = next_token(parser);
    }
}

Parser init_parser(String source, String filename) {
    Parser parser = {0};

    parser.source_code = source;
    assert(parser.source_code.size != 0);

    parser.current_file = filename;

    parser.loc.ptr = source.data;
    parser.loc.line = 1;
    parser.loc.column = 1;

    advance_token(&parser);

    return parser;
}

INTERNAL bool current_token_is(Parser *parser, u32 kind) {
    return parser->current_token.kind == kind;
}

INTERNAL bool consume(Parser *parser, u32 kind, String error_msg) {
    if (current_token_is(parser, kind)) {
        advance_token(parser);
        return true;
    } else {
        parse_error(parser, parser->current_token.loc, error_msg);
    }

    return false;
}

INTERNAL bool match(Parser *parser, u32 kind) {
    if (current_token_is(parser, kind)) {
        advance_token(parser);
        return true;
    }
    return false;
}



INTERNAL Blueprint default_config() {
    Blueprint blueprint = {};

#ifdef OS_WINDOWS
    blueprint.compiler = "msvc";
    blueprint.linker   = "msvc";
#endif

    blueprint.build_dir = "build";

    return blueprint;
}


void free_sheet(Sheet *sheet);

INTERNAL bool parse_declaration(Parser *parser, Blueprint *blueprint) {
    if (!consume(parser, TOKEN_IDENTIFIER, "Missing field name.")) return false;
    Token field = parser->previous_token;

    if (!consume(parser, TOKEN_COLON, "Missing : in field declaration.")) return false;

    if (equal(field.content, "Compiler")) {
        if (!consume(parser, TOKEN_STRING, "Expected string in Compiler field.")) return false;
        blueprint->compiler = parser->previous_token.content;
    } else if (equal(field.content, "Linker")) {
        if (!consume(parser, TOKEN_STRING, "Expected string in Linker field.")) return false;
        blueprint->linker = parser->previous_token.content;
    } else if (equal(field.content, "BuildDir")) {
        if (!consume(parser, TOKEN_STRING, "Expected string in BuildDir field.")) return false;
        blueprint->build_dir = parser->previous_token.content;
    } else {
        parse_error(parser, field.loc, "Unknown identifier.");
        return false;
    }

    if (!consume(parser, TOKEN_SEMICOLON, "Missing ; after declaration")) return false;

    return true;
}

INTERNAL PlatformKind target_platform(String platform_string) {
    if (platform_string == "win32") return TARGET_WIN32;

    return TARGET_UNDEFINED;
}

INTERNAL bool parse_path_list(Parser *parser, String name, DArray<String> *value) {
    String base_dir = path_without_filename(parser->current_file);
    String sub_dir = {};

    StringBuilder builder = {};

    do {
        reset(&builder);

        if (match(parser, TOKEN_STRING)) {
            if (base_dir.size) append_format(&builder, "%S/", base_dir);
            if (sub_dir.size)  append_format(&builder, "%S/", sub_dir);

            append(&builder, parser->previous_token.content);
            append(*value, to_allocated_string(&builder, App.string_alloc));
        } else if (match(parser, TOKEN_SLASH)) {
            if (!consume(parser, TOKEN_STRING, "Expected string after directory specifier (/).")) return false;
            sub_dir = parser->previous_token.content;
        } else if (match(parser, TOKEN_AT)) {
            if (App.target_platform != target_platform(parser->current_token.content)) {
                u32 kind;
                do {
                    advance_token(parser);
                    kind = peek_token(parser).kind;
                } while (kind != TOKEN_AT && kind != TOKEN_SEMICOLON && kind != TOKEN_END_OF_INPUT);
            } else {
                advance_token(parser);

                continue;
            }
        } else {
            parse_error(parser, parser->current_token.loc, format(App.string_alloc, "Expected string or / specifier in %S field.", name));
            return false;
        }
    } while (match(parser, TOKEN_COMMA));

    return true;
}

INTERNAL bool parse_string_list(Parser *parser, String name, DArray<String> *value) {
    do {
        if (!consume(parser, TOKEN_STRING, format(App.string_alloc, "Expected string in %S field.", name))) return false;

        append(*value, parser->previous_token.content);
    } while (match(parser, TOKEN_COMMA));

    return true;
}

INTERNAL bool parse_string_field(Parser *parser, String name, String *value) {
    if (!consume(parser, TOKEN_STRING, format(App.string_alloc, "Expected string in field %S.", name))) return false;
    *value = parser->previous_token.content;

    return true;
}

INTERNAL bool parse_dependency_list(Parser *parser, String name, DArray<Dependency> *deps, DArray<String> *libs) {
    do {
        Dependency dep = {};

        if (match(parser, TOKEN_STRING)) {
            append(*libs, parser->previous_token.content);

            continue;
        } else if (match(parser, TOKEN_IDENTIFIER)) {
            dep.sheet = parser->previous_token.content;

            if (match(parser, TOKEN_DOT)) {
                if (!consume(parser, TOKEN_IDENTIFIER, "Expected identifier as element specifier.")) return false;
                dep.module = dep.sheet;
                dep.sheet = parser->previous_token.content;
            }
        } else if (match(parser, TOKEN_AT)) {
            if (App.target_platform != target_platform(parser->current_token.content)) {
                u32 kind;
                do {
                    advance_token(parser);
                    kind = peek_token(parser).kind;
                } while (kind != TOKEN_AT && kind != TOKEN_SEMICOLON && kind != TOKEN_END_OF_INPUT);
            } else {
                advance_token(parser);
            }

            continue;
        } else {
            parse_error(parser, parser->current_token.loc, format(App.string_alloc, "Expected either a string, identifier or @ specifier in dependency list %S.", name));
            return false;
        }

        append(*deps, dep);
    } while (match(parser, TOKEN_COMMA));

    return true;
}

INTERNAL bool parse_sheet_field(Parser *parser, Sheet *sheet) {
    if (!consume(parser, TOKEN_IDENTIFIER, "Missing field name.")) return false;
    Token field = parser->previous_token;

    if (match(parser, TOKEN_HASHTAG)) {
        if (!consume(parser, TOKEN_STRING, "Here needs to be a string nameing the build type.")) return false;

        if (parser->previous_token.content != App.build_type) {
            while (parser->current_token.kind != TOKEN_SEMICOLON) {
                advance_token(parser);
            }

            return true;
        }
    }

    if (!consume(parser, TOKEN_COLON, "Missing : in field.")) return false;

    if (sheet->kind == SHEET_BRICK) {
        if      (field.content == "sources")      return parse_path_list(parser, "sources", &sheet->sources);
        else if (field.content == "include_dirs") return parse_path_list(parser, "include_dirs", &sheet->include_dirs);
        else if (field.content == "symbols")      return parse_string_list(parser, "symbols", &sheet->symbols);
        else if (field.content == "dependencies") return parse_dependency_list(parser, "dependencies", &sheet->dependencies, &sheet->libraries);
        else parse_error(parser, field.loc, format(App.string_alloc, "Unknown field %S.", field.content));
    } else if (sheet->kind == SHEET_EXECUTABLE || sheet->kind == SHEET_LIBRARY) {
        if      (field.content == "sources")      return parse_path_list(parser, "sources", &sheet->sources);
        else if (field.content == "include_dirs") return parse_path_list(parser, "include_dirs", &sheet->include_dirs);
        else if (field.content == "symbols")      return parse_string_list(parser, "symbols", &sheet->symbols);
        else if (field.content == "dependencies") return parse_dependency_list(parser, "dependencies", &sheet->dependencies, &sheet->libraries);
        else if (field.content == "compiler")     return parse_string_field(parser, "compiler", &sheet->compiler);
        else if (field.content == "group")        return parse_string_field(parser, "group", &sheet->group);
        else if (field.content == "build_dir")    return parse_string_field(parser, "build_dir", &sheet->build_dir);
        else parse_error(parser, field.loc, format(App.string_alloc, "Unknown field %S.", field.content));
    }

    return false;
}

INTERNAL String SheetKindLookup[SHEET_COUNT] = {
    "<Errornous sheet kind>",
    "Blueprint",
    "Brick",
    "Executable",
    "Library",
};

INTERNAL String enum_string(SheetKind kind) {
    return SheetKindLookup[kind];
}

INTERNAL bool parse_sheet_declaration(Parser *parser, Blueprint *blueprint, SheetKind kind) {
    advance_token(parser);

    Sheet sheet = {};
    sheet.kind = kind;
    sheet.compiler  = blueprint->compiler;
    sheet.linker    = blueprint->linker;
    sheet.build_dir = blueprint->build_dir;

    if (!consume(parser, TOKEN_COLON, t_format("Missing : after %S.", enum_string(sheet.kind)))) return false;
    if (!consume(parser, TOKEN_IDENTIFIER, t_format("Missing %S name.", enum_string(sheet.kind)))) return false;
    sheet.name = parser->previous_token.content;

    if (match(parser, TOKEN_LEFT_BRACE)) {
        if (match(parser, TOKEN_RIGHT_BRACE)) {
            parse_error(parser, parser->previous_token.loc, t_format("Empty %S not allowed.", enum_string(sheet.kind)));
            free_sheet(&sheet);

            return false;
        }
        do {
            if (current_token_is(parser, TOKEN_RIGHT_BRACE)) break;
            if (!parse_sheet_field(parser, &sheet)) {
                free_sheet(&sheet);

                return false;
            }
        } while (match(parser, TOKEN_SEMICOLON));

        if (!consume(parser, TOKEN_RIGHT_BRACE, "Missing } in Executable declaration.")) {
            free_sheet(&sheet);

            return false;
        }
    } else {
        parse_error(parser, parser->current_token.loc, "Missing { in Executable declaration");
        free_sheet(&sheet);

        return false;
    }

    String extension;
    if (kind == SHEET_EXECUTABLE) {
        extension = App.target_info.exe;
        sheet.full_path = format(App.string_alloc, "%S/%S/%S%S", blueprint->path, sheet.build_dir, sheet.name, extension);
    } else if (kind == SHEET_LIBRARY) {
        if (sheet.additional_info == SHEET_STATIC) {
            extension = App.target_info.static_lib;
        } else if (sheet.additional_info == SHEET_SHARED) {
            extension = App.target_info.shared_lib;
        } else {
            die("Should not ne reachable.\n");
        }
        String file_with_extension = t_format("%S%S", sheet.name, extension);
        sheet.full_path = format(App.string_alloc, "%S/%S/%S", App.build_files_directory, file_with_extension, file_with_extension);
    }


    append(blueprint->sheets, sheet);

    return true;
}

Blueprint parse_blueprint_file(String file);

INTERNAL bool parse_blueprint_declaration(Parser *parser, Blueprint *blueprint) {
    advance_token(parser);

    if (!consume(parser, TOKEN_COLON, "Missing : in Blueprint declaration.")) return false;
    if (!consume(parser, TOKEN_IDENTIFIER, "Blueprint declaration needs a name.")) return false;

    String name = parser->previous_token.content;

    String file = t_format("%S/%S/blueprint", App.brickyard_directory, parser->previous_token.content);
    Blueprint bp = parse_blueprint_file(file);
    bp.name = name;

    if (!consume(parser, TOKEN_SEMICOLON, "Missing ; in Bricks declaration.")) return false;
    if (bp.valid) {
        append(blueprint->blueprints, bp);

        return true;
    }

    return false;
}

INTERNAL bool parse_statement(Parser *parser, Blueprint *blueprint) {
    switch (parser->current_token.kind) {
    case TOKEN_IDENTIFIER: {
        return parse_declaration(parser, blueprint);
    } break;

    case TOKEN_KEYWORD_EXECUTABLE: {
        return parse_sheet_declaration(parser, blueprint, SHEET_EXECUTABLE);
    } break;

    case TOKEN_KEYWORD_BRICK: {
        return parse_sheet_declaration(parser, blueprint, SHEET_BRICK);
    } break;

    case TOKEN_KEYWORD_LIBRARY: {
        return parse_sheet_declaration(parser, blueprint, SHEET_LIBRARY);
    } break;

    case TOKEN_KEYWORD_BLUEPRINT: {
        return parse_blueprint_declaration(parser, blueprint);
    } break;
    }

    parse_error(parser, parser->current_token.loc, "Expected statement.");

    return false;
}

INTERNAL Blueprint parse_blueprint_file(String file) {
    Blueprint blueprint = default_config();

    blueprint.source = read_entire_file(file);
    if (blueprint.source.size == 0) {
        print("No blueprint file available or file is empty. Attempted to open file %S\n", file);

        return blueprint;
    }

    blueprint.file = allocate_string(App.string_alloc, platform_get_full_path(file));
    blueprint.path = path_without_filename(blueprint.file);

    Parser parser = init_parser(blueprint.source, blueprint.file);

    while (!current_token_is(&parser, TOKEN_END_OF_INPUT)) {
        if (!parse_statement(&parser, &blueprint)) {
            return blueprint;
        }
    }
    blueprint.valid = true;

    return blueprint;
}

void free_sheet(Sheet *sheet) {
    destroy(sheet->sources);
    destroy(sheet->include_dirs);
    destroy(sheet->symbols);
    destroy(sheet->libraries);
    destroy(sheet->dependencies);
    destroy(sheet->diagnostics);

    INIT_STRUCT(sheet);
}

void free_blueprint(Blueprint *blueprint) {
    FOR (blueprint->sheets, sheet) {
        free_sheet(sheet);
    }

    FOR (blueprint->blueprints, bp) {
        free_blueprint(bp);
    }

    destroy(blueprint->sheets);
    destroy(blueprint->blueprints);

    destroy_string(&blueprint->source);

    INIT_STRUCT(blueprint);
}

