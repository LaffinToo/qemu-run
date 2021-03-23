/*
 * test.c
 *
 * Copyright 2021 Luis "Laffin" Espinoza <laffintoo at gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ERR
#define ERR (0)
#define OK  !ERR
#endif
/**
 * @brief Symbols Table
 *
 *
 */
typedef struct st_symbols {
   struct st_symbols    *next;   /**< Link List Next Element */
   long                  hash;   /**<  Hash Value*/
   char                 *key,    /**<  Symbol Name */
                        *val;    /**<  Symbol Value */
} st_symbols;

/**
 * @brief Hash Table
 *
 *
 */
typedef struct st_hashmap {
   long         hash;   /**<  Hash Value*/
   st_symbols  *symbol; /**<  Pointer to Symbol */
} st_hashmap;

/**
 * @brief Holds symbol & hashmap pointers
 *
 *
 *
 */
typedef struct st_syminfo {
   int          count;  /**< Number of elements in Symbol Table */
   st_symbols  *symbols,*cur;   /**<  Pointer to symbols link list*/
   st_hashmap  *hashmap;   /**< Pointer to hashmap */
} st_syminfo;

/**
 * @brief Keeps track of symbol info, creates if necessary
 * @returns symbol info
 *
 *
 */
st_symbols *sym_base(void)
{
   static st_syminfo *base=NULL;
   if(base==NULL) {
     base=calloc(1,sizeof(st_syminfo));
   }
   return base;
}

/**
 * @brief Find a symbol by its hash
 * @param hash - symbol hash we looking for
 * @return     - pointer to symbol
 * @return     - NULL if not found
 *
 *
 */
st_symbols *sym_find_hash(long hash)
{
   int count=0;
   st_syminfo *si;
   st_symbols *sym=NULL;

   si=sym_base();
   while(count<si->count) {
      if(si->hashmap[count].hash==hash) {
         sym=si->hashmap[count].symbol;
         break;
      }
      count++;
   }
   return sym;
}

/**
 * @brief Generate a hash based on string characters
 * @param str  - string to generare hash from
 * @returns    - hash value
 *
 *
 */
long sym_hash_generate(char *str,int exists)
{
   int pos;
   long hash=0xCAFEBABE;
   for(;;) {
      pos=0;
      while(str[pos]) {
         hash=~
            (((((hash&0xFF)^str[pos])<<5)|
            (((hash>>26)&0x1f)^(pos&31)))|
            (hash<<18));
         pos++;
      }
      if(!exists && sym_find_hash(hash)) {
         printf("Hash Collision (%s)\n",str);
         exit(1);
         continue;
      } else if(exists)
         hash=sym_find_hash(hash)?hash:0;
      break;
   }
   return hash;
}


st_symbols *sym_find_key(char *name)
{
   long hash;

   hash=sym_hash_generate(name,1);

   return hash?sym_find_hash(hash):NULL;
}

int sym_add(char *key,char *val)
{
   static const char* empty_str="";
   long hash;
   st_syminfo *si;
   st_symbols *sym;
   if(!val) val = empty_str;
   si=sym_base();
   hash=sym_hash_generate(key,0);
   if(!si->count) {
      sym=si->symbols=calloc(1,sizeof(st_symbols));
      if(sym)
         si->hashmap=calloc(1,sizeof(st_hashmap));
   } else {
      sym=si->hashmap[si->count-1].symbol;
      sym->next=calloc(1,sizeof(st_symbols));
      if(sym->next) {
         si->hashmap=realloc(si->hashmap,sizeof(st_hashmap)*(si->count+1));
         if(!si->hashmap) {
            free(sym->next);
            sym=sym->next=NULL;
         } else
            sym=sym->next;

      }
   }
   if(!sym) return !OK;
   si->hashmap[si->count].symbol=sym;
   sym->hash=si->hashmap[si->count].hash=hash;
   sym->key=strdup(key);
   sym->val=strdup(val);
   si->count++;
   return OK;
}

int sym_set(char *key,char *val)
{
   int ret=OK;

   st_symbols *sym;
   if(sym=sym_find_key(key)) {
      if(sym->val)
         free(sym->val);
      sym->val=strdup(val);
   } else {
      ret=sym_add(key,val);
   }
   return ret;
}

char *sym_get(char *key)
{
   st_symbols *sym;
   return ((sym=sym_find_key(key))?sym->val:NULL);
}

st_symbols *sym_first(void)
{
   st_syminfo *si;
   si=sym_base();
   si->cur=si->symbols;
   return si->cur;
}

st_symbols *sym_next(void)
{
   st_syminfo *si;
   si=sym_base();
   if(si->cur!=NULL)
      si->cur=si->cur->next;
   return si->cur;
}
