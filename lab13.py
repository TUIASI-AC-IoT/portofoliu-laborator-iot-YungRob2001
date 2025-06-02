from flask import Flask, request, jsonify
from flask_jwt_extended import JWTManager, create_access_token, get_jwt_identity, jwt_required
from flask_jwt_extended import get_jwt
from datetime import timedelta

app = Flask(__name__)
app.config['JWT_SECRET_KEY'] = 'super-secret-key'
app.config['JWT_ACCESS_TOKEN_EXPIRES'] = timedelta(hours=1)

jwt = JWTManager(app)

# In-memory user database
users = {
    "user1": {"password": "parola1", "role": "admin"},
    "user2": {"password": "parola2", "role": "owner"},
    "user3": {"password": "parolaX", "role": "owner"}
}

# Token blacklist for logout
jwt_blacklist = set()

@jwt.token_in_blocklist_loader
def check_if_token_revoked(jwt_header, jwt_payload):
    return jwt_payload['jti'] in jwt_blacklist

@app.route('/auth', methods=['POST'])
def login():
    data = request.get_json()
    username = data.get('username')
    password = data.get('password')

    user = users.get(username)
    if user and user['password'] == password:
        access_token = create_access_token(identity=username, additional_claims={"role": user['role']})
        return jsonify(token=access_token), 200
    return jsonify(msg="Bad username or password"), 401

@app.route('/auth/jwtStore', methods=['GET'])
@jwt_required()
def validate_token():
    identity = get_jwt_identity()
    claims = get_jwt()
    role = claims.get("role")
    return jsonify(username=identity, role=role), 200

@app.route('/auth/jwtStore', methods=['DELETE'])
@jwt_required()
def logout():
    jti = get_jwt()['jti']
    jwt_blacklist.add(jti)
    return jsonify(msg="Successfully logged out"), 200

# Example sensor data endpoint
@app.route('/sensor/<sensor_id>', methods=['GET'])
@jwt_required()
def read_sensor(sensor_id):
    claims = get_jwt()
    role = claims.get("role")
    if role in ["owner", "admin"]:
        return jsonify(sensor_id=sensor_id, value="123.45"), 200
    return jsonify(msg="Unauthorized"), 403

@app.route('/sensor/<sensor_id>/config', methods=['POST', 'PUT'])
@jwt_required()
def update_sensor_config(sensor_id):
    claims = get_jwt()
    role = claims.get("role")
    if role == "admin":
        return jsonify(sensor_id=sensor_id, status="config updated"), 200
    return jsonify(msg="Unauthorized"), 403

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')
