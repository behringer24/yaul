# YAUL Yet Another UDP Logger

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
    -h this help
    -d daemonize
    -p [port] Bind to port number
    -b [ip] Bind top ip address
    -l [path] Logging to path
    -s [frequency] log statistics to file yaul.stat every [frequency] logmessage
    -v Version
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