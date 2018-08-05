'''
* Copyright (C) 2018 by Authors
*
*Redistribution,modification or use of this software in source or binary source is permitted as long 
*as the files maintain this copyright. Authors is not liable for any misuse of this material
* 
@Description: The program labels.py listens on the UDP port 10001 
	      which recives data regarding objects detected,the 
              data is parsed and converted to voice

@Authors:     Srivishnu Alvakonds
 	      Mounika Reddy Edula
              Shiril Tichkule

@Python Version:Python 2.7

@Date: 04/30/2018

@Reference:https://www.geeksforgeeks.org/convert-text-speech-python/
           
'''

import socket #module for UDP socket
from gtts import gTTS #module for Google text to Speech
import os #module to run on command line
import json #parsing json data

UDP_PORT = 10002 #port on which server listens
language = 'en' #language of text to voice
sock = socket.socket(socket.AF_INET,socket.SOCK_DGRAM) #open socket for UDP
sock.bind(('', UDP_PORT)) #bind the IP of server to the port which accepts connection from any IP

while True:
    f = open("labels.txt","w") #save data to labels.txt
    data, addr = sock.recvfrom(8000) # buffer size is 8000 bytes
    f.write(data)
    f.close()
    os.system(r"tr -d '\0' < labels.txt > labels1.txt") #remove unwanted nulls at the end
    json_data = open("labels1.txt","r").read()
    data= json_data.split(', "ResponseMetadata"')#remove the unwanted data from AWS json
    complete=str(data[0]) + '}'#add a brace for a json
    parsed_json = json.loads(complete) #load json for parsing
    txt_str = " "
    txt1_str = " "
    i = 0
    for labels in parsed_json['Labels']:
	txt_str = txt_str + labels['Name'] + ''
    #store the object names detected
    #for labels in parsed_json['Labels']:
    #    i = i + 1
    #	txt1_str = txt1_str + labels['Name']
    #    if(i == 3):
    #        break     
    print txt_str
    #myobj = gTTS(text=txt1_str, lang=language, slow=False)#text to voice conversion
    #myobj.save("welcome.mp3")#save the data to mp3
    #os.system("mpg321 welcome.mp3")#play the audio

