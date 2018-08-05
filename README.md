# Real_Time_Video_Streaming_Object_Detection
Real-time wireless video streaming and object detection on the Nvidia Jetson TK1 platform

Soft real-time system for wireless video streaming @ 10 fps using OpenCV, along with object-detection functionality on the Nvidia Jetson TK1 platform.

Frame-timing and jitter analysis to determine worst-case execution times (WCET) of the image capture, streaming, and object-detection services, combined with a cyclical executive to schedule them using the POSIX threading framework.

Server-client architecture for transferring image frames to a remote terminal via the UDP protocol, as well as to communicate with the AWS Rekognition object-detection server. 

System augmented with Google text-to-speech (gTTS) functionality to provide audio output for objects detected within the video stream.
