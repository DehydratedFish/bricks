#include "bricks.h"

#include "blueprint.h"
#include "platform.h"
#include "string_builder.h"
#include "io.h"


extern ApplicationState App;

struct SourceLocation {
    u8 *ptr;
    s32 line;
    s32 column;
};

enum TokenKind {
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
    TOKEN_KEYWORD_IMPORT,
    TOKEN_KEYWORD_AS,

    TOKEN_MISSING_QUOTE,

    TOKEN_UNKNOWN,

    TOKEN_COUNT
};
struct Token {
    TokenKind kind;
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

    Blueprint *bp;
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

INTERNAL s32 trim_indentation(String *line) {
    s32 removed = 0;

    for (s64 i = 0; i < line->size; i += 1) {
        if (line->data[i] != ' ' && line->data[i] != '\t') break;

        removed += 1;
    }

    *line = shrink_front(*line, removed);

    return removed;
}

INTERNAL void parse_error(Parser *parser, SourceLocation loc, String message) {
    String line    = find_line(loc, parser->source_code);
    s32 line_start = trim_indentation(&line);
    s32 spaces = loc.column - line_start - 1;

    StringBuilder builder = {};
    DEFER(destroy(&builder));

    format(&builder, "Error in blueprint file %S: %d:%d: %S\n", parser->current_file, loc.line, loc.column, message);
    format(&builder, "%S\n", line);

    for (s32 i = 0; i < spaces; i += 1) {
        append(&builder, " ");
    }
    append(&builder, "^");

    add_diagnostic(DIAG_ERROR, to_allocated_string(&builder, App.string_alloc));
    parser->bp->status = BLUEPRINT_ERROR;
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

    if      (token.content == "executable") token.kind = TOKEN_KEYWORD_EXECUTABLE;
    else if (token.content == "brick")      token.kind = TOKEN_KEYWORD_BRICK;
    else if (token.content == "library")    token.kind = TOKEN_KEYWORD_LIBRARY;
    // TODO: Do something like a global import. So the blueprint will always be loaded from the brickyard.
    else if (token.content == "import")     token.kind = TOKEN_KEYWORD_IMPORT;
    else if (token.content == "as")         token.kind = TOKEN_KEYWORD_AS;

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


// TODO: This is not right in cases of a missing ".
INTERNAL Token parse_string(Parser *parser) {
    SourceLocation location = parser->loc;

    u8 *mark = parser->source_code.data;
    TokenKind kind = TOKEN_STRING;
    s64 size = 0;

    u8 c;
    do {
        if (!get_char(parser, &c)) {
            kind = TOKEN_MISSING_QUOTE;
            break;
        }

        size += 1;
    } while (c != '"');

    size -= 1;

    Token token = {
        kind,
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
    case '"': {
        token = parse_string(parser); 
        if (token.kind == TOKEN_MISSING_QUOTE) {
            parse_error(parser, token.loc, "Missing .");
        }
    } break;

    case ';': { token.kind = TOKEN_SEMICOLON; } break;
    case '*': { token.kind = TOKEN_ASTERISK;  } break;
    case '/': {
        if (match_char(parser, '/')) {
            skip_comment(parser);

            // TODO: This leads to a lot of recursion on multiple comments.
            return next_token(parser);
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

    Token token = {};
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

Parser init_parser(String source, Blueprint *blueprint) {
    Parser parser = {};
    parser.bp = blueprint;

    parser.source_code = source;
    assert(parser.source_code.size != 0);

    parser.current_file = blueprint->file;

    parser.loc.ptr = source.data;
    parser.loc.line = 1;
    parser.loc.column = 1;

    advance_token(&parser);

    return parser;
}

INTERNAL bool current_token_is(Parser *parser, TokenKind kind) {
    return parser->current_token.kind == kind;
}

INTERNAL bool consume(Parser *parser, TokenKind kind, String error_msg) {
    if (current_token_is(parser, kind)) {
        advance_token(parser);
        return true;
    } else {
        parse_error(parser, parser->current_token.loc, error_msg);
    }

    return false;
}

INTERNAL bool match(Parser *parser, TokenKind kind) {
    if (current_token_is(parser, kind)) {
        advance_token(parser);
        return true;
    }
    return false;
}



INTERNAL void default_config(Blueprint *blueprint) {
    *blueprint = {};

#ifdef OS_WINDOWS
    blueprint->compiler = "msvc";
    blueprint->linker   = "msvc";
#endif

    blueprint->build_type = App.build_type;
}


INTERNAL void parse_declaration(Parser *parser, Blueprint *blueprint) {
    if (!consume(parser, TOKEN_IDENTIFIER, "Missing field name.")) return;
    Token field = parser->previous_token;

    if (!consume(parser, TOKEN_COLON, "Missing : in field declaration.")) return;

    if (equal(field.content, "compiler")) {
        if (!consume(parser, TOKEN_STRING, "Expected string in compiler field.")) return;
        blueprint->compiler = parser->previous_token.content;
    } else if (equal(field.content, "linker")) {
        if (!consume(parser, TOKEN_STRING, "Expected string in linker field.")) return;
        blueprint->linker = parser->previous_token.content;
    } else if (equal(field.content, "build_folder")) {
        if (!consume(parser, TOKEN_STRING, "Expected string in build_folder field.")) return;
        blueprint->build_folder = parser->previous_token.content;
    } else {
        parse_error(parser, field.loc, "Unknown identifier.");
    }

    consume(parser, TOKEN_SEMICOLON, "Missing ; after declaration");
}

INTERNAL String combine_file_path(String folder, String sub_folder, String name) {
    StringBuilder builder = {};
    if (folder != "") {
        folder = remove_trailing_slashes(folder);

        append(&builder, folder);
        append(&builder, '/');
    }

    if (sub_folder != "") {
        sub_folder = remove_leading_slashes(sub_folder);
        sub_folder = remove_trailing_slashes(sub_folder);

        append(&builder, sub_folder);
        append(&builder, '/');
    }

    name = remove_leading_slashes(name);
    append(&builder, name);
    
    return to_allocated_string(&builder, App.string_alloc);
}

INTERNAL String EntityKindLookup[ENTITY_COUNT] = {
    "<NONE>",
    "Blueprint",
    "Brick",
    "Executable",
    "Library",
};

INTERNAL String enum_string(EntityKind kind) {
    return EntityKindLookup[kind];
}

INTERNAL b32 parse_deps(Parser *parser, Entity *entity, b32 skip) {
    b32 result = true;

    do {
        if (match(parser, TOKEN_IDENTIFIER)) {
            Dependency dep = {};
            dep.entity = parser->previous_token.content;
            if (match(parser, TOKEN_DOT)) {
                if (!consume(parser, TOKEN_IDENTIFIER, "Identifier needed after . in dependency.")) return false;
                dep.module = dep.entity;
                dep.entity = parser->previous_token.content;
            }
            if (skip) continue;

            if (dep.module != "") {
                dep.module = allocate_string(dep.module, App.string_alloc);
            }
            dep.entity = allocate_string(dep.entity, App.string_alloc);
            append(&entity->dependencies, dep);
        } else if (match(parser, TOKEN_STRING)) {
            append(&entity->libraries, allocate_string(parser->previous_token.content, App.string_alloc));
        } else {
            parse_error(parser, parser->current_token.loc, "Expected library string or entity identifier.");
        }
    } while (match(parser, TOKEN_COMMA));

    return result;
}

INTERNAL b32 parse_symbols(Parser *parser, Entity *entity, b32 skip) {
    b32 result = true;

    do {
        if (!consume(parser, TOKEN_STRING, "Expected string as symbol.")) result = false;

        if (skip) continue;
        append(&entity->symbols, allocate_string(parser->previous_token.content, App.string_alloc));
    } while (match(parser, TOKEN_COMMA));

    return result;
}

INTERNAL b32 parse_includes(Parser *parser, Entity *entity, b32 skip) {
    b32 result = true;

    do {
        if (!consume(parser, TOKEN_STRING, "Expected string as include folder.")) result = false;
        if (skip) continue;

        String file = combine_file_path(parser->bp->path, "", parser->previous_token.content);
        append(&entity->include_folders, file);
    } while (match(parser, TOKEN_COMMA));

    return result;
}

INTERNAL b32 parse_build_folder(Parser *parser, Entity *entity, b32 skip) {
    b32 result = false;

    if (consume(parser, TOKEN_STRING, "Missing build folder.")) {
        if (!skip) {
            entity->build_folder = allocate_string(parser->previous_token.content, App.string_alloc);
        }
        result = true;
    }

    return result;
}

INTERNAL b32 parse_sources(Parser *parser, Entity *entity, b32 skip) {
    b32 result = false;

    String sub_folder = "";
    do {
        if (match(parser, TOKEN_STRING)) {
            if (skip) continue;

            String file = combine_file_path(parser->bp->path, sub_folder, parser->previous_token.content);
            append(&entity->sources, file);
        } else if (match(parser, TOKEN_SLASH)) {
            if (!consume(parser, TOKEN_STRING, "Expected sub folder string.")) return result;
            sub_folder = parser->previous_token.content;
        } else {
            parse_error(parser, parser->current_token.loc, "Expexted source file.");
        }
    } while (match(parser, TOKEN_COMMA));

    result = true;
    return result;
}

INTERNAL b32 parse_field_spec(Parser *parser, Blueprint *bp) {
    b32 skip_field = true;

    if (match(parser, TOKEN_LEFT_PARENTHESIS)) {
        if (match(parser, TOKEN_RIGHT_PARENTHESIS)) {
            return false;
        }

        do {
            if (match(parser, TOKEN_RIGHT_PARENTHESIS)) {
                break;
            } else if (match(parser, TOKEN_IDENTIFIER)) {
                if (parser->previous_token.content == bp->build_type) skip_field = false;
            } else if (match(parser, TOKEN_HASHTAG)) {
                // TODO: Also allow strings?
                if (!consume(parser, TOKEN_IDENTIFIER, "Expected platform specifier.")) return true;
                if (parser->previous_token.content == App.target_platform) skip_field = false;
            } else {
                parse_error(parser, parser->current_token.loc, "Expected build type or platform specifier as field argument.");
            }
        } while (match(parser, TOKEN_COMMA));

        match(parser, TOKEN_RIGHT_PARENTHESIS);
    } else {
        skip_field = false;
    }

    return skip_field;
}

INTERNAL b32 parse_field(Parser *parser, Entity *entity, Blueprint *bp) {
    b32 result = false;
    if (!consume(parser, TOKEN_IDENTIFIER, t_format("Unkown field %S.", parser->current_token.content))) {
        return result;
    }
    
    Token name_token = parser->previous_token;
    String name = name_token.content;

    b32 skip_field = parse_field_spec(parser, bp);

    if (!consume(parser, TOKEN_COLON, "Missing : in field.")) return result;

    if        (name == "sources") {
        result = parse_sources(parser, entity, skip_field);
    } else if (name == "folder") {
        result = parse_build_folder(parser, entity, skip_field);
    } else if (name == "include") {
        result = parse_includes(parser, entity, skip_field);
    } else if (name == "symbols") {
        result = parse_symbols(parser, entity, skip_field);
    } else if (name == "dependencies") {
        result = parse_deps(parser, entity, skip_field);
    } else {
        parse_error(parser, name_token.loc, t_format("Unkown field %S.", name));
    }

    if (!result) return result;

    // NOTE: No consume as the error message is not helpful. Still not quite right though.
    if (!match(parser, TOKEN_SEMICOLON)) {
        parse_error(parser, parser->previous_token.loc, "Missing ; .");
        return false;
    }

    return result;
}

INTERNAL void parse_entity_declaration(Parser *parser, Blueprint *blueprint) {
    EntityKind  kind     = ENTITY_NONE;
    LibraryKind lib_kind = NO_LIBRARY;
    if (current_token_is(parser, TOKEN_KEYWORD_EXECUTABLE)) {
        kind = ENTITY_EXECUTABLE;
    } else if (current_token_is(parser, TOKEN_KEYWORD_LIBRARY)) {
        kind     = ENTITY_LIBRARY;
        lib_kind = STATIC_LIBRARY;
    } else if (current_token_is(parser, TOKEN_KEYWORD_BRICK)) {
        kind = ENTITY_BRICK;
    } else {
        String msg = t_format("Unknown entity type %S.", parser->current_token.content);
        parse_error(parser, parser->current_token.loc, msg);
    }

    Entity entity = {};
    entity.kind = kind;
    entity.lib_kind  = lib_kind;
    entity.compiler  = blueprint->compiler;
    entity.linker    = blueprint->linker;
    
    // NOTE: On static libraries the resulting lib should not necessarily be
    //       in the build folder. Only put it there if the blueprint specifies it.
    if (entity.kind != ENTITY_LIBRARY && entity.lib_kind != STATIC_LIBRARY) {
        entity.build_folder = blueprint->build_folder;
    }

    advance_token(parser);

    if (!consume(parser, TOKEN_COLON, t_format("Missing : after %S.", enum_string(entity.kind)))) return;
    if (!consume(parser, TOKEN_IDENTIFIER, t_format("Missing %S name.", enum_string(entity.kind)))) return;
    entity.name = parser->previous_token.content;

    if (!consume(parser, TOKEN_LEFT_BRACE, "Missing { in declaration.")) {
        entity.status = ENTITY_STATUS_ERROR;
        return;
    }

    do {
        if (match(parser, TOKEN_RIGHT_BRACE)) break;

        if (!parse_field(parser, &entity, blueprint)) {
            return;
        }
    } while (parser->previous_token.kind == TOKEN_SEMICOLON || match(parser, TOKEN_RIGHT_BRACE));

    append(&blueprint->entitys, entity);
}

INTERNAL void parse_import(Parser *parser, Blueprint *blueprint) {
    advance_token(parser);

    if (current_token_is(parser, TOKEN_IDENTIFIER) || current_token_is(parser, TOKEN_STRING)) {
        advance_token(parser);
    } else {
        parse_error(parser, parser->current_token.loc, "Import missing name or path.");
        return;
    }

    Token name_token = parser->previous_token;

    // TODO: Think of something to specifiy a version.
    String version = {};

    // TODO: Maybe support multiple groups?
    String group = {};

    if (match(parser, TOKEN_COLON)) {
        if(!consume(parser, TOKEN_IDENTIFIER, "Missing group name.")) {
            return;
        }
        group = parser->previous_token.content;
    }

    String name = {};
    if (match(parser, TOKEN_KEYWORD_AS)) {
        if (!consume(parser, TOKEN_IDENTIFIER, "Rename needs to be an identifier.")) return;

        name = allocate_string(parser->previous_token.content, App.string_alloc);
    } else {
        name = allocate_string(name_token.content, App.string_alloc);
    }

    // NOTE: First looking for a sub directory with the import name.
    String path = t_format("%S/blueprint", name_token.content);
    if (!platform_file_exists(path)) {
        path = find(&App.brickyard, name_token.content, version);
        if (path == "") {
            parse_error(parser, name_token.loc, t_format("Could not find %S in the Brickyard.", name_token.content));
            return;
        } else {
            path = t_format("%S/blueprint", path);
        }
    }

    if (!consume(parser, TOKEN_SEMICOLON, "Missing ; after import.")) return;

    Blueprint import = {};
    parse_blueprint_file(&import, path);
    import.name = name;

    if (import.status == BLUEPRINT_ERROR) {
        destroy(&import);
    } else {
        append(&blueprint->imports, import);
    }
}

INTERNAL void parse_statement(Parser *parser, Blueprint *blueprint) {
    switch (parser->current_token.kind) {
    case TOKEN_IDENTIFIER: {
        parse_declaration(parser, blueprint);
    } break;

    case TOKEN_KEYWORD_EXECUTABLE:
    case TOKEN_KEYWORD_BRICK:
    case TOKEN_KEYWORD_LIBRARY: {
        parse_entity_declaration(parser, blueprint);
    } break;

    case TOKEN_KEYWORD_IMPORT: {
        parse_import(parser, blueprint);
    } break;

    default:
        parse_error(parser, parser->current_token.loc, "Expected statement.");
    }
}

// TODO: Make this more usable/configurable for imports.
void parse_blueprint_file(Blueprint *blueprint, String file) {
    default_config(blueprint);

    auto read_result = platform_read_entire_file(file);
    if (read_result.error) {
        String full = t_format("%S/%S", App.starting_folder, file);
        if (read_result.error == PLATFORM_FILE_NOT_FOUND) {
            add_diagnostic(DIAG_ERROR, t_format("File not found %S.", full));
        } else {
            add_diagnostic(DIAG_ERROR, t_format("Read error in file %S.", full));
        }
        blueprint->status = BLUEPRINT_ERROR;

        return;
    }

    blueprint->file = allocate_string(file, App.string_alloc);
    blueprint->path = path_without_filename(blueprint->file);

    Parser parser = init_parser(read_result.content, blueprint);

    while (!current_token_is(&parser, TOKEN_END_OF_INPUT)) {
        parse_statement(&parser, blueprint);

        if (blueprint->status == BLUEPRINT_ERROR) break;
    }
}

void destroy(Entity *entity) {
    destroy(&entity->sources);
    destroy(&entity->include_folders);
    destroy(&entity->symbols);
    destroy(&entity->libraries);
    destroy(&entity->dependencies);

    INIT_STRUCT(entity);
}

void destroy(Blueprint *blueprint) {
    FOR (blueprint->entitys, entity) {
        destroy(entity);
    }

    FOR (blueprint->imports, bp) {
        destroy(bp);
    }

    destroy(&blueprint->entitys);
    destroy(&blueprint->imports);

    INIT_STRUCT(blueprint);
}


Blueprint *find_submodule(Blueprint *bp, String name) {
    if (name != "") {
        FOR (bp->imports, import) {
            if (import->name == name) {
                return import;
            }
        }

        add_diagnostic(DIAG_ERROR, t_format("No imported blueprint %S.\n", name));
        return 0;
    }

    return bp;
}

Entity *find_dependency(Blueprint *bp, String name) {
    FOR (bp->entitys, entity) {
        if (entity->name == name) {
            return entity;
        }
    }

    add_diagnostic(DIAG_ERROR, t_format("No entity %S in blueprint %S.\n", name, bp->name));
    return 0;
}

void print_diagnostics(Entity *entity) {
    FOR (entity->diagnostics, diag) {
        print("%S\n", diag->message);
    }
}

void add_diagnostic(Entity *entity, DiagnosticKind kind, String msg) {
    Diagnostic diag = {};
    diag.kind = kind;
    diag.message = allocate_string(msg, App.string_alloc);

    if (kind == DIAG_ERROR) entity->has_errors = true;
    
    append(&entity->diagnostics, diag);
}

