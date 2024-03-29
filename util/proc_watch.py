#!/usr/bin/env python

from optparse import OptionParser
from ConfigParser import RawConfigParser
from subprocess import Popen, PIPE
from email.mime.multipart import MIMEMultipart
from email.mime.text import MIMEText
from datetime import date, timedelta
import smtplib
import socket


"""
send_mail function
"""
def send_mail(app_name, mail_addresses):
    # Proceed to compose the email message.
    fromAddr = ''
    toAddr = mail_addresses
    hostname = socket.gethostname()
    msg = MIMEMultipart("alternative")
    msg["Subject"] = "Daily stats from " + hostname
    msg["From"] = fromAddr
    msg["To"] = ','.join(toAddr)
    cur_date = (date.today() - timedelta(1)).isoformat()
	
    plain = 'Number of streams served on ' + cur_date + ' in server ' + hostname + '\n\nvideo ID\tDaily total\n\n'
    html = '<html><head>Daily requests stats</head><body>' + '<br /><br />Number of streams served on <b>' + cur_date + '</b> in server <b>' + hostname + '</b><br /><br /><table border="0" cellpadding="5" cellspacing="5"><tr><td align="center"><b>video ID</b></td><td align="center"><b>Daily total</b></td></tr>'

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
    

"""
is_app_running function
"""
def is_app_running(name):
    ret = Popen(['/bin/ps', '-C', name, '-o', 'pid='], stdout=PIPE).communicate()[0]
    pass



"""
restart_app function
"""
def restart_app(name, parameters):
    pass

"""
main function
"""
def main ():
    # Parse command line options.
    # This options superseed those in the config file, if available. 
    cli_parser = OptionParser()
    cli_parser.add_option('-V', '--version',
                      action='store_true', dest='version', default=False,
                      help='Shows the current version')
    cli_parser.add_option('-D', '--daemonize',
                      action='store_true', dest='daemonize', default=False,
                      help='Puts program into background')
    cli_parser.add_option('-t', '--time-lapse',
                      action='store', type='int', dest='time_lapse', default=60,
                      help='Time between process checks')
    cli_parser.add_option('-m', '--mail-addresses',
                      action='store', type='string', dest='mail_addresses',
                      help='Send mail alerts to one or more mail addresses')
    cli_parser.add_option('-c', '--config-file',
                      action='store', type='string', dest='config_file', default='/etc/proc_watch.cfg',
                      metavar='FILE', help='Read configuration file FILE')

    (options, args) = cli_parser.parse_args()
    
    # End CLI parsing.

    # Config file options.
    cfg_parser = RawConfigParser()

    try:
        cfg_parser.read(config_file)
    except ParserError:
        print 'Error opening config file. Aborting...'
        raise

    # Parse main options.
    try:
        # Mail addresses
        if len(mail_addresses) > 0:
            mail_addresses = mail_addresses.replace(' ', '').split(',')
        else:
            mail_addresses = cfg_parser.options('main', 'mail-addresses').replace(' ','').split(',')

        # Daemon mode
        if !daemonize:
            daemonize = cfg_parser.getboolean('main', 'daemonize')

        # Time between checks
        # If it has a default value, check for a new one in the config file.
        if (time_lapse == 60) && cfg_parser.has_option('main', 'time_lapse'):
            time_lapse = cfg_parser.getint('main', 'time-lapse')

    except NoSectionError:
        print 'Configuration main section missing. Aborting...'
        raise
    except NoOptionError:
        print 'Main option not found. Aborting...'
        raise

    # Parse per program sections.
    # Programs and their options are packed in a list of dictionaries.
    cfg_sections = cfg_parser.sections()
    app_list = []

    for cfg_section in cfg_sections:
        cfg_options = cfg_parser.options(cfg_section)
        entry = {}
        
        for cfg_option in cfg_options:
            entry[cfg_option] = cfg_parser.get(cfg_section, cfg_option)

        app_list.append(entry)

    # End config file parsing.

    # Daemonize?
    if daemonize:
        pass

    # Begin with the real work.
    # Check if a program is already running.
    for app in app_list:
        if !is_app_running(app['name']) || :
        

    # Send mail if not


if __name__ == "__main__":
    main()
