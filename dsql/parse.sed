s/^short yy/static short yy/
s/^long  yy/static long yy/
s/^int yy/static int yy/
s/^int \*yy/static int *yy/
s/^YYSTYPE yy/static YYSTYPE yy/
s/^YYSTYPE \*yy/static YYSTYPE *yy/
s/^yytabelem yy/static CONST yytabelem yy/
s/^yytoktype yy/static CONST yytoktype yy/
s/^char \* yy/static char * yy/
s/^static int yychar;/#ifndef SHLIB_DEFS\
int yychar;\
#endif/
s/^static int yyerrflag;/#ifndef SHLIB_DEFS\
int yyerrflag;\
#endif/
s/yychar/DSQL_yychar/g
s/yyerrflag/DSQL_yyerrflag/g
s/yyparse/dsql_yyparse/g
s/static int dsql_yyparse/int dsql_yyparse/g
/^#ident/d
s/^static TOK tokens/static CONST TOK FAR_VARIABLE tokens/
s/^__YYSCLASS yytabelem/__YYSCLASS CONST yytabelem FAR_VARIABLE/
s/^__YYSCLASS yytoktype/__YYSCLASS CONST yytoktype FAR_VARIABLE/
s/^__YYSCLASS char/__YYSCLASS char FAR_VARIABLE/
s/^#include *<values.h> *$//g
s/yylex(void)/yylex(USHORT, USHORT, USHORT, BOOLEAN *)/g
s/yylex()/yylex(client_dialect, db_dialect, parser_version, stmt_ambiguous)/g
s/dsql_yyparse(void);/dsql_yyparse(USHORT, USHORT, USHORT, BOOLEAN *);/g
s/dsql_yyparse(void)/dsql_yyparse(USHORT client_dialect, USHORT db_dialect, USHORT parser_version, BOOLEAN *stmt_ambiguous)/g
s/dsql_yyparse()/dsql_yyparse(USHORT client_dialect, USHORT db_dialect, USHORT parser_version, BOOLEAN *stmt_ambiguous)/g
s/YYLEX()/YYLEX(client_dialect, db_dialect, parser_version, stmt_ambiguous)/g
