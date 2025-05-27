from flask import Flask, jsonify, request, abort
import os
import uuid

app = Flask(__name__)

FILES_DIR = "files"

if not os.path.exists(FILES_DIR):
    os.makedirs(FILES_DIR)


def is_text_file(filename):
    text_extensions = ['.txt', '.md', '.py', '.js', '.html', '.css', '.json', '.xml', '.csv']
    _, ext = os.path.splitext(filename)
    return ext.lower() in text_extensions


def safe_filename(filename):    
    return os.path.basename(filename)


@app.route('/')
def hello():
    return jsonify({
        "message": "file manager",
        "endpoints": {
            "GET /files": "listeaza fisierele",
            "GET /files/<filename>": "arata continutul unui fisier",
            "POST /files": "creeaza un nou fisier",
            "POST /files/<filename>": "creeaza un fisier nou cu un nume specificat",
            "PUT /files/<filename>": "updateaza un fisier",
            "DELETE /files/<filename>": "sterge un fisier"
        }
    })


@app.route('/files', methods=['GET'])
def list_files():
    files = []
    for filename in os.listdir(FILES_DIR):
        file_path = os.path.join(FILES_DIR, filename)
        if os.path.isfile(file_path):
            file_info = {
                'name': filename,
                'size': os.path.getsize(file_path),
                'created': os.path.getctime(file_path),
                'modified': os.path.getmtime(file_path),
                'is_text': is_text_file(filename)
            }
            files.append(file_info)
    return jsonify({'files': files})


@app.route('/files/<filename>', methods=['GET'])
def get_files(filename):
    filename = safe_filename(filename)
    file_path = os.path.join(FILES_DIR, filename)

    if not os.path.exists(file_path):
        abort(404, description=f"File {filename} not found")
    
    if not os.path.isfile(file_path):
        abort(400, description=f"{filename} is not a file")
    
    if not is_text_file(filename):
        abort(400, description=f"{filename} is not a text file")
 
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()

            return jsonify({
                'name': filename,
                'content': content,
                'size': os.path.getsize(file_path),
                'modified': os.path.getmtime(file_path)
            })

    except Exception as e:
        abort(500, description=f"nu s-a putut citi fisierul: {str(e)}")
        

@app.route('/files', methods=['POST'])
def create_file_auto():
    if not request.json or 'content' not in request.json:
        abort(400, description="Content is required")

    content = request.json['content']

    filename = f"{uuid.uuid4()}.txt"
    file_path = os.path.join(FILES_DIR, filename)

    try:
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(content)

        return jsonify({
            'name': filename,
            'content': content,
            'size': os.path.getsize(file_path),
            'created': os.path.getctime(file_path)
        }), 201

    except Exception as e:
        abort(500, description=f"Error creating the file: {str(e)}")


@app.route('/files/<filename>', methods=['POST'])
def create_file(filename):
    if not request.json or 'content' not in request.json:
        abort(400, description="Content is required")
    
    filename = safe_filename(filename)
    content = request.json['content']
    file_path = os.path.join(FILES_DIR, filename)
    
    if os.path.exists(file_path):
        abort(409, description=f"File {filename} already exists")
    
    try:
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(content)
        
        return jsonify({
            'name': filename,
            'content': content,
            'size': os.path.getsize(file_path),
            'created': os.path.getctime(file_path)
        }), 201
    except Exception as e:
        abort(500, description=f"Error creating file: {str(e)}")


@app.route('/files/<filename>', methods=['PUT'])
def update_file(filename):
    if not request.json or 'content' not in request.json:
        abort(400, description="Content is required")
    
    filename = safe_filename(filename)
    content = request.json['content']
    file_path = os.path.join(FILES_DIR, filename)
    
    if not os.path.exists(file_path):
        abort(404, description=f"File {filename} not found")
    
    if not os.path.isfile(file_path):
        abort(400, description=f"{filename} is not a file")
    
    try:
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(content)
        
        return jsonify({
            'name': filename,
            'content': content,
            'size': os.path.getsize(file_path),
            'modified': os.path.getmtime(file_path)
        })
    except Exception as e:
        abort(500, description=f"Error updating file: {str(e)}")


@app.route('/files/<filename>', methods=['DELETE'])
def delete_file(filename):
    filename = safe_filename(filename)
    file_path = os.path.join(FILES_DIR, filename)
    
    if not os.path.exists(file_path):
        abort(404, description=f"File {filename} not found")
    
    if not os.path.isfile(file_path):
        abort(400, description=f"{filename} is not a file")
    
    try:
        os.remove(file_path)
        return jsonify({
            'result': 'success',
            'message': f"File {filename} deleted successfully"
        })
    except Exception as e:
        abort(500, description=f"Error deleting file: {str(e)}")


@app.errorhandler(400)
def bad_request(error):
    return jsonify({
        'error': 'Bad Request',
        'message': str(error.description)
    }), 400


@app.errorhandler(404)
def not_found(error):
    return jsonify({
        'error': 'Not Found',
        'message': str(error.description)
    }), 404


@app.errorhandler(409)
def conflict(error):
    return jsonify({
        'error': 'Conflict',
        'message': str(error.description)
    }), 409


@app.errorhandler(500)
def server_error(error):
    return jsonify({
        'error': 'Internal Server Error',
        'message': str(error.description)
    }), 500


if __name__ == "__main__":
    print(f"File Manager is running. Files will be stored in '{os.path.abspath(FILES_DIR)}'")
    app.run(debug=True)