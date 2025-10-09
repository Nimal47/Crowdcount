Crowd Count Monitoring System

An IoT-based smart crowd counting system that detects and tracks the number of people entering or leaving a room using ultrasonic sensors and an ESP32. The live count is updated in Firebase Realtime Database and displayed on a web dashboard for easy monitoring.

Project Overview

This system automatically counts the number of people in a room or hall and updates the data to the cloud in real-time.
It’s ideal for smart classrooms, labs, libraries, or offices, where knowing occupancy levels is important for automation, safety, or energy management.

Setup Instructions

1️⃣ Hardware Setup
	•	Place two ultrasonic sensors at the entrance
	•	Connect them to ESP32 GPIO pins as shown in the code.
	•	Power the ESP32 using USB or 5V adapter.

2️⃣ Firebase Setup
	1.	Go to Firebase Console
	2.	Create a new project
	3.	Enable Realtime Database
	4.	Copy your database URL and credentials.

3️⃣ Arduino Code Setup
	•	Add your Wi-Fi and Firebase credentials in credentials.h
	•	Upload the code to ESP32
	•	Open Serial Monitor to confirm connection and data updates

4️⃣ Web Dashboard Setup
	•	Create a index.html file and add your Firebase config in it
	•	The dashboard fetches live count data and displays it dynamically
  
