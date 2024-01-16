#ifndef MYLOG
#define MYLOG

#include <stdarg.h> 
#include <stdio.h> 
FILE *f;
 
// your own printf 
int log_up(const char* str, ...) 
{ 
    // initializing list pointer 
    va_list ptr; 
    va_start(ptr, str); 
  
    // char array to store token 
    char token[1000]; 
    // index of where to store the characters of str in 
    // token 
    int k = 0; 
  
    // parsing the formatted string 
    for (int i = 0; str[i] != '\0'; i++) { 
        token[k++] = str[i]; 
  
        if (str[i + 1] == '%' || str[i + 1] == '\0') { 
            token[k] = '\0'; 
            k = 0; 
            if (token[0] != '%') { 
                fprintf( 
                    f, "%s", 
                    token); // printing the whole token if 
                            // it is not a format specifier 
                fflush(f);
            } 
            else { 
                int j = 1; 
                char ch1 = 0; 
  
                // this loop is required when printing 
                // formatted value like 0.2f, when ch1='f' 
                // loop ends 
                while ((ch1 = token[j++]) < 58) { 
                } 
                // for integers 
                if (ch1 == 'i' || ch1 == 'd' || ch1 == 'u'
                    || ch1 == 'h') { 
                    fprintf(f, token, 
                            va_arg(ptr, int)); 
                    fflush(f);
                } 
                // for characters 
                else if (ch1 == 'c') { 
                    fprintf(f, token, 
                            va_arg(ptr, int)); 
                } 
                // for float values 
                else if (ch1 == 'f') { 
                    fprintf(f, token, 
                            va_arg(ptr, double)); 
                    fflush(f);
                } 
                else if (ch1 == 'l') { 
                    char ch2 = token[2]; 
  
                    // for long int 
                    if (ch2 == 'u' || ch2 == 'd'
                        || ch2 == 'i') { 
                        fprintf(f, token, 
                                va_arg(ptr, long)); 
                        fflush(f);
                    } 
  
                    // for double 
                    else if (ch2 == 'f') { 
                        fprintf(f, token, 
                                va_arg(ptr, double)); 
                        fflush(f);
                    } 
                } 
                else if (ch1 == 'L') { 
                    char ch2 = token[2]; 
  
                    // for long long int 
                    if (ch2 == 'u' || ch2 == 'd'
                        || ch2 == 'i') { 
                        fprintf(f, token, 
                                va_arg(ptr, long long)); 
                        fflush(f);
                    } 
  
                    // for long double 
                    else if (ch2 == 'f') { 
                        fprintf(f, token, 
                                va_arg(ptr, long double)); 
                        fflush(f);
                    } 
                } 
  
                // for strings 
                else if (ch1 == 's') { 
                    fprintf(f, token, 
                            va_arg(ptr, char*)); 
                    fflush(f);
                } 
  
                // print the whole token 
                // if no case is matched 
                else { 
                    fprintf(f, "%s", token);
                    fflush(f); 
                } 
            } 
        } 
    } 
    // ending traversal 
    va_end(ptr); 
    return 0; 
} 
#endif 