from flask import Flask, jsonify, render_template
import json

app = Flask(__name__)

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/spectrum")
def spectrum():

    try:
        with open("data.json") as f:
            data = json.load(f)

        return jsonify(data)

    except:
        return jsonify({"scan":0,"channels":[]})

if __name__ == "__main__":
    app.run(debug=True)