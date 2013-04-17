#!/usr/bin/env python

from MySQLdb import connect

"""
Main function
"""
def main ():
	# Assume a MySQL fresh install.
	user = root
	
	try:
		db_conn = connect(user=user, passwd="", db="ichoppedthatvideo")
	except:
		print "** FATAL ERROR ** Could not connect to database. Aborting..."
		raise
		
	cursor = db_conn.cursor()
	cursor.execute("SET PASSWORD FOR 'root'@'localhost' = PASSWORD('bleh')")
	cursor.execute("SET PASSWORD FOR 'root'@'localhost.localdomain' = PASSWORD('bleh')")
	cursor.execute("SET PASSWORD FOR 'root'@'127.0.0.1' = PASSWORD('bleh')")
	cursor.execute("CREATE DATABASE ichoppedthatvideo")
	cursor.execute("USE ichoppedthatvideo")
	cursor.execute("create user 'ichoppedthatvideo_server'@'localhost' identified by 'bleh'")
	cursor.execute("grant all privileges on ichoppedthatvideo.* to 'ichoppedthatvideo_server'@'localhost' with grant option")

	cursor.execute("""
		create table requests (id bigint unsigned not null auto_increment, client_id char(40) not null, server_id varchar(30) not null, child_id smallint unsigned not 			null, type varchar(20), browser varchar(40), opsys varchar(40), city varchar(127), country varchar(127), cur_timestamp timestamp(8), primary key (id), unique 			idx_request (client_id, server_id, child_id, cur_timestamp))
	""")

	cursor.execute("""
		create table streams (id bigint(20) unsigned not null auto_increment, request_id bigint(20) unsigned not null, video_id int(10) unsigned not null, seconds int(10) 			unsigned not null, primary key (id));
	""")


if __name__ == "__main__":
    main()
