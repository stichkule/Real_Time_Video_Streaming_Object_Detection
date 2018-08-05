# Real time Video Streaming and Object Detection

This project is inspired by https://github.com/chenxiaoqino/udp-image-streaming , where images are transfered using

Parameters for video frame size are adjusted in config.h file and server is run on 2 different ports 10000,10001 for receiving audio and video.

## Capture

The code grabs frame from OpenCV's default input device. To change the source, change the argument passed to `cv::VideoCapture()` accordingly
from the results of ls/dev/.

## Demo

Run the following commands to see stream your camera
```
cd udp-image-streaming-client/
cmake . && make
./client serverip 10000 10001
```

## Acknowledgement and Copyright
This project is built from various open-sourced libraries, like [Practical C++ Sockets](http://cs.ecs.baylor.edu/~donahoo/practical/CSockets/practical/) and [OpenCV 3](http://opencv.org/) ; please refer to their original license accordingly (GPL/BSD).
