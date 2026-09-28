The spin CGI program reads a number of seconds from QUERY_STRING.

Build it on the machine that will run the web server:

    make

The resulting executable must be named "spin" and remain in this directory.
Try /cgi/spin?1 and /cgi/spin?3 from index.html.
