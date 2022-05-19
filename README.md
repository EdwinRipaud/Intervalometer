## Installation
### Prerequisites
>You must connect your Raspberry Pi to a router via a wifi or ethernet cable.
You must also identify its IP address.
>You can use ``ifconfig`` on the Raspberry Pi terminal to get the IP address.

### Local installation
Download the repository on your ***Raspberry Pi Zero W*** (or ***Raspberry Pi 4***)
Run the ***'compiler'*** file with :
```
sh compiler.sh
```
Then, to check if the code works, run the ***'run.sh'*** file with :
```
sh run.sh
```
Connect your smartphone (tablet or computer) to the same router as the Raspberry Pi, and open a web browser.

Type the IP address of the Raspberry Pi followed by ``::55000``, which is the TCP port open for the web application. The URL should look like this: ``10.03.141.1::55000``.

You are ready to use the Intervalometer!

## Error

### The web page does not load
* The problem may be due to the compilation of the source code.
You must recompile the ***'server.cpp'*** code from a terminal by typing: ``g++ -Wall server.cpp -o server.exe -lwiringPi``.
If the compiler outputs an error... it's complicated 🫤
* It can also be a connection problem between the smartphone and the Raspberry Pi. You can check this by running from a terminal: ``./server.exe``. This should run the compiled code from that terminal, giving you access to the code output. When you try to load the web-app page, you should see a new output that indicates that someone is connected to the web-app. If not, you will simply see "Listening on port 55000 ...``.

