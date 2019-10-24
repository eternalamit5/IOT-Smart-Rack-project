
// Importing all the required libraries //
from __future__ import division
from time import sleep
import requests
import csv
import math
import serial
import os
import sys
import urllib
import urllib2
import urllib3
import httplib
import RPi.GPIO as GPIO
import requests

// Defining API key to connect to thingspeak //
key = 'RVI5QQENMEC5Y7PE'
sleep = 15


def thingspeak(): // Defining the function "thingspeak" //
        ser = serial.Serial('/dev/ttyUSB0',115200, 8, 'N', 1, timeout = 15) // Read input from serial port (Re-mote) //
        print "reading"
        while True:
		// Define all the parameters required for reading //
                output = ser.readline()
                print output
                a, b, c, d = output.split()
                node = int(a)
                temp = int(b)
                tempx = temp/1000
                hum = int(c)
                humi = hum/100
                power = int(d)
                ran = 0
                id = 0
                sen1 = 0
                sen2 = 0
                Temp1 = 0
                Hum1 = 0
                Power1 = 0
                Temp2 = 0
                Hum2 = 0
                Power2 = 0
                if node == 100:
                        ran = node
                        id = temp
                        sen1 = hum
                        sen2 = power
                if node == 1:
                        Temp1 = tempx
                        Hum1 = humi
                        Power1 = power
                if node == 2:
                        Temp2 = tempx
                        Hum2 = humi
                        Power2 = power
		// Connect all the parameters to thingspeak and upload the data on the server //	
                params = urllib.urlencode({'field1':Temp1,'field2':Hum1,'field3':Temp2,'field4':Hum2,'field5':Power1,'field6':Power2,'field7':id,'key':key})
                headers = {"Content-typZZe": "application/x-www-form-urlencoded","Accept": "text/plain"}
                conn = httplib.HTTPConnection("api.thingspeak.com:80")
                try:
                        conn.request("POST", "/update", params, headers)
                        response = conn.getresponse()
                        print response.status, response.reason
                        data = response.read()
                        conn.close()
                except:
                        print "connection failed"
                break

// Call the function defined //
if __name__ == '__main__':
        while True:
                thingspeak()

