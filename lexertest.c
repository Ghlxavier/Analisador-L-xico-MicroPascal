#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
    /* Operadores */
    OP_EQ,    /* =  */
    OP_NE,    /* <> */
    OP_LT,    /* <  */
    OP_LE,    /* <= */
    OP_GT,    /* >  */
    OP_GE,    /* >= */
    OP_AD,    /* +  */
    OP_MIN,   /* -  */
    OP_MUL,   /* *  */
    OP_DIV,   /* /  */
    OP_ASS,   /* := */
    SMB_OPA,  /* ( */
    SMB_CPA,  /* ) */
    SMB_COM,  /* , */
    SMB_SEM,  /* ; */
    SMB_OBC,  /* { */
    SMB_CBC,  /* } */
    SMB_DOT,  /* . */
    SMB_COL,  /* : */
    KW_PROGRAM,
    KW_VAR,
    KW_INTEGER,
    KW_REAL,
    KW_BEGIN,
    KW_END,
    KW_IF,
    KW_THEN,
    KW_ELSE,
    KW_WHILE,
    KW_DO,
    ID,
    NUM_INT,
    NUM_REAL,
    T_EOF,
    T_ERROR
} TokenType;

static const char* token_name(TokenType t){
    switch(t){
        case OP_EQ: return "OP_EQ";
        case OP_NE: return "OP_NE";
        case OP_LT: return "OP_LT";
        case OP_LE: return "OP_LE";
        case OP_GT: return "OP_GT";
        case OP_GE: return "OP_GE";
        case OP_AD: return "OP_AD";
        case OP_MIN:return "OP_MIN";
        case OP_MUL:return "OP_MUL";
        case OP_DIV:return "OP_DIV";
        case OP_ASS:return "OP_ASS";

        case SMB_OPA:return "SMB_OPA";
        case SMB_CPA:return "SMB_CPA";
        case SMB_COM:return "SMB_COM";
        case SMB_SEM:return "SMB_SEM";
        case SMB_OBC:return "SMB_OBC";
        case SMB_CBC:return "SMB_CBC";
        case SMB_DOT:return "SMB_DOT";
        case SMB_COL: return "SMB_COL";

        case KW_PROGRAM: return "program";
        case KW_VAR:     return "var";
        case KW_INTEGER: return "integer";
        case KW_REAL:    return "real";
        case KW_BEGIN:   return "begin";
        case KW_END:     return "end";
        case KW_IF:      return "if";
        case KW_THEN:    return "then";
        case KW_ELSE:    return "else";
        case KW_WHILE:   return "while";
        case KW_DO:      return "do";

        case ID:      return "ID";
        case NUM_INT: return "NUM_INT";
        case NUM_REAL:return "NUM_REAL";
        case T_EOF:   return "EOF";
        case T_ERROR: return "ERROR";
        default: return "?";
    }
}


typedef struct {
    TokenType type;
    char *lexeme; /* malloc'd; pode ser mensagem no caso de erro */
    int line;
    int col;
    int st_index; /* índice na TS (>=0) se for KW/ID; -1 caso contrário */
} Token;

static char* xstrndup_(const char* s, size_t n){
    char *p = (char*)malloc(n+1);
    if(!p){ perror("malloc"); exit(1); }
    memcpy(p, s, n); p[n]='\0'; return p;
}
static char* xstrdup_(const char* s){ return xstrndup_(s, strlen(s)); }


typedef struct {
    char *lexeme;
    TokenType type; /* KW_* ou ID */
} Sym;

typedef struct {
    Sym *v;
    size_t n, cap;
} SymTab;

static void st_init(SymTab *st){
    st->cap=64; st->n=0;
    st->v=(Sym*)calloc(st->cap,sizeof(Sym));
    if(!st->v){ perror("calloc"); exit(1); }
}
static void st_free(SymTab *st){
    for(size_t i=0;i<st->n;i++) free(st->v[i].lexeme);
    free(st->v);
}
static void st_grow(SymTab *st){
    st->cap*=2;
    st->v=(Sym*)realloc(st->v, st->cap*sizeof(Sym));
    if(!st->v){ perror("realloc"); exit(1); }
}
static int st_find(const SymTab *st, const char *lex){
    for(size_t i=0;i<st->n;i++){
        if(strcmp(st->v[i].lexeme, lex)==0) return (int)i;
    }
    return -1;
}
static int st_intern(SymTab *st, const char *lex, TokenType as_type){
    int idx = st_find(st, lex);
    if(idx>=0) return idx;
    if(st->n==st->cap) st_grow(st);
    st->v[st->n].lexeme = xstrdup_(lex);
    st->v[st->n].type   = as_type;
    return (int)st->n++;
}
static void st_preload_keywords(SymTab *st){
    st_intern(st, "program", KW_PROGRAM);
    st_intern(st, "var",     KW_VAR);
    st_intern(st, "integer", KW_INTEGER);
    st_intern(st, "real",    KW_REAL);
    st_intern(st, "begin",   KW_BEGIN);
    st_intern(st, "end",     KW_END);
    st_intern(st, "if",      KW_IF);
    st_intern(st, "then",    KW_THEN);
    st_intern(st, "else",    KW_ELSE);
    st_intern(st, "while",   KW_WHILE);
    st_intern(st, "do",      KW_DO);
}

static void st_dump(const SymTab *st, FILE *out){
    fprintf(out, "===== TABELA DE SÍMBOLOS =====\n");
    for(size_t i=0;i<st->n;i++){
        fprintf(out, "%4zu: (%s, \"%s\")\n", i, token_name(st->v[i].type), st->v[i].lexeme);
    }
}

typedef struct {
    FILE *in;
    int c;    /* caractere atual */
    int line; /* 1.. */
    int col;  /* 1.. */
    SymTab *st;
} Lexer;

static int nextc(Lexer *L){
    int ch = fgetc(L->in);
    if(ch=='\n'){ L->line++; L->col=1; }
    else if(ch!=EOF){ L->col++; }
    return ch;
}

static void lex_init(Lexer *L, FILE *in, SymTab *st){
    L->in=in; L->line=1; L->col=0; L->st=st;
    L->c = nextc(L);
}
static void lex_close(Lexer *L){ (void)L; }

static void skip_ws(Lexer *L){
    while(L->c!=EOF && isspace(L->c)) L->c = nextc(L);
}

static Token make_token(TokenType tp, const char *buf, size_t n, int line, int col, int st_index){
    Token t; t.type=tp; t.lexeme=xstrndup_(buf,n); t.line=line; t.col=col; t.st_index=st_index; return t;
}
static Token simple(Lexer *L, TokenType tp, int line, int col){
    char ch = (char)L->c; L->c = nextc(L);
    return make_token(tp,&ch,1,line,col,-1);
}

static Token ident_or_kw(Lexer *L){
    int line=L->line, col=L->col;
    char buf[256]; size_t n=0;
    while(L->c!=EOF && (isalpha(L->c) || isdigit(L->c) || L->c=='_')){
        if(n+1<sizeof(buf)) buf[n++] = (char)tolower(L->c);
        L->c = nextc(L);
    }
    buf[n]='\0';
    int idx = st_find(L->st, buf);
    if(idx>=0 && L->st->v[idx].type>=KW_PROGRAM && L->st->v[idx].type<=KW_DO){
        return make_token(L->st->v[idx].type, buf, n, line, col, idx);
    }else{
        idx = st_intern(L->st, buf, ID);
        return make_token(ID, buf, n, line, col, idx);
    }
}

static Token number(Lexer *L){
    int line=L->line, col=L->col;
    char buf[256]; size_t n=0;
    int has_dot=0, has_exp=0;

    while(isdigit(L->c)){
        if(n+1<sizeof(buf)) buf[n++]=(char)L->c;
        L->c = nextc(L);
    }
    if(L->c=='.'){
        has_dot=1;
        if(n+1<sizeof(buf)) buf[n++]='.';
        L->c = nextc(L);
        while(isdigit(L->c)){
            if(n+1<sizeof(buf)) buf[n++]=(char)L->c;
            L->c = nextc(L);
        }
    }
    if(L->c=='e' || L->c=='E'){
        has_exp=1;
        if(n+1<sizeof(buf)) buf[n++]=(char)tolower(L->c);
        L->c = nextc(L);
        if(L->c=='+' || L->c=='-'){
            if(n+1<sizeof(buf)) buf[n++]=(char)L->c;
            L->c = nextc(L);
        }
        if(!isdigit(L->c)){
            const char *msg = "Expoente malformado em número real";
            return make_token(T_ERROR, msg, strlen(msg), line, col, -1);
        }
        while(isdigit(L->c)){
            if(n+1<sizeof(buf)) buf[n++]=(char)L->c;
            L->c = nextc(L);
        }
    }

    return make_token((has_dot||has_exp)?NUM_REAL:NUM_INT, buf, n, line, col, -1);
}

static Token string_handle(Lexer *L){
    int line=L->line, col=L->col;
    L->c = nextc(L);
    while(L->c!=EOF && L->c!='\n' && L->c!='\''){
        L->c = nextc(L);
    }
    if(L->c=='\''){
        L->c = nextc(L);
        const char *msg="Strings não são suportadas na linguagem";
        return make_token(T_ERROR,msg,strlen(msg),line,col,-1);
    }else{
        const char *msg="String não-fechada antes de quebra de linha";
        return make_token(T_ERROR,msg,strlen(msg),line,col,-1);
    }
}

static Token lex_next(Lexer *L){
    skip_ws(L);
    if(L->c==EOF) return make_token(T_EOF,"",0,L->line,L->col,-1);

    int line=L->line, col=L->col;

    if(isalpha(L->c) || L->c=='_') return ident_or_kw(L);

    if(isdigit(L->c)) return number(L);

    if(L->c=='\'') return string_handle(L);

if (L->c == ':') {
    int c1 = nextc(L);          /* lookahead */
    if (c1 == '=') {
        L->c = nextc(L);        /* já consumiu ':', '=', avança */
        return make_token(OP_ASS, ":=", 2, line, col, -1);
    } else {
        L->c = c1;              /* devolve o lookahead para a leitura normal */
        return make_token(SMB_COL, ":", 1, line, col, -1);
    }
}
    if(L->c=='<'){
        int c1 = nextc(L);
        if(c1=='='){ L->c = nextc(L); return make_token(OP_LE,"<=",2,line,col,-1); }
        if(c1=='>'){ L->c = nextc(L); return make_token(OP_NE,"<>",2,line,col,-1); }
        return make_token(OP_LT,"<",1,line,col,-1);
    }
    if(L->c=='>'){
        int c1 = nextc(L);
        if(c1=='='){ L->c = nextc(L); return make_token(OP_GE,">=",2,line,col,-1); }
        return make_token(OP_GT,">",1,line,col,-1);
    }

    switch(L->c){
        case '+': return simple(L, OP_AD,  line, col);
        case '-': return simple(L, OP_MIN, line, col);
        case '*': return simple(L, OP_MUL, line, col);
        case '/': return simple(L, OP_DIV, line, col);
        case '=': return simple(L, OP_EQ,  line, col);
        case '(': return simple(L, SMB_OPA,line, col);
        case ')': return simple(L, SMB_CPA,line, col);
        case ',': return simple(L, SMB_COM,line, col);
        case ';': return simple(L, SMB_SEM,line, col);
        case '{': return simple(L, SMB_OBC,line, col);
        case '}': return simple(L, SMB_CBC,line, col);
        case '.': return simple(L, SMB_DOT,line, col);
        default: {
            char buf[64];
            snprintf(buf,sizeof(buf),"Caractere inválido: '%c'", L->c);
            L->c = nextc(L);
            return make_token(T_ERROR, buf, strlen(buf), line, col, -1);
        }
    }
}


static void token_free(Token *t){ if(t->lexeme) free(t->lexeme); t->lexeme=NULL; }

static void write_lex(FILE *out, FILE *in){
    SymTab st; st_init(&st); st_preload_keywords(&st);
    Lexer L; lex_init(&L, in, &st);

    for(;;){
        Token t = lex_next(&L);
        fprintf(out, "<%s, \"%s\"> @ %d:%d\n", token_name(t.type), t.lexeme, t.line, t.col);
        printf("<%s, \"%s\"> @ %d:%d\n", token_name(t.type), t.lexeme, t.line, t.col);

        if(t.type==T_EOF || t.type==T_ERROR){
            token_free(&t);
            break;
        }
        token_free(&t);
    }
    fprintf(out, "\n");
    st_dump(&st, out);

    lex_close(&L);
    st_free(&st);
}

int main(int argc, char **argv){
    if(argc<2){
        fprintf(stderr, "Uso: %s arquivo.pas\n", argv[0]);
        return 1;
    }
    const char *inpath = argv[1];
    FILE *in = fopen(inpath, "r");
    if(!in){ perror("fopen entrada"); return 1; }

    char outpath[1024];
    snprintf(outpath, sizeof(outpath), "%s.lex", inpath);
    FILE *out = fopen(outpath, "w");
    if(!out){ perror("fopen saida .lex"); fclose(in); return 1; }

    write_lex(out, in);

    fclose(in);
    fclose(out);
    return 0;
}