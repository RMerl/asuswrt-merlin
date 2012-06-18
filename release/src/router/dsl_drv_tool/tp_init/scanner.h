typedef enum {
    TOKEN_EOF,
    TOKEN_COMMA,
    TOKEN_STRING,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_EQUAL,    
    TOKEN_NUMBER_LITERAL,
    TOKEN_NUMBER_LITERAL_HEX_FORMAT
} token;

void scanner_set(char* pByte, int Len);
token scanner();
token scanner_get_line();
char* scanner_get_str(void);
token scanner_get_ip();

