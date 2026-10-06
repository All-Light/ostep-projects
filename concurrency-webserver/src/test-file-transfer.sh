

# request 1000 images at once (largest file)
for i in {0..1000}
do
    ./wclient localhost 8003 /images/image5.jpg &
done