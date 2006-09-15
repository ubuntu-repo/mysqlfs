/*
  mysqlfs - MySQL Filesystem
  Copyright (C) 2006 Tsukasa Hamano <code@cuspy.org>
  $Id: pool.c,v 1.8 2006/09/15 04:10:43 ludvigm Exp $

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql/mysql.h>
#include <pthread.h>

#include "pool.h"
#include "log.h"

static pthread_mutex_t mysql_pool_mutex=PTHREAD_MUTEX_INITIALIZER;

MYSQL_POOL *mysqlfs_pool_init(MYSQLFS_OPT *opt)
{
    int i;
    MYSQL_POOL *pool;
    MYSQL *ret;
    my_bool reconnect = 1;

    pool = malloc(sizeof(MYSQL_POOL));
    if(!pool){
        return NULL;
    }

    pool->num = opt->connection;
    if(pool->num < 0){
        return NULL;
    }

    pool->use = malloc(sizeof(int) * pool->num);
    if(!pool->use){
        free(pool);
        return NULL;
    }

    pool->conn = malloc(sizeof(MYSQL_CONN) * pool->num);
    if(!pool->conn){
        free(pool->use);
        free(pool);
        return NULL;
    }

    for(i=0; i<pool->num; i++){
        pool->conn[i].id = i;
        pool->use[i] = 0;

        pool->conn[i].mysql = mysql_init(NULL);
        if(!pool->conn[i].mysql){
            break;
        }

	if (!opt->mycnf_group)
	    opt->mycnf_group = "mysqlfs";

	mysql_options(pool->conn[i].mysql, MYSQL_READ_DEFAULT_GROUP,
		      opt->mycnf_group);

        ret = mysql_real_connect(
            pool->conn[i].mysql, opt->host, opt->user,
            opt->passwd, opt->db, opt->port, opt->socket, 0);
        if(!ret){
            log_printf(LOG_ERROR, "ERROR: mysql_real_connect(): %s\n",
		       mysql_error(pool->conn[i].mysql));
            return NULL;
        }

	/* Reconnect must be set *after* real_connect()! */
	mysql_options(pool->conn[i].mysql, MYSQL_OPT_RECONNECT,
		      (char*)&reconnect);
    }

    /* Check the server version and some required records.  */
    unsigned long mysql_version;
    mysql_version = mysql_get_server_version(pool->conn[0].mysql);
    if (mysql_version < MYSQL_MIN_VERSION) {
    	fprintf(stderr, "Your server version is %s. "
    		"Version %lu.%lu.%lu or higher is required.\n",
    		mysql_get_server_info(pool->conn[0].mysql), 
    		MYSQL_MIN_VERSION/10000L,
    		(MYSQL_MIN_VERSION%10000L)/100,
    		MYSQL_MIN_VERSION%100L);
    	return NULL;
    }
        
    return pool;
}

int mysqlfs_pool_free(MYSQL_POOL *pool)
{
    int i;
    
    for(i=0; i<pool->num; i++){
        mysql_close(pool->conn[i].mysql);
    }

    if(pool->conn){
        free(pool->conn);
    }

    if(pool->use){
        free(pool->use);
    }

    if(pool){
        free(pool);
    }

    return 0;
}

MYSQL_CONN *mysqlfs_pool_get(MYSQL_POOL *pool)
{
    int i;
    MYSQL_CONN *conn = NULL;

    pthread_mutex_lock(&mysql_pool_mutex);

    for(i=0; i<pool->num; i++){
        if(!pool->use[i]){
//            log_printf("get connecttion %d\n", i);
            pool->use[i] = 1;
            conn = &(pool->conn[i]);
            break;
        }
    }

    pthread_mutex_unlock(&mysql_pool_mutex);

    log_printf(LOG_D_POOL, "%s() => %d\n", __func__, i >= pool->num ? -1 : i);
    if (!conn)
      mysqlfs_pool_print(pool);

    return conn;
}

int mysqlfs_pool_return(MYSQL_POOL *pool, MYSQL_CONN *conn)
{
    log_printf(LOG_D_POOL, "%s() <= %d\n", __func__, conn->id);

    pthread_mutex_lock(&mysql_pool_mutex);
    pool->use[conn->id] = 0;
    pthread_mutex_unlock(&mysql_pool_mutex);

    return 0;
}

void mysqlfs_pool_print(MYSQL_POOL *pool)
{
    int i;

    pthread_mutex_lock(&mysql_pool_mutex);
    
    for(i=0; i<pool->num; i++){
        log_printf(LOG_D_POOL, "pool->use[%d] = %d\n", i, pool->use[i]);
    }

    pthread_mutex_unlock(&mysql_pool_mutex);

    return;
}
