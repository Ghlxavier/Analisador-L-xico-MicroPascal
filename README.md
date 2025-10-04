Analisador Léxico para MicroPascal (implementação em C).
 * - Comentários e documentação em Português BR.
 * - Reconhece: palavras reservadas, identificadores, números (inteiros e reais),
 *   operadores ( + - * / := = <> < <= >= > ), pontuação, parênteses, ';' ',' '.' ':' .
 * - Tabela de Símbolos (TS) armazena palavras reservadas e identificadores (sem duplicatas).
 * - Gera saída em arquivo <nome_entrada>.lex com lista de tokens: <NOME,lexema> linha:coluna
 * - Detecta e reporta erros léxicos
