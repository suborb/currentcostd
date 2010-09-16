/*
 *   Current cost daemon - configuration library
 *   http://www.rst38.org.uk/currentcost
 *
 *   Copyright (C) 2005,2010 Dominic Morris
 *
 *   $Id: libini.c,v 1.1 2008/01/02 22:48:37 dom Exp $
 *   $Date: 2008/01/02 22:48:37 $
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation;  version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *   Small .ini file reader for Windows style .ini files
 *   Originally written as a z88dk library
 */



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "libini.h"


/* Some macros that allow the malloc implementation to be replaced easily */
#ifndef MALLOC
#define MALLOC(x)               malloc(x)
#define REALLOC(x,y)            realloc(x,y)
#define CALLOC(x,y)             calloc(x,y)
#define FREE(x)                 free(x)
#define FREENULL(x)             if(x) { FREE(x); (x) = NULL; }
#define STRDUP(x)               strdup(x)
#endif


/* Maximum length of line to read in from the file*/

#define LIBINI_LINE_MAX 1024


struct _option {
    char               *group;
    char               *word;
    char               *lopt;
    char                sopt;
    char               *desc;
    unsigned char       type;
    void               *value;
    int                *count;
    struct _option *next;
};

typedef struct _option option_t;



struct _cache {
    char            *group;
    char            *word;           /* Option value */
    int              num_values;
    char           **values;
    struct _cache   *next;
};

typedef struct _cache cache_t;


struct _configctx {
    option_t       *list;
    char           *current_group;
    char            do_cache;
    cache_t        *cache;
};



/* Internal functions */

static char       *skpws(char *ptr);
static void        strip_ws(char *ptr);
static void        get_section(configctx_t *ctx, char *ptr);
static int         option_set(configctx_t *ctx, char *opt, char *it);
static int         option_set_sopt(configctx_t *ctx, char sopt, char *it);
static void        option_do_set(option_t *option, char *value);
static int         iniread_file(configctx_t *ctx, FILE *fp);



void iniparse_cleanup(configctx_t *ctx)
{
    option_t *next;
    option_t *opt = ctx->list;
    cache_t  *cache = ctx->cache;
    cache_t  *ncache;
    int       i;

    while ( opt != NULL ) {
        free(opt->desc);
        free(opt->lopt);
        free(opt->group);
        free(opt->word);
        next = opt->next;
        free(opt);
        opt = next;
    }

    while ( cache != NULL ) {
        free(cache->group);
        free(cache->word);
        for ( i = 0; i < cache->num_values; i++ ) {
            free(cache->values[i]);
        }
        free(cache->values);
        ncache = cache->next;
        free(cache);
        cache = ncache;
    } 

    free(ctx->current_group);
    free(ctx);
}

configctx_t *iniparse_init(char *default_section)
{
    configctx_t    *ctx;

    ctx = calloc(1,sizeof(*ctx));
    ctx->current_group = strdup(default_section);
    ctx->list = NULL;
    ctx->cache = NULL;
    ctx->do_cache = 0;

    return ctx;
}


configctx_t *iniparse_cache_init()
{
    configctx_t    *ctx;

    ctx = calloc(1,sizeof(*ctx));
    ctx->current_group = strdup("");
    ctx->list = NULL;
    ctx->cache = NULL;
    ctx->do_cache = 1;
    return ctx;
}



int iniparse(configctx_t *ctx, char *filename, int argc, char *argv[])
{
    if ( ( iniparse_args(ctx, argc, argv) ) < 0 ) {
        return -1;
    }
    return iniparse_file(ctx,filename);
}



int iniparse_args(configctx_t *ctx, int argc, char *argv[])
{
    int  i;
    int  ret;
    char *arg;

    for ( i = 1; i < argc; i++ ) {
        if ( argv[i] && argv[i][0] == '-' ) {
            if ( argv[i][1] == '-' ) {
                char  *temp_section = ctx->current_group;
                char  *ptr, *option,*value = NULL;
                int    optc = i;

                arg = STRDUP(argv[i]);
                /* Long option handling - break out the section if present */
                if ( ( ptr = strchr(arg,':') ) != NULL ) {
                    *ptr = 0;  /* Truncate section */
                    ctx->current_group = &arg[2];
                    option = ptr + 1;
                } else {
                    option = &arg[2];
                    temp_section = NULL;
                }


                /* Now we have to find where the option is - it could either
                   be after an '=', or the next argument */
                if ( ( ptr = strchr(option,'=') ) != NULL ) {
                    *ptr = 0;
                    value = ptr + 1;
                } else if ( i + 1 < argc ) {
                    value = argv[i+1];
                    optc = i + 1;
                }

                if ( option ) {
                    if ( (ret = option_set(ctx, option,value) ) > 0 ) {
//                        argv[i] = NULL;
                        if ( ret > 0 ) {
//                            argv[optc] = NULL;
                            i = optc;
                        } else {
                            i++;
                        }
                    }
                }
                /* Restore the default section */
                if ( temp_section ) {
                    ctx->current_group = temp_section;
                }

                FREENULL(arg);
                if ( option == NULL ) {                    
                    return -1;
                }
            } else {   /* Short option handling */
                char sopt = argv[i][1];
                char *nextarg = NULL;

                if ( i + 1 < argc ) {
                    nextarg = argv[i+1];
                }

                if ( ( ret = option_set_sopt(ctx,sopt,nextarg) ) > 0 ) {
                    argv[i] = NULL;
                    argv[i + ret] = NULL;
                    i += ret;
                }
            }
        }
    }
    return 0;
}

int iniparse_file(configctx_t *ctx, char *name)
{
    FILE        *fp;
    int          ret;

    if ( ( fp = fopen(name,"r") ) == NULL ) {
        return (-1);
    }
    ret = iniread_file(ctx, fp);
    fclose(fp);
    return ret;
}

/** \brief Dump out all of the arguments to stdout - a sort of help
 *   display
 */
void iniparse_help(configctx_t *ctx, FILE *fp)
{
    char      opttext[6];
    option_t *opt = ctx->list;



    while ( opt != NULL ) {
        if ( opt->sopt ) {
            snprintf(opttext,sizeof(opttext),"-%c",opt->sopt);
        } else {
            snprintf(opttext,sizeof(opttext),"  ");
        }


        switch ( opt->type ) {
        case OPT_BOOL:
            fprintf(fp,"%s --%-23s (bool)      %s\n", opttext, opt->lopt, opt->desc);
            break;
        case OPT_STR:
        case OPT_FILENAME:
            fprintf(fp,"%s --%-23s (string)    %s\n", opttext, opt->lopt, opt->desc);
            break;
        case OPT_INT:
            fprintf(fp,"%s --%-23s (integer)   %s\n", opttext, opt->lopt, opt->desc);
            break;
        case OPT_STR|OPT_ARRAY:
        case OPT_FILENAME|OPT_ARRAY:
            fprintf(fp,"%s --%-23s (str array) %s\n", opttext, opt->lopt, opt->desc);
            break;
        case OPT_INT|OPT_ARRAY:
            fprintf(fp,"%s --%-23s (int array) %s\n", opttext, opt->lopt,opt->desc);
            break;
        }
            

        opt = opt->next;
    }    


}


int iniparse_add_array(configctx_t *ctx, char sopt, char *key2, char *desc, unsigned char type, void *data, int *num_ptr)
{
    option_t *option;
    char         *word;
    char         *key = strdup(key2);
    
    word = strchr(key,':');
    
    if ( word == NULL ) {
        free(key);
        return -1;
    }
    
    *word++ = 0;
    
    option = malloc(sizeof(option_t));
    
    option->next = NULL;

    if ( ctx->list == NULL ) {
        ctx->list = option;
    } else {
        option_t *arg = ctx->list;
        while ( arg->next != NULL )
            arg = arg->next;

        arg->next = option;
    }

    option->sopt   = sopt;
    option->lopt   = strdup(key2);
    option->desc   = strdup(desc);
    option->group  = strdup(key);
    option->word   = strdup(word);
    option->type   = type | OPT_ARRAY;
    option->value  = data;
    option->count  = num_ptr;
    free(key);
    return 0;
}

int iniparse_add(configctx_t *ctx, char sopt, char *key2, char *desc, unsigned char type, void *data)
{
    option_t     *option;
    char         *word;
    char         *key = strdup(key2);
    
    word = strchr(key,':');
    
    if ( word == NULL ) {
        free(key);
        return -1;
    }
    
    *word++ = 0;
    
    option = malloc(sizeof(option_t));
    option->next = NULL;
   
    if ( ctx->list == NULL ) {
        ctx->list = option;
    } else {
        option_t *arg = ctx->list;
        while ( arg->next != NULL )
            arg = arg->next;

        arg->next = option;
    }   
    
    option->sopt   = sopt;
    option->lopt   = strdup(key2);
    option->desc   = strdup(desc);
    option->group  = strdup(key);
    option->word   = strdup(word);
    option->type   = type;
    option->value  = data;
#if 0
    switch ( type ) {
    case OPT_BOOL:
        *(char *)data = 0;
        break;
    case OPT_INT:
        *(int *) data = 0;
        break;
    case OPT_STR:
        *(char **) data = NULL;
        break;
    }
#endif
    free(key);
    return 0;
}

static int isoptionchar( const char c )
{
    return (isalnum(c) || ('-' == c) || ('_' == c) || ('.' == c));
}

/** \brief
 *
 *  \param ctx
 *  \param fp
 *
 *  \return Number of options set
 */
int iniread_file(configctx_t *ctx, FILE *fp)
{
    char        buffer[LIBINI_LINE_MAX];
    char       *start,*ptr;
    int         ret = 0;
    
    while (fgets(buffer,sizeof(buffer),fp) != NULL ) {
        start = skpws(buffer);
        if ( *start == ';' || *start =='#' || *start ==0)
            continue;
        if ( *start == '[' ) {
            get_section(ctx, start);
        } else {        /* Must be an option! */
            ptr = start;
            while ( isoptionchar(*ptr) )
                ptr++;
            if ( *ptr == '=' ) {
                *ptr++ = 0;
            } else {
                *ptr++ = 0;
                /* Search for the '=' now */
                if ( (ptr = strchr(ptr,'=') ) == NULL ) {
                    continue;
                }
                ptr++;
            }

            ptr = skpws(ptr); /* Skip over any white space */
            if ( *ptr == ';' || *ptr =='#' || *ptr ==0 ) {
                continue;
            }
            if ( strlen(start) ) {
                if ( option_set(ctx, start,ptr) >= 0 ) {
                    ret++;
                }
            }
        }
    }
    return ret;
}



static int option_set(configctx_t *ctx, char *opt, char *it)
{
    option_t    *option;
    

    if ( ctx->do_cache == 0 ) {
        option = ctx->list;
        while ( option != NULL ) {
            if ( strcmp(option->group,ctx->current_group) == 0 &&
                 strcmp(option->word,opt) == 0 ) {
                if ( option->type == OPT_BOOL && it == NULL ) {
                    *(char *)(option->value) = 1;
                    return 0;
                } else {
                    option_do_set(option,it);
                }
                return 1;
            }
            option = option->next;
        }
    } else {
        /* Caching */
        iniparse_cache_set(ctx,opt,it,0);
        return 1;
    }
    return -1;
}



static int option_set_sopt(configctx_t *ctx, char sopt, char *it)
{
    option_t    *option;
    
    option = ctx->list;

    while ( option != NULL ) {
        if ( option->sopt == sopt ) {
            if ( option->type == OPT_BOOL ) {
                *(char *)(option->value) = 1;
                return 0;
            } else {
                option_do_set(option,it);
            }
            return 1;
        }
        option = option->next;
    }
    return -1;
}

static int parse_int(char *value)
{
    int multiplier = 1;
    char *ptr;

    if ( (ptr = strstr(value,"0x") ) != NULL ) {
        return strtoul(ptr+2,NULL,16);
    }


    ptr = strchr(value,'k');
    if (!ptr)
        ptr = strchr(value,'K');
    if (!ptr)
        ptr = strchr(value,'m');
    if (!ptr)
        ptr = strchr(value,'M');

    if (ptr)
    {
        switch (*ptr)
        {
            case 'k': case 'K':
                multiplier = 1024;
                break;
            case 'm': case 'M':
                multiplier = 1024*1024;
                break;
        }
    }

    return atoi(value) * multiplier;
}

static double parse_float(char *value)
{
    double multiplier = 1.0;
    double val = 0;
    char *ptr;

    ptr = strchr(value,'k');
    if (!ptr)
        ptr = strchr(value,'K');
    if (!ptr)
        ptr = strchr(value,'m');
    if (!ptr)
        ptr = strchr(value,'M');
    if (!ptr)
        ptr = strchr(value,'%');

    if (ptr)
    {
        switch (*ptr)
        {
            case 'k': case 'K':
                multiplier = 1024.0;
                break;
            case 'm': case 'M':
                multiplier = 1024.0*1024.0;
                break;
            case '%':
                multiplier = 1.0/100.0;
                break;
        }
    }

    val = atof(value) * multiplier;
    
    return val;
}

static void option_do_set(option_t *option, char *value)
{
    char        expand[FILENAME_MAX+1];
    int         val;
    double      val2;
    char        c;
    char       *ptr;
    char       *temp;

    /* Handle comments after the value */
    ptr = strchr(value,';');
    if ( ptr )
        *ptr = 0;

    if ( option->type & OPT_ARRAY ) {
        switch ( option->type & ~OPT_ARRAY ) {
        case OPT_INT:
            val = parse_int(value);
            *(int **)option->value = realloc(*(int **)option->value, (*option->count + 1) * sizeof(int));
            (*(int **)(option->value))[*option->count] = val;
            (*option->count)++;
            break;
        case OPT_STR:
        case OPT_FILENAME:
            strip_ws(value);
            if ( (option->type & ~OPT_ARRAY) == OPT_FILENAME ) {
                filename_expand(value,expand,sizeof(expand),NULL,NULL);
                temp = strdup(expand);
            } else {
                temp = strdup(value);
            }
            *(char **)option->value = realloc(*(char **)option->value, (*option->count + 1) * sizeof(char *));
            (*(char ***)(option->value))[*option->count] = temp;
            (*option->count)++;
            break;
        case OPT_FLOAT:
            val2 = parse_float(value);
            *(double **)option->value = realloc(*(double **)option->value, (*option->count + 1) * sizeof(double));
            (*(double **)(option->value))[*option->count] = val2;
            (*option->count)++;
            break;
        }
        return;

    }
    switch ( option->type ) {
    case OPT_INT:
        val = parse_int(value);
        *(int *)(option->value) = val;
        return;
    case OPT_STR:
    case OPT_FILENAME:
        strip_ws(value);
        if ( option->type == OPT_FILENAME ) {
            filename_expand(value,expand,sizeof(expand),NULL,NULL);
            temp = strdup(expand);
        } else {
            temp = strdup(value);
        }
        *(char **)(option->value) = temp;
        return;
    case OPT_BOOL:
        c = toupper(value[0]);
        val = 0;
        switch (c) {
        case 'Y':
        case 'T':
        case '1':
            val = 1;
        }
        *(char *)(option->value) = val;
        return;
    case OPT_FLOAT:
        val2 = parse_float(value);
        *(double *)(option->value) = val2;
        return;
    }
}



/* enters in with ptr pointing to the '[' */

static void get_section(configctx_t *ctx, char *ptr)
{
    char *end; 
    end = strchr(ptr,']');
    
    if ( end == NULL )
        return;
    *end = 0;
   
    free(ctx->current_group);
    ctx->current_group = strdup(ptr+1);
}


static void strip_ws(char *value)
{
    value = value+strlen(value)-1;
    while (*value && isspace(*value)) 
        *value-- = 0;
}

static char *skpws(char *ptr)
{
        while (*ptr && isspace(*ptr) )
                ptr++;
        return (ptr);
}

/* Caching functionality below */

static cache_t *iniparse_cache_find(configctx_t *ctx, char *group, char *option)
{
    cache_t        *cache = ctx->cache;

    while ( cache ) {
        if ( strcmp(cache->group,group) == 0 && strcmp(cache->word,option) == 0 ) {
            return cache;
        }
        cache = cache->next;
    }
    return NULL;
}

void iniparse_cache_add(configctx_t *ctx, char *option, int type, void *dest_ptr, int *dest_num)
{
    char          buf[2048];
    int           total = 1;
    int           i;
    char         *key = strdup(option);
    char         *word;
    
    word = strchr(key,':');
    
    if ( word == NULL ) {
        free(key);
        return;
    }
    
    *word++ = 0;

    free(ctx->current_group);
    ctx->current_group = strdup(key);
    
    if ( dest_num != NULL ) {
        total = *dest_num;
    }

    for ( i = 0; i < total; i++ ) {           
        switch ( type ) {
        case OPT_STR:
        case OPT_FILENAME:
            if ( dest_num != NULL ) {
                snprintf(buf,sizeof(buf),"%s",(*((char ***)dest_ptr))[i]);
            } else {
                snprintf(buf,sizeof(buf),"%s", *((char **)(dest_ptr)));
            }
            break;
        case OPT_INT:
            if ( dest_num != NULL ) {
                snprintf(buf,sizeof(buf),"%d",(((int *)dest_ptr)[i]));
            } else {
                snprintf(buf,sizeof(buf),"%d",*(int *)dest_ptr);
            }
            break;
        case OPT_FLOAT:
            if ( dest_num != NULL ) {
                snprintf(buf,sizeof(buf),"%lf",(((double *)dest_ptr)[i]));
            } else {
                snprintf(buf,sizeof(buf),"%lf",*(double *)dest_ptr);
            }
            break;
        }
        iniparse_cache_set(ctx, word, buf, i == 0 ? 1 : 0);
    }
    free(key);
}

void iniparse_cache_set(configctx_t *ctx, char *option, char *value, char overwrite)
{
    cache_t        *cache = ctx->cache;
    int             i;
 
    strip_ws(value);

    if  ( ( cache = iniparse_cache_find(ctx,ctx->current_group,option) ) != NULL ) {
        /* If replacing, then remove the old cache value */
        if ( overwrite == 1 ) {
            cache_t *arg = ctx->cache;

            if ( arg == cache ) {
                ctx->cache = cache->next;
            } else {
                while ( arg->next != cache ) {
                    arg = arg->next;
                }
                arg->next = cache->next;
            }

            free(cache->group);
            free(cache->word);
            for ( i = 0; i < cache->num_values; i++ ) {
                free(cache->values[i]);
            }
            free(cache);
            cache = NULL;
        }       
    } 

    if ( cache == NULL ) {
        cache = calloc(1,sizeof(*cache));
        cache->group = strdup(ctx->current_group);
        cache->word = strdup(option);
        cache->num_values = 0;
        cache->values = NULL;
        cache->next = NULL;

        if ( ctx->cache == NULL ) {
            ctx->cache = cache;
        } else {
            cache_t *arg = ctx->cache;
            while ( arg->next != NULL )
                arg = arg->next;
            
            arg->next = cache;
        }   

    }

    cache->values = realloc(cache->values, (cache->num_values + 1) * sizeof(char *));
    cache->values[cache->num_values] = strdup(value);
    cache->num_values++;

    return;
}



int iniparse_cache_extract(configctx_t *ctx, char *key2, unsigned char type, void *ptr)
{
    option_t      option;
    cache_t      *cache;
    char         *key = strdup(key2);
    char         *word;
    int           ret = -1;
    

    word = strchr(key,':');
    
    if ( word == NULL ) {
        free(key);
        return ret;
    }
    
    *word++ = 0;
    
    /* key contains the group, word contains the option */
    if ( ( cache = iniparse_cache_find(ctx,key,word) ) != NULL ) {
        ret = 1;
        option.value = ptr;
        option.type = type;
        option_do_set(&option, cache->values[cache->num_values-1]);
    }

    free(key);
    return ret;
}

int iniparse_cache_extract_array(configctx_t *ctx, char *key2, unsigned char type, void *ptr, int *num_ptr)
{
    option_t      option;
    cache_t      *cache;
    char         *word;
    char         *key = strdup(key2);
    int           ret = -1;
    int           i;
    
    word = strchr(key,':');
    
    if ( word == NULL ) {
        free(key);
        return ret;
    }
    
    *word++ = 0;
    
    /* key contains the group, word contains the option */
    if ( ( cache = iniparse_cache_find(ctx,key,word) ) != NULL ) {
        ret = cache->num_values;
        option.value = ptr;
        option.count = num_ptr;
        option.type = type | OPT_ARRAY;
        for ( i = 0; i < cache->num_values; i++ ) {
            option_do_set(&option, cache->values[i]);
        }
    }
    free(key);
    return ret;

}

/** \brief Write a cache to disc
 *
 *  \param ctx Configuration context
 *  \param filename Filename
 *
 *  \return Number of options written */
int iniparse_cache_write(configctx_t *ctx, char *filename)
{
    cache_t        *cache = ctx->cache;
    char           *last = NULL;
    FILE           *fp = NULL;
    int             ret = 0;
    int             i;

    while ( cache ) {
        if ( fp == NULL ) {
            if ( ( fp = fopen(filename,"w") ) == NULL ) {
                fp = stdout;
            }
        }
        if ( last == NULL || strcmp(cache->group,last) ) {
            last = cache->group;
            fprintf(fp,"\n[%s]\n",cache->group);
        }
        for ( i = 0; i < cache->num_values; i++ ) {
            fprintf(fp,"%s = %s\n",cache->word,cache->values[i]);
        }
        ret++;
        cache = cache->next;
    }
    if ( fp ) {
        fclose(fp);
    }
    return ret;
}


#ifdef TEST
int main(int argc, char *argv[])
{
    char *ptr;
    int   ret;
    unsigned int i;
    char   **arr = NULL;
    int    arr_num = 0;
    int    arr_int[] = { 1, 2, 3, 4};
    int   *arr_ptr;
    char  *arr_str[] = { "hello", "bye", "tea", "cake" };
    char  **arr_ptr2 = arr_str;
    char  *string = "test";
    configctx_t *ctx;

    ctx = iniparse_init("test");
    iniparse_add(ctx, 0,"test:blah","Bah",OPT_STR,&ptr);
    iniparse_add(ctx, 'c',"test:count","Humbug",OPT_INT,&i);
    iniparse_add_array(ctx, 0,"test:array","Array",OPT_STR,&arr,&arr_num);

    iniparse_help(ctx, stdout);

    ret = iniparse(ctx, "test.ini",argc,argv);
    iniparse_cleanup(ctx);
    printf("%lu \n",i);
    printf("%d \n",arr_num);

    for ( i = 0; i < arr_num; i++ ) {
        printf("%d %s\n",i,arr[i]);
    }

    ctx = iniparse_cache_init();
    iniparse_cache_add(ctx,"test:int",OPT_INT,&arr_int[0],&arr_int[3]);
    iniparse_cache_add(ctx,"test:str",OPT_STR,&arr_ptr2,&arr_int[3]);
    iniparse_cache_add(ctx,"test:array",OPT_STR,&arr,&arr_num);
    iniparse_cache_add(ctx, "test:_int",OPT_INT,&arr_int[0],NULL);
    iniparse_cache_add(ctx, "test:_str",OPT_STR,&arr_str[0],NULL);

    iniparse_cache_write(ctx,"test2.ini2");
    arr = NULL;
    arr_num = 0;
    i = 0;
    ptr = NULL;

    iniparse_cache_extract(ctx,"test:blah",OPT_STR,&ptr);
    iniparse_cache_extract(ctx,"test:count",OPT_INT,&i);
    iniparse_cache_extract_array(ctx,"test:array",OPT_STR,&arr,&arr_num);
    
    printf("%lu \n",i);
    printf("%d \n",arr_num);

    for ( i = 0; i < arr_num; i++ ) {
        printf("%d %s\n",i,arr[i]);
    }

}
#endif


