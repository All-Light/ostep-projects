# request index.html 10 times
for i in {0..10}
do
    ./wclient localhost 8003 /cgi/spin?${i} &
done