/*
  (C) 2006 Z RESEARCH Inc. <http://www.zresearch.com>
  
  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License, or (at your option) any later version.
    
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.
    
  You should have received a copy of the GNU General Public
  License along with this program; if not, write to the Free
  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301 USA
*/ 

#include "io-cache.h"


/*
 * str_to_ptr - convert a string to pointer
 * @string: string
 *
 */
void *
str_to_ptr (char *string)
{
  void *ptr = (void *)strtoul (string, NULL, 16);
  return ptr;
}


/*
 * ptr_to_str - convert a pointer to string
 * @ptr: pointer
 *
 */
char *
ptr_to_str (void *ptr)
{
  char *str;
  asprintf (&str, "%p", ptr);
  return str;
}

/* 
 * ioc_inode_update - create a new ioc_inode_t structure and add it to 
 *                    the table table. fill in the fields which are derived 
 *                    from inode_t corresponding to the file
 * 
 * @table: io-table structure
 * @inode: inode structure
 *
 * not for external reference
 */
ioc_inode_t *
ioc_inode_update (ioc_table_t *table, 
		  inode_t *inode)
{
  ioc_inode_t *ioc_inode = calloc (1, sizeof (ioc_inode_t));
  
  /* If mandatory locking has been enabled on this file,
     we disable caching on it */
  if ((inode->buf.st_mode & S_ISGID) && !(inode->buf.st_mode & S_IXGRP))
    ioc_inode->disabled = 1;
  
  ioc_inode->size = inode->buf.st_size;
  ioc_inode->table = table;
  ioc_inode->inode = inode;

  /* initialize the list for pages */
  INIT_LIST_HEAD (&ioc_inode->pages);
  INIT_LIST_HEAD (&ioc_inode->page_lru);

  list_add (&ioc_inode->inode_list, &table->inodes);
  list_add_tail (&ioc_inode->inode_lru, &table->inode_lru);

  pthread_mutex_init (&ioc_inode->inode_lock, NULL);
  
  return ioc_inode;
}

/*
 * ioc_inode_search - search for a ioc_inode in the table.
 *
 * @table: io-table structure
 * @inode: inode_t structure
 *
 * not for external reference
 */
ioc_inode_t *
ioc_inode_search (ioc_table_t *table,
		  inode_t *inode)
{
  ioc_inode_t *ioc_inode = NULL;
  
  list_for_each_entry (ioc_inode, &table->inodes, inode_list){
    if (ioc_inode->inode->ino == inode->ino)
      return ioc_inode;
  }

  return NULL;
}



/*
 * ioc_inode_ref - io cache inode ref
 * @inode:
 *
 */
ioc_inode_t *
ioc_inode_ref (ioc_inode_t *inode)
{
  inode->refcount++;
  return inode;
}



/*
 * ioc_inode_ref_locked - io cache inode ref
 * @inode:
 *
 */
ioc_inode_t *
ioc_inode_ref_locked (ioc_inode_t *inode)
{
  ioc_inode_lock (inode);
  inode->refcount++;
  ioc_inode_unlock (inode);
  return inode;
}

/* 
 * ioc_inode_destroy -
 * @inode:
 *
 */
void
ioc_inode_destroy (ioc_inode_t *ioc_inode)
{
  ioc_table_t *table = ioc_inode->table;
  ioc_page_t *curr = NULL, *prev = NULL;

  if (ioc_inode->refcount)
    return -1;

  ioc_table_lock (table);
  list_del (&ioc_inode->inode_list);
  list_del (&ioc_inode->inode_lru);
  ioc_table_unlock (table);
  
  /* free all the pages of this inode cached */
  list_for_each_entry (curr, &ioc_inode->pages, pages) {
    if (prev) {
      ioc_page_destroy (prev);
      list_del (&prev->pages);
      free (prev);
    }
    prev = curr;
  }
  
  /* free the last page */
  if (prev) {
    ioc_page_destroy (prev);
    list_del (&prev->pages);
    free (prev);
  }

  pthread_mutex_destroy (&ioc_inode->inode_lock);
  free (ioc_inode);
}

/*
 * ioc_inode_unref_locked -
 * @inode:
 *
 */
void
ioc_inode_unref_locked (ioc_inode_t *inode)
{
  int32_t refcount;

  refcount = --inode->refcount;

  if (refcount)
    return;

}

/*
 * ioc_inode_unref - unref a inode
 * @inode:
 *
 */
void
ioc_inode_unref (ioc_inode_t *inode)
{
  int32_t refcount;

  ioc_inode_lock (inode);
  refcount = --inode->refcount;
  ioc_inode_unlock (inode);

  if (refcount)
    return;

}
