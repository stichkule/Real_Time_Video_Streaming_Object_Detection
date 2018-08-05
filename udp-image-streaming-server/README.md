# Real time Video Streaming and Object Detection

This project is inspired by https://github.com/chenxiaoqino/udp-image-streaming , where images are transfered using

Parameters for video frame size are adjusted in config.h file and server is run on 2 different ports 10000,10001 for receiving audio and video.

## Demo

Run these commands to run the code
```
cd udp-image-streaming-server/
cmake . && make
./server 10000 &
open another terminal
cd Audio
python labels.py 
```

## Acknowledgement and Copyright
This project is using various open-sourced libraries, like [Practical C++ Sockets](http://cs.ecs.baylor.edu/~donahoo/practical/CSockets/practical/) and [OpenCV 3](http://opencv.org/) ; please refer to their original license accordingly (GPL/BSD).
