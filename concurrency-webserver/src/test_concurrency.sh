# simple request 
./wclient localhost 8003 /index.html &

# concurrent test on slow endpoint
./wclient localhost 8003 /cgi/spin?3 &
./wclient localhost 8003 /cgi/spin?6 &
./wclient localhost 8003 /cgi/spin?3 &
./wclient localhost 8003 /cgi/spin?1 &
./wclient localhost 8003 /cgi/spin?7 &
./wclient localhost 8003 /cgi/spin?4 &
./wclient localhost 8003 /level-one/level-two/deep.html 
