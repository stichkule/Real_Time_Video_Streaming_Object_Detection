'''
* Copyright (C) 2018 by Authors
*
*Redistribution,modification or use of this software in source or binary source is permitted as long 
*as the files maintain this copyright. Authors is not liable for any misuse of this material
* 
*@Description: The program test.py uses the image stored and detects the labels in the frame
              with AWS and response of AWS is stored in file data.txt

*@Authors:     Srivishnu Alvakonds
 	      Mounika Reddy Edula
              Shiril Tichkule

*@Python Version:Python 2.7

*@Date: 04/30/2018

*@Reference:https://gist.github.com/alexcasalboni/0f21a1889f09760f8981b643326730ff
           
'''
import sys
import boto3
import json

client = boto3.client('rekognition','us-west-2')
image_name = "images.jpg"
try:
    imgfile = open(image_name,'rb')
    imgbytes = imgfile.read()
    imgfile.close()
except:
    print("error opening image")

imgobj = {'Bytes':imgbytes}
response = client.detect_labels(Image=imgobj)
datafile = open("data.txt",'w');
json.dump(response,datafile);
			
