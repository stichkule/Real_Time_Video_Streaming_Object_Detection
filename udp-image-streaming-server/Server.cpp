/*****************************************************************************************
 *   C++ UDP server for live image upstreaming
 *   Modified from https://github.com/chenxiaoqino/udp-image-streaming
 *   Copyright (C) 2018
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   @Description: This uses Open source library practical sockets to set
 *                 udp server which listens to the port provided by user and
 *                 uses Open CV library to decode the raw bytes recieved
 *   @Authors:     Srivishnu Alvakonda
 *	               Mounika Reddy Edula
 *                 Shiril Tichkule
 *   @Date:        04/30/2018
 *
 **************************************************************************************/

#include "PracticalSocket.h"  // UDPSocket,SocketException - Open source library
#include <cstdlib>            // For atoi()
#include <iostream>           //For Cerror
#include "opencv2/opencv.hpp" //open cv library
#include "config.h" //configuration settings

using namespace cv;

#define BUF_LEN 65540 // maximum udp packet size theortically

int main(int argc, char * argv[]) {

    if (argc != 2) { // check number of parameters
        cerr << "Usage: " << argv[0] << " <Server Port>" << endl;
        exit(1);
    }

    unsigned short serv_Port = atoi(argv[1]); // Port on which server is binded

    namedWindow("Video Reciever", CV_WINDOW_AUTOSIZE); // Open the Window for Video streaming

    try {
        UDPSocket sock(serv_Port); // open UDP socket binded to the port
        char buffer[BUF_LEN]; // Buffer to store messages recieved
        int recv_MsgSize; // received bytes
        string sourceAddr; // Address of the client
        unsigned short source_Port; // Port of client
	      int total_pack = 0;
        while (1) {
            // Block until you receive message from a client
            do {
                recv_MsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddr, source_Port);
            } while (recv_MsgSize > sizeof(int));
	          //Number of UDP packets for an image
            total_pack= ((int * ) buffer)[0];
	         //final buffer which has complete image
            char * final_image = new char[PACK_SIZE * total_pack];
           //Recieve all the packets for an image
            for (int i = 0; i < total_pack; i++) {
                recv_MsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddr, source_Port);
                //each packet should be of size PACK_SIZE
                if (recv_MsgSize != PACK_SIZE) {
                    cerr << "Received unexpected size:" << recv_MsgSize << endl;
                    continue;
                }
                //Copy the packet to final buffer for complete image
                memcpy( & final_image[i * PACK_SIZE], buffer, PACK_SIZE);
            }

            Mat raw_Data = Mat(1, PACK_SIZE * total_pack, CV_8UC1, final_image);
            //Decode the raw bytes to form image
            Mat frame = imdecode(raw_Data, CV_LOAD_IMAGE_COLOR);
            //Check if the decoding worked
            if (frame.size().width == 0) {
                cerr << "decoding failure!" << endl;
                continue;
            }

            //Display the frame recieved
            imshow("Video Reciever", frame);
            //Free the memory for the frame
            free(final_image);
	          //Mandatory for imshow()
            waitKey(1);
        }
    } catch (SocketException & e) {
        cerr << e.what() << endl;
        exit(1);
    }

    return 0;
}
