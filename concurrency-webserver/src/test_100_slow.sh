# request index.html 100 times
for i in {0..100}
do
    ./wclient localhost 8003 /cgi/spin?${i} &
done