# YAUL Yet Another UDP Logger [![Build Status](https://secure.travis-ci.org/behringer24/yaul.png?branch=development)](https://travis-ci.org/behringer24/yaul)

## What is it
YAUL is a simple and lightweight UDP logging daemon for linux written in C.

It takes UDP messages in a simple specified format and writes them to logfiles.

YAUL is an easy solution to implement non blocking high performance logging
solutions.

## Installation
Installation of yaul is very simple.

1. Load the sources on your server by cloning from git or download the tar.gz
2. unpack the ressources if you loaded the tar.gz
3. compile sources by typing: make and then make install
4. make sure there is a directory /var/log/yaul and the user that will run the yaul server binary has write access to that directory
5. run the server and configure itvia the provided options 

Setup an appropriate logrotate pattern in your /etc/logrotate.conf or /etc/logrotate.de

## Usage
```
    Usage: yaul [options]
    -h, -?, --help             display command line help information
    -d, --daemonize            daemonize server process
    -p, --port=PORT            bind to port number (default %u)
    -b, --bind=IP              bind to ip address (default %s)
    -l, --logpath=PATH         logging to path (default %s)
    -s, --statistics=FREQUENCY log statistics to file yaul.stat after every [frequency] logmessage
    -f, --flush=FREQUENCY      flush output stream after every [frequency] logmessage
    -v, --version              display version information
```

## Message format
\[\<logname\>\]\<message\>

Log the text in \<message\> in the logfile /var/log/yaul/\<logname\>.log

The \<logname\> must consist of ASCII a-z, A-Z, 0-9 and the . (dot) for namespacing

If the \<logname\> and the paranthesis [ ] are omitted the default logname is 'yaul'. Same applies if the logname is invalid

## Limitations
The maximum length of the logname are 255 chars.

The maximum length of the message in total (including logname) is 1500 chars by default. This also depends of the maximum length of UDP messages of your server.

The message is truncated on the first newline char. This means the message cannot consist of multiple lines.

## Logrotate
The setup of an additional logrotate rule is simple. Just create another role in /etc/logrotate.conf or add a file with the rules for the yaul logfiles in /etc/logrotate.d/

a restart of a daemon is not needed. The rule is used on the next logrotate run.

```
/var/log/yaul/*.log {
    compress
    delaycompress
    dateext
    maxage 365
    rotate 30
    size=+4096k
    notifempty
    missingok
    create 644 root root
    postrotate
     kill -s HUP $(pidof yaul)
    endscript
}
````

This rule will rotate all logs in /var/log/yaul/ that end with .log and are over 4096k in size. The files are extended with a date stamp and kept to a maximum of 365 days. The rotation is performed only if the file is not empty. The first rotation just attaches the date stamp, on the second rotation the file is compressed by gz. The maximum number of rotated files is 30. The rotated files belong to root and are readable by all. There is no error thrown if the files do not exist at all.