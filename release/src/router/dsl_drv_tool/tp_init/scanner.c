#include <ctype.h>
#include "scanner.h"


#define MAX_TOKEN_BUF 512
static char m_token_buffer[MAX_TOKEN_BUF+1];
static int m_buffer_count;
//
static char* m_file_in_ptr;
static int m_file_in_len;
static int m_file_in_pos;

void clear_buffer(void)
{
    for (m_buffer_count=0; m_buffer_count<sizeof(m_token_buffer); m_buffer_count++) {
        m_token_buffer[m_buffer_count]=0;
    }
    m_buffer_count=0;
}

void buffer_char(char c)
{
    if (m_buffer_count >= MAX_TOKEN_BUF) return;
    m_token_buffer[m_buffer_count++]=c;
}


///////////////////////

int src_eof()
{
    if (m_file_in_pos > m_file_in_len) return 1;
    return 0;
}

char src_getc()
{
    char c;
    if (m_file_in_pos > m_file_in_len) return 0;
    c = *m_file_in_ptr++;
    m_file_in_pos++;
    return c;
}

void src_ungetc(char c)
{
    if (m_file_in_pos < 1) return;
    m_file_in_ptr--;
    *m_file_in_ptr = c;
    m_file_in_pos--;
}

////////////

void scanner_set(char* pByte, int Len)
{
    m_file_in_ptr = pByte;
    m_file_in_len = Len;
    m_file_in_pos = 0;
}

char* scanner_get_str(void)
{
    return m_token_buffer;
}

int ishex(char c)
{
   return (((c >= '0') && (c <= '9')) || ((c >='A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f')));
}

token scanner()
{
    char in_char, c;

    clear_buffer();
    if (m_file_in_ptr == 0) return TOKEN_EOF;
    if (src_eof()) return TOKEN_EOF;

    while (((in_char=src_getc())!=0)) {
        if (isspace(in_char))
            continue;
        else if (in_char == 0x0d)
            continue;            
        else if (in_char == 0x0a)
            continue;            
            
/*            
        else if (in_char=='"') {
            buffer_char(in_char);
            for (c=src_getc(in); c!='"' ; c=src_getc(in)) {
                buffer_char(c);
            }
            buffer_char(in_char);
            return String;
        }
*/                    
         else if (isalpha(in_char)) {
            buffer_char(in_char);
            for (c=src_getc(); 
                c!=' ' && c!=',' && c!=';' && c!=':' && c!='=' && c!='\t' && c!='\n'; 
                c=src_getc()) {
                if (c==0) break;
                buffer_char(c);
            }
            src_ungetc(c);
            return TOKEN_STRING;
        } else if (isdigit(in_char) || in_char=='-') {
            buffer_char(in_char);
            if (in_char == '0')
            {
                c=src_getc();
                if (c == 'x' || c == 'X')
                {
                    buffer_char(c);
                    for (c=src_getc(); ishex(c); c=src_getc()) {
                        if (c==0) break;            
                        buffer_char(c);
                    }
                    src_ungetc(c);                    
                    return TOKEN_NUMBER_LITERAL_HEX_FORMAT;
                }
                src_ungetc(c);
            }
            for (c=src_getc(); isdigit(c) || c=='.'; c=src_getc()) {
                if (c==0) break;            
                buffer_char(c);
            }
            src_ungetc(c);
            return TOKEN_NUMBER_LITERAL;
        }    
        else if (in_char==',')
            return TOKEN_COMMA;
        else if (in_char==';')
            return TOKEN_SEMICOLON;
        else if (in_char==':')
            return TOKEN_COLON;            
        else if (in_char=='=')
            return TOKEN_EQUAL;            
    }
    
    return TOKEN_EOF;
}


token scanner_get_line()
{
    char in_char, c;

    clear_buffer();
    if (m_file_in_ptr == 0) return TOKEN_EOF;
    if (src_eof()) return TOKEN_EOF;

    while (((in_char=src_getc())!=0)) {
        if (isspace(in_char))
            continue;
        else if (in_char == 0x0d)
            continue;            
        else if (in_char == 0x0a)
            continue;            
        else if (isalpha(in_char) || isdigit(in_char)) {
            buffer_char(in_char);
            for (c=src_getc(); 
                c!=',' && c!='\t' && c!='\n'; 
                c=src_getc()) {
                if (c==0) break;
                buffer_char(c);
            }
            src_ungetc(c);
            return TOKEN_STRING;
        }    
    }
    
    return TOKEN_EOF;
}


token scanner_get_ip()
{
    char in_char, c;

    clear_buffer();
    if (m_file_in_ptr == 0) return TOKEN_EOF;
    if (src_eof()) return TOKEN_EOF;

    while (((in_char=src_getc())!=0)) {
        if (isspace(in_char))
            continue;
        else if (in_char == 0x0d)
            continue;            
        else if (in_char == 0x0a)
            continue;            
        else if (isdigit(in_char)) {
            buffer_char(in_char);
            for (c=src_getc(); 
                c!=',' && c!='\t' && c!='\n'; 
                c=src_getc()) {
                if (c==0) break;
                buffer_char(c);
            }
            src_ungetc(c);
            return TOKEN_STRING;
        }    
    }
    
    return TOKEN_EOF;
}

