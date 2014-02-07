#include <mysql.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "EpollServer.h"
#include "ObjectQueue.h"
#include "Connection.h"
#include "DBConnection.h"

void DBConnection::connectDB()
{
	const char *server = "localhost";
	const char *user = "root";
	const char *password = "314159"; /* set me first */
	const char *database = "mysql";
	mConnection = mysql_init(NULL);
	/* Connect to database */
	if (!mysql_real_connect(mConnection, server,
				user, password, database, 0, NULL, 0)) {
		fprintf(stderr, "%s\n", mysql_error(mConnection));
		abort();
	}
}

void DBConnection::processMessage(const char *msg, size_t len)
{
	MYSQL_RES *res;
	MYSQL_ROW row;
	/* send SQL query */
	if (mysql_query(mConnection, "show tables")) {
		fprintf(stderr, "%s\n", mysql_error(mConnection));
		exit(1);
	}
	res = mysql_store_result(mConnection);
	/* output table name */
	printf("MySQL Tables in mysql database:\n");
	while ((row = mysql_fetch_row(res)) != NULL)
		printf("%s \n", row[0]);
	mysql_free_result(res);
}

void DBConnection::closeConnection()
{
	/* close connection */
	mysql_close(mConnection);
}

