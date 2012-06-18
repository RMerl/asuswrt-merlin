#define MAX_TOKEN_BUF 2048

typedef enum {
    TOKEN_EOF,
    TOKEN_COMMA,
    TOKEN_STRING,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_EQUAL,    
	TOKEN_CAT,
	TOKEN_LB,
    TOKEN_NUMBER_LITERAL,
    TOKEN_NUMBER_LITERAL_HEX_FORMAT
} token;

void scanner_set(WCHAR* pByte, int Len);
token scanner();
token scanner_get_line();
WCHAR* scanner_get_str(void);
token scanner_get_ip();
token scanner_get_str_bracket(void);
token scanner_get_rest();
