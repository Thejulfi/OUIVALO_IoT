import os
import atexit
import random
import binascii
import struct
from flask import Flask, render_template, request, json
import gspread

app = Flask(__name__)

# deviceId = "1D1F68" # L'idée https://backend.sigfox.com/

@app.route('/')
def hello_world():
    return 'Hello from Flask!'

@app.route('/getuid', methods=['GET', 'POST'])

def getiud():    
    if request.method == 'POST':
        # Connexion & ouverture du Google spreadsheet
        gc = gspread.service_account("credentials.json")
        sh = gc.open_by_url('https://docs.google.com/spreadsheets/d/1f8ZtkkAl2HnXhESc9xcHb-rWooIIDdOR7TOpHp76WD0/edit?usp=sharing')
        worksheet = sh.sheet1

        raw_s_data = binascii.unhexlify(request.json["data"])

        # print(raw_s_data)
        # print(raw_s_data.decode('utf-8'))
        
        raw_uid = raw_s_data.decode('utf-8').split('_')[0]
        raw_batteryLevel = ord(raw_s_data.decode('utf-8').split('_')[1])
        # raw_duree = ord(raw_s_data.decode('utf-8').split('_')[2][0])
        raw_temp = ord(raw_s_data.decode('utf-8').split('_')[2])
        raw_hum = ord(raw_s_data.decode('utf-8').split('_')[3])
        
       
       # Données brutes reçues du callback Sigfox
        rawData  = "{}-{}-{}-{}-{}-{}".format(raw_uid,request.json["device"], request.json["time"], raw_batteryLevel, raw_temp, raw_hum)
       
       # Mise à jour de la cellule du Googlel spreadsheet
        values_list = worksheet.col_values(2)
        # print(values_list)

        uid = "B{}".format(str(len(values_list)+1))
        time = "C{}".format(str(len(values_list)+1))
        deviceID = "A{}".format(str(len(values_list)+1))
        batteryL = "E{}".format(str(len(values_list)+1))
        temperature = "F{}".format(str(len(values_list)+1))
        humidity = "G{}".format(str(len(values_list)+1))

        worksheet.update(uid, str(rawData.split('-')[0]))
        worksheet.update(deviceID, str(rawData.split('-')[1]))
        worksheet.update(time, str(rawData.split('-')[2]))
        worksheet.update(batteryL, str(rawData.split('-')[3]))
        worksheet.update(temperature, str(rawData.split('-')[4]))
        worksheet.update(humidity, str(rawData.split('-')[5]))
        
        print(request.json) # Affichage de l'information brute
        
        # print(rawData) # Affichage de l'information brute
        return request.json 
    else:
        return 0


if __name__=="__main__":
    app.run()