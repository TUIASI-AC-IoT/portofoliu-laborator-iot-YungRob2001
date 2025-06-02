from flask import Flask, abort, request, jsonify
import os
import random

app = Flask(__name__)

SENSORS_DIR = "sensors"

FILES_DIR = "files"

if not os.path.exists(FILES_DIR):
    os.makedirs(FILES_DIR)
def safe_filename(filename):    
    return os.path.basename(filename)


if not os.path.exists(SENSORS_DIR):
    os.makedirs(SENSORS_DIR)

@app.route('/sensor/<sensor_id>', methods=['GET'])
def get_sensor_value(sensor_id):
    
    value = round(random.uniform(0, 100), 2)
    return jsonify({
        "sensor_id": sensor_id,
        "value": value,
        "unit": "unitate_simulata"
    })


@app.route('/sensor/<sensor_id>', methods=['POST'])
def create_sensor_config(sensor_id):
    if not request.json or 'scale' not in request.json:
        abort(400, description="Missing 'scale' in JSON body.")

    sensor_dir = os.path.join(SENSORS_DIR, sensor_id)
    os.makedirs(sensor_dir, exist_ok=True)

    config_path = os.path.join(sensor_dir, 'config.txt')

    if os.path.exists(config_path):
        abort(409, description=f"Config for sensor '{sensor_id}' already exists.")

    try:
        with open(config_path, 'w', encoding='utf-8') as f:
            f.write(f"scale={request.json['scale']}\n")
        return jsonify({
            "sensor_id": sensor_id,
            "config_file": "config.txt",
            "message": "Configuration created"
        }), 201
    except Exception as e:
        abort(500, description=f"Failed to create config: {str(e)}")


@app.route('/sensor/<sensor_id>/<config_name>', methods=['PUT'])
def update_sensor_config(sensor_id, config_name):
    if not request.json or 'scale' not in request.json:
        abort(400, description="Missing 'scale' in JSON body.")

    sensor_dir = os.path.join(SENSORS_DIR, sensor_id)
    config_path = os.path.join(sensor_dir, safe_filename(config_name))

    if not os.path.exists(config_path):
        abort(404, description=f"Configuration '{config_name}' for sensor '{sensor_id}' not found.")

    try:
        with open(config_path, 'w', encoding='utf-8') as f:
            f.write(f"scale={request.json['scale']}\n")
        return jsonify({
            "sensor_id": sensor_id,
            "config_file": config_name,
            "message": "Configuration updated"
        })
    except Exception as e:
        abort(500, description=f"Failed to update config: {str(e)}")
