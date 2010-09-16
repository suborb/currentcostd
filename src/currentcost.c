/*
 *   Current Cost Daemon
 *
 *   Copyright (C) 2010, Dominic Morris
 *
 *   $Id:$ 
 *   $Date:$
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License.
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



#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <errno.h>
#include <pwd.h>
#include <termios.h>
#include <fcntl.h>
#include <regex.h>



#include "libini.h"

#define VERSION "0.0.1"


static int         serial_open(char *device);
static void        serial_close();
static void        parse_line(char *line);
static char       *strduplen(char *ptr, size_t len);

/* Real configurable items */
static char       *c_config_file         = NULL;
static char       *c_pid_file            = "/var/run/currentcost.pid";
static char       *c_user                = NULL;
static char        c_help                = 0;
static char        c_daemon              = 0;
static char       *c_serial_port         = "/dev/ttyU1";
static char       *c_update_command      = NULL;
static int         c_baudrate            = 57600;

static int         serial_fd             = -1;
static FILE       *serial_fp             = NULL;


static void cleanup_files()
{
    unlink(c_pid_file);

    closelog();
}

    


int main(int argc, char *argv[])
{
    configctx_t *ctx;
    int          s;
    int          len;

    /* Set up some basic stuff */
    atexit(cleanup_files);
    openlog("currentcost", LOG_PID, LOG_USER);

    /* Get out any daemonising configuration */
    ctx = iniparse_init("main");

    iniparse_add(ctx,'f',"main:config-file","Configuration file location",OPT_STR,&c_config_file);
    iniparse_add(ctx, 0, "main:pid-filename", "Location of the pid file",OPT_STR,&c_pid_file);
    iniparse_add(ctx, 0, "main:user","User to run the program as", OPT_STR,&c_user);
    iniparse_add(ctx, 'd', "main:daemon", "Daemonise the program", OPT_BOOL, &c_daemon);
    iniparse_add(ctx,'h',"main:help","Display this help information",OPT_BOOL,&c_help);
    iniparse_add(ctx, 0, "serial:port","Serial port", OPT_STR,&c_serial_port);
    iniparse_add(ctx, 0, "serial:baudrate","Baudrate for the serial device",OPT_INT,&c_baudrate);
    iniparse_add(ctx, 0, "exec:command","Command to log data",OPT_STR,&c_update_command);

    /* Parse arguments to get out since location of config may change */
    iniparse_args(ctx,argc,argv);

    if ( (c_config_file && iniparse_file(ctx,c_config_file) == -1) || c_help ) {
        printf("Current cost daemon %s\n\n",VERSION);
        iniparse_help(ctx,stdout);
        printf("\n");
        exit(1);
    }
    iniparse_cleanup(ctx);


    /* Put the application into the background if necessary */
    if ( c_daemon ) {
        pid_t pid;       
        if( (pid = fork()) > 0) {
            FILE *pidfile = fopen(c_pid_file, "w");
            if( pidfile != NULL ) {
                fprintf(pidfile, "%d\n", pid);
                fclose(pidfile);
            } else {
                fprintf(stderr, "Unable to write pid to %s, pid is %d\n", c_pid_file,pid);
            }
            _exit(0);
        }
        freopen("/dev/null","w",stdout);
        freopen("/dev/null","w",stderr);
    }

   if ( c_user ) {
        uid_t               saved_uid, uid;
        gid_t               saved_gid, gid;
        struct passwd      *pw;

        uid = (uid_t)atoi(c_user);
        
        if ( uid == 0 ) {
            pw = getpwnam(c_user);
            if ( pw ) {
                uid = pw->pw_uid;
                gid = pw->pw_gid;
            } else {
                uid = -1;
                gid = -1;
            }
        } else {
            pw = getpwuid(uid);

            gid = pw->pw_gid;
        }

        saved_uid = geteuid();
        saved_gid = getegid();
        if (seteuid(0) == 0) {
            if (gid != -1 && setegid(gid) != 0) {
                setegid(saved_gid);
            }
            
            if (uid != -1 && seteuid(uid) != 0) {
                seteuid(saved_uid);
            }
        } 
    }

    syslog(LOG_INFO,"Current cost daemon %s starting",VERSION);

    /* Loop round waiting to read from the socket */
    while ( 1 ) {
        if ( serial_fd == -1 ) {
            serial_open(c_serial_port);
        }
        if ( serial_fd != -1 ) {
            char  line[1024];

            if ( fgets(line,sizeof(line),serial_fp) != NULL ) {
                parse_line(line);
            } else {
               serial_close();
               sleep(1);
            }
        } else {
            sleep(5);
        }
    }
    /* And exit as normal */
    exit(0);
}

/** \brief Parse the line and then do something with it as necessary
 *
 *  \param line - Line to parse
 */
#define regparm(x) strduplen(line + regmatch[x].rm_so, regmatch[x].rm_eo - regmatch[x].rm_so)
static void parse_line(char *line)
{
    static regex_t   regex;
    static time_t    last;
    static int       compiled = 0;
    char             buf[4096];
    regmatch_t       regmatch[6];
    char            *hour,*min,*sec,*temp,*watt;
    time_t           now;
    struct tm         tm;
    int              delta;
    int              offset;
    int              joules;

    if ( compiled == 0 && regcomp(&regex,"<time>(.*):(.*):(.*)</time>.*<tmpr>(.*)</tmpr>.*<ch1><watts>(.*)</watts>", REG_EXTENDED) != 0 ) {
        return;
    }
    compiled=1;

    if ( regexec(&regex, line, 6, &regmatch[0], 0) != 0 ) {
       syslog(LOG_WARNING,"Failed to match regex on: %s",line);
       return;
    }
    hour = regparm(1);
    min = regparm(2);
    sec = regparm(3);
    temp = regparm(4);
    watt = regparm(5);

    now = time(NULL);
    localtime_r(&now,&tm);
    if ( last == 0 ) {
       delta = 6;
    } else {
       delta = now -last;
    }
    last = now;
    joules = atoi(watt) * delta;
    offset = (atoi(hour) * 3600) + (atoi(min)*60) + atoi(sec);
    offset -= ( ( tm.tm_hour * 3600 ) + ( tm.tm_min * 60 ) + tm.tm_sec);

    if ( c_update_command ) {
       snprintf(buf,sizeof(buf),"%s %ld %d %s %s:%s:%s %d %d %d",c_update_command, now, atoi(watt), temp, hour, min, sec, delta, joules, offset);
       system(buf);
    }
    syslog(LOG_INFO,"Temperature is %s current watts %d",temp,atoi(watt));

    free(hour); 
    free(min);
    free(sec);
    free(temp);
    free(watt);
}

static char *strduplen(char *ptr, size_t len)
{
    char *ret = calloc(len + 1, sizeof(char));
    memcpy(ret, ptr, len);
    return ret;
}

/**
 * \brief Open the specified serial port
 *
 * \param device - Serial port to open
 *
 * \return 0 - Opened ok (and serial_fd/serial_fp are setup
 * \retval -1 - Failure to open
 *
 * \note Code lifted from open2300 - http://www.lavrsen.dk/twiki/bin/view/Open2300/WebHome
 */
static int serial_open(char *device)
{
    int             fd;
    struct termios  adtio;
    int             portstatus, fdflags;
    int             arg;

    if ((fd = open(device, O_RDONLY|O_NONBLOCK)) < 0) {
        printf("\nUnable to open serial device %s\n", device);
        return -1;
    }
    syslog(LOG_INFO,"Opened serial port <%s>",device);
    arg = fcntl(fd, F_GETFL, NULL);
    arg&= ~O_NONBLOCK; 
    fcntl(fd, F_SETFL, arg);
    
#if 0
    if ( flock(fd, LOCK_EX|LOCK_NB) < 0 ) { 
        perror("\nSerial device is locked by other program\n");
        close(fd);
        return -1;
    }
#endif
    
    memset(&adtio, 0, sizeof(adtio));
    
    adtio.c_cflag &= ~PARENB;      // No parity
    adtio.c_cflag &= ~CSTOPB;      // One stop bit
    adtio.c_cflag &= ~CSIZE;       // Character size mask
    adtio.c_cflag |= CS8;          // Character size 8 bits
    adtio.c_cflag |= CREAD;        // Enable Receiver
    adtio.c_cflag &= ~HUPCL;       // No "hangup"
    adtio.c_cflag |= CRTSCTS;      // No flowcontrol
    adtio.c_cflag |= CLOCAL;       // Ignore modem control lines

    cfsetispeed(&adtio, c_baudrate);
    cfsetospeed(&adtio, c_baudrate);    
    
    // Serial local options: adtio.c_lflag
    // Raw input = clear ICANON, ECHO, ECHOE, and ISIG
    // Disable misc other local features = clear FLUSHO, NOFLSH, TOSTOP, PENDIN, and IEXTEN
    // So we actually clear all flags in adtio.c_lflag
    adtio.c_lflag = 0;

    // Serial input options: adtio.c_iflag
    // Disable parity check = clear INPCK, PARMRK, and ISTRIP 
    // Disable software flow control = clear IXON, IXOFF, and IXANY
    // Disable any translation of CR and LF = clear INLCR, IGNCR, and ICRNL    
    // Ignore break condition on input = set IGNBRK
    // Ignore parity errors just in case = set IGNPAR;
    // So we can clear all flags except IGNBRK and IGNPAR
    adtio.c_iflag = IGNBRK|IGNPAR;
    
    // Serial output options
    // Raw output should disable all other output options
    adtio.c_oflag &= ~OPOST;

    adtio.c_cc[VTIME] = 10;        // timer 1s
    adtio.c_cc[VMIN] = 1;        // blocking read until 1 char
    
    if (tcsetattr(fd, TCSANOW, &adtio) < 0) {
        syslog(LOG_ERR,"Unable to initialize serial device - will try again in a bit");
        close(fd);
        return -1;
    }

    tcflush(fd, TCIOFLUSH);

    // Set DTR low and RTS high and leave other ctrl lines untouched

    ioctl(fd, TIOCMGET, &portstatus);    // get current port status
    portstatus &= ~TIOCM_DTR;
    portstatus |= TIOCM_RTS;
    ioctl(fd, TIOCMSET, &portstatus);    // set current port status

    serial_fd = fd;
    serial_fp = fdopen(serial_fd,"r");

    return 0;
}

static void serial_close()
{
    close(serial_fd);
    fclose(serial_fp);
    serial_fd = -1;
    serial_fp = NULL;
}



/** \brief Required to satisfy the linking of libini.c
 */
char *filename_expand(char *format, char *buf, size_t buflen, char *i_option, char *k_option)
{
    snprintf(buf, buflen, "%s",format);

    return buf;
}
