import pandas as pd
import joblib
import time
import json
import requests

# Load the pre-trained model and preprocessing tools
best_pipeline = joblib.load('path_to_saved_model/best_pipeline.joblib')
scaler = joblib.load('path_to_saved_model/scaler.joblib')
selector = joblib.load('path_to_saved_motdel/selector.joblib')

#mqtt
mqtt_broker_address = 'localhost'
mqtt_topic = 'esp32/sensor'



def on_message(client, userdata, message):
    payload = message.payload.decode('utf-8')
    print(f'Received message: {payload}')

    sensor = json.loads(payload)
    avgtemp = sensor["Temperature"]
    avghumidity = sensor["Humidity"]
    moisture = sensor["Soil Moisture"]
    forecast = requests.get("https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current=temperature_2m,wind_speed_10m&hourly=temperature_2m,relative_humidity_2m,wind_speed_10m")
    avgwindspeed = forecast["current"]["wind_speed_10m"]

    if avgtemp is not None and avghumidity is not None and avgwindspeed is not None and moisture < 20:
        # Preprocess the sensor data
        preprocessed_data = preprocess_sensor_data(avgtemp, avghumidity, avgwindspeed)

        # Make a prediction
        prediction = make_prediction(preprocessed_data)

        # Output the prediction
        rain_prediction = "Yes" if prediction[0] else "No"
        print(f"Prediction: Will it rain tomorrow? {rain_prediction}")
        if prediction[0]:
            msg = "on"
            client.publish("esp32/pump", msg)



    # Wait for some time before reading new data
    time.sleep(60)  # for example, wait for 60 seconds


# Function to read sensor data from a file
def read_sensor_data(file_path):
    try:
        # Assuming the file has one line with comma-separated values for avgtemp, avghumidity, avgwindspeed
        with open(file_path, 'r') as file:
            line = file.readline()
            avgtemp, avghumidity, avgwindspeed = map(float, line.strip().split(','))
        return avgtemp, avghumidity, avgwindspeed
    except Exception as e:
        print(f"Error reading sensor data: {e}")
        return None, None, None

# Function to preprocess sensor data
def preprocess_sensor_data(avgtemp, avghumidity, avgwindspeed):
     # Create a DataFrame with the sensor data
    sensor_data = pd.DataFrame({
        'avgtemp': [avgtemp],
        'avghumidity': [avghumidity],
        'avgwindspeed': [avgwindspeed]
    })
    # Apply the same preprocessing as during training
    sensor_data_scaled = scaler.transform(sensor_data)
    sensor_data_selected = selector.transform(sensor_data_scaled)

    return sensor_data_selected

# Function to make predictions
def make_prediction(preprocessed_data):
   # Use the loaded model to make predictions
    prediction = best_pipeline.predict(preprocessed_data)
    return prediction

# Path to the file containing sensor data
sensor_data_file_path = 'path_to_sensor_data_file.txt'

client = mqtt.Client()
client.on_message = on_message
# Connect to MQTT broker
client.connect(mqtt_broker_address, 1883, 60)
# Subscribe to MQTT topic
client.subscribe(mqtt_topic)
# Start the MQTT loop
client.loop_forever()
