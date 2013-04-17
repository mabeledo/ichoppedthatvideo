#!/usr/bin/env python

from optparse import OptionParser
from MySQLdb import connect
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from datetime import date, timedelta
import smtplib
import socket

"""
Main function
"""
def main ():
	# Read user and password from a file, open a DB connection and
	# execute the count query.
	try:
		fd = open("/etc/ichoppedthatvideo/passwd")
		for line in fd:
			if line.find("ichoppedthatvideo=") > 0:
				break
	except IOError:
		print "Error opening passwd file. Aborting..."
		raise
	finally:
		fd.close()

	user = line[line.find('=') + 1:line.find(',')]
	passwd = line[line.find(',') + 1:len(line) - 2]
	
	try:
		db_conn = connect(user=user, passwd=passwd, db="ichoppedthatvideo")
	except:
		print "** FATAL ERROR ** Could not connect to database. Aborting..."
		raise
		
	cursor = db_conn.cursor()
	cursor.execute("SELECT streams.video_id, COUNT(*) video_count FROM streams INNER JOIN requests ON streams.request_id = requests.id WHERE (DATEDIFF(CURRENT_TIMESTAMP(), requests.cur_timestamp) = 1) GROUP BY streams.video_id")
	video_counts = cursor.fetchall()
	
	# End of the DB part.
	# Proceed to compose the email message.
	fromAddr = "server@ichoppedthat.video"
	toAddr = ['galicia@ichoppedthat.video']
	hostname = socket.gethostname()
	msg = MIMEMultipart("alternative")
	msg["Subject"] = "Daily stats from " + hostname
	msg["From"] = fromAddr
	msg["To"] = ','.join(toAddr)
	cur_date = (date.today() - timedelta(1)).isoformat()
	
	plain = 'Number of streams served on ' + cur_date + ' in server ' + hostname + '\n\nvideo ID\tDaily total\n\n'
	html = '<html><head>Daily requests stats from server</head><body>' + '<br /><br />Number of streams served on <b>' + cur_date + '</b> in server <b>' + hostname + '</b><br /><br /><table border="0" cellpadding="5" cellspacing="5"><tr><td align="center"><b>video ID</b></td><td align="center"><b>Daily total</b></td></tr>'

	for video_count in video_counts:
		plain += str(video_count[0]) + '\t' + str(video_count[1]) + '\n'
		html += '<tr><td align="center">' + str(video_count[0]) + '</td><td align="center">' + str(video_count[1]) + '</td></tr>'

	text_part = MIMEText(plain, 'plain')
	html_part = MIMEText(html, 'html')
	msg.attach(text_part)
	msg.attach(html_part)
	
	smtp_conn = smtplib.SMTP('localhost')
	smtp_conn.sendmail(fromAddr, toAddr, msg.as_string())
	smtp_conn.quit()

if __name__ == "__main__":
    main()
