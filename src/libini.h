/*
 *   Current cost daemon - configuration library
 *   http://www.rst38.org.uk/currentcost
 *
 *   Copyright (C) 2005,2010 Dominic Morris
 *
 *   $Id: libini.h,v 1.1 2008/01/02 22:48:37 dom Exp $
 *   $Date: 2008/01/02 22:48:37 $
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
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
 */


#ifndef LIBINI_H
#define LIBINI_H




    
/* Available option types */
#ifndef OPT_INT
#define OPT_INT         0
#define OPT_STR         1
#define OPT_BOOL        2
#define OPT_FLOAT       3
#define OPT_FILENAME    4
#endif
#define OPT_ARRAY       64


typedef struct _configctx configctx_t;

/* Call this first to initialise the structures */
extern configctx_t *iniparse_init(char *default_section);
extern configctx_t *iniparse_cache_init();

extern int          iniparse_add(configctx_t *ctx, char sopt, char *key, char *desc, unsigned char type, void *ptr);
extern int          iniparse_add_array(configctx_t *ctx, char sopt, char *key, char *desc, unsigned char type, void *ptr, int *num_ptr);





/* Call this to parse the file and fill in the options */
extern int          iniparse(configctx_t *ctx, char *filename, int argc, char *argv[]);
extern int          iniparse_args(configctx_t *ctx, int argc, char *argv[]);
extern int          iniparse_file(configctx_t *ctx, char *filename);

/* When you've got all your options, call this to cleanup afterwards */
extern void         iniparse_cleanup(configctx_t *ctx);

extern void         iniparse_help(configctx_t *ctx, FILE *fpout);

extern void         iniparse_cache_add(configctx_t *ctx, char *option, int type, void *dest_ptr, int *dest_num);
extern void         iniparse_cache_set(configctx_t *ctx, char *option, char *value, char overwrite);
extern int          iniparse_cache_write(configctx_t *ctx, char *filename);
extern int          iniparse_cache_extract(configctx_t *ctx, char *key2, unsigned char type, void *ptr);
extern int          iniparse_cache_extract_array(configctx_t *ctx, char *key2, unsigned char type, void *ptr, int *num_ptr);

/* This function must be implemented by the calling application */
extern char        *filename_expand(char *format, char *buf, size_t buflen, char *i_option, char *k_option);

#endif /* __LIBINI_H_ */
