# Real_Time_Video_Streaming_Object_Detection
Real-time wireless video streaming and object detection on the Nvidia Jetson TK1 platform

Developed a soft real-time system for wireless video streaming @ 10 fps using OpenCV, along with object-detection functionality on the Nvidia Jetson TK1 platform.

Performed frame-timing and jitter analysis to determine worst-case execution times (WCET) of the image capture, streaming, and object-detection services, and devised a cyclical executive to schedule them using the POSIX threading framework.

Implemented a server-client architecture for transferring image frames to a remote terminal via the UDP protocol, as well as to communicate with the AWS Rekognition object-detection server. 

Augmented the system with Google text-to-speech (gTTS) functionality to provide audio output for objects detected within the video stream.
