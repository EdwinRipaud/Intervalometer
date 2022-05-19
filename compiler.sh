echo "run the file \"compiler.sh\" to compile the main programme of the Intervalometer software"

g++ -Wall server.cpp -o server.exe -lwiringPi

echo "The compilation ended"