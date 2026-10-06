

# request 10 000 images at once (largest file)
for i in {0..10000}
do
    ./wclient localhost 8003 /images/image5.jpg > /dev/null &
done