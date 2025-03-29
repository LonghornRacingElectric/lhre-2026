import json
import logging
import os
import sys
import time
from pathlib import Path
from tqdm import tqdm
from datetime import date, datetime
from flask_login import LoginManager, UserMixin, login_user, current_user, logout_user, login_required
import sqlite3
from flask_bcrypt import check_password_hash, generate_password_hash

sys.path.append(str(Path(__file__).parents[3]))
from flask import Flask, render_template, url_for, request, redirect

from analysis.sql_utils.db_handler import DBHandler, DBTarget
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget

app = Flask(__name__)
app.secret_key = "some-super-secret-key"
config = {}
os.environ["event_details"] = ""
os.environ["event_id"] = "-1"
os.environ["page_details"] = ""
#os.environ["date_id"] = DBHandler.simple_select('SELECT date FROM drive_day ORDER BY day_id DESC LIMIT 1')[0][0]
os.environ["date_id"] = "2025-02-16" #TODO revert to above for deployment. Testing only
#Create login manager, redirect to login page if login fails
login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = 'login'
USER_DB_PATH = "users.db"

class User(UserMixin):
    def __init__(self, id, username, password_hash):
        self.id = id
        self.username = username
        self.password_hash = password_hash

    def __repr__(self):
        return f"<User {self.username}>"

@login_manager.user_loader
def load_user(user_id):
    """
    Given a user_id, return the associated user object  from storage.
    """
    conn = sqlite3.connect(USER_DB_PATH)
    c = conn.cursor()
    c.execute('SELECT id, username, password_hash FROM users WHERE id = ?', (user_id,))
    row = c.fetchone()
    conn.close()

    if row:
        # row = (id, username, password_hash)
        return User(*row)
    return None

def config_subscribe(client, userdata, msg):
    if msg.topic == 'config/event_sync':
        #Convert msg to json object
        msg = json.loads(msg.payload.decode())

        #Check for end event flag
        if "endFlag" in msg:
            logging.debug("End Flag Detected. Closing event on DB.") #TODO remove, debug only

            #Packet Generation for Testing
            #for i in tqdm(range(1, 100)):
            #    DBHandler.insert('packet', target=DBTarget.LOCAL, user='electric', data={'packet_id': i, 'time': int(time.time())})

            #Close the event in the database
            try:
                last_pack = DBHandler.simple_select('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1')[0][0]
            except IndexError as e:
                last_pack = 0
            DBHandler.set_event_status(int(os.getenv("event_id")), 0, packet_end=last_pack, user='electric')
            #Re-set event ID
            os.environ["event_id"] = "-1"

        #Store and print return
        os.environ["event_details"] = json.dumps(msg)
        logging.debug("Index Event Details: " + os.getenv("event_details"))

    elif msg.topic == 'config/page_sync':
        # Convert msg to json object
        msg = msg.payload.decode()
        # TODO safety, ensure all fields present?
        logging.debug("PAGE PAYLOAD: " + str(msg))
        os.environ["page_details"] = msg

def mqtt_client_loop(mqtt):
    # Start the MQTT client loop (this will run forever in the background)
    mqtt.client.loop_forever()


config = {}

@app.route('/index', methods=['GET'])
@login_required
def index():
    #Print Environs for Debug
    logging.debug("PING Index Call, Event Details: " + os.getenv("event_details")) # TODO Remove, DEBUG only
    logging.debug("RES Today is: " + str(date.today()) + " | And date_id stores: " + os.getenv("date_id"))

    #Check if there is an active event
    try:
        os.environ["event_id"] = str(DBHandler.simple_select('SELECT event_id FROM event WHERE status = 1 ORDER BY event_id DESC LIMIT 1')[0][0])
        logging.debug("event_id Database Select returns: " + os.getenv("event_id"))
    except (ValueError, IndexError) as e:
        #When in debug, set to debug event id
        if logging.getLogger().getEffectiveLevel() == logging.DEBUG:
            os.environ["event_id"] = "-9999"
        logging.debug("event_id Database Select 'event-running' check failed with error: " + str(e))
    #If an event is currently active, redirect to its page
    if os.getenv("event_id") != "-1" or os.getenv("event_details"): #TODO more robust check than -1 pls
        logging.debug("Redirecting to Running Event.")
        return redirect(url_for('create_event'))

    #If no event running but drive day has been created, set current page to event details config page
    if os.getenv("date_id") == str(date.today()):
        logging.debug("DEBUG: Date ID equal.")
        #day_id = DBHandler.simple_select(table='drive_day', target=os.getenv('SERVER_TARGET', DBTarget.LOCAL), user='electric', returning='day_id')
        day_id = DBHandler.simple_select('SELECT day_id FROM drive_day ORDER BY day_id DESC LIMIT 1')[0][0]
        logging.debug("DEBUG select day_id returns: " + str(day_id))

        os.environ["day_id"] = str(day_id)
        return redirect(url_for('new_event', day_id=os.getenv("day_id"), method='new')) #temporary routing
    else:
        logging.debug("DEBUG: Date ID NOT equal.")

    #No drive day or event running, set current page to index in page_sync and return render template for creating the drive day
    with MQTTHandler('flask_app') as mqtt:
        mqtt.publish('config/page_sync', "index_page")
    return render_template('index.html')


@app.route('/new_drive_day/', methods=['GET'])
@login_required
def new_drive_day():
    day_id = DBHandler.insert(table='drive_day', target=os.getenv('SERVER_TARGET', DBTarget.LOCAL), user='electric', data=request.args, returning='day_id')
    os.environ["date_id"] = str(date.today())
    os.environ["day_id"] = str(day_id)
    logging.debug("NEW_DRIVE_DAY Reset date_id to: " + os.getenv("date_id"))
    return redirect(url_for('new_event', day_id=day_id, method='new'))


@app.route('/new_event/', methods=['GET'])
@login_required
def new_event():
    with MQTTHandler('flask_app') as mqtt:
        mqtt.publish('config/page_sync', "new_event_page")

    return render_template('input_screen.html', day_id=request.form.get('day_id', request.args['day_id']))


@app.route('/create_event/', methods=['POST', 'GET'])
@login_required
def create_event():
    current_date = datetime.today().strftime("%B %d, %Y")
    if request.method == 'POST':
        inputs = request.form.to_dict()
    else:
        return render_template('event_tracker.html', current_date=current_date,
                host_ip=DBTarget.resolve_target(os.getenv('SERVER_TARGET', DBTarget.LOCAL)),
                event_id = os.getenv("event_id"), config_image = os.getenv("event_details"))
    inputs['status'] = 2
    try:
        last_packet = DBHandler.simple_select('SELECT packet_end FROM event WHERE status = 0 ORDER BY event_id DESC LIMIT 1')[0][0]
    except IndexError as e:
        last_packet = 0
    inputs['packet_start'] = last_packet + 1
    day_id, event_id = DBHandler.insert(table='event', target=os.getenv('SERVER_TARGET', DBTarget.LOCAL),
                                        user='electric', data=inputs, returning=['day_id', 'event_id'])
    os.environ["event_id"] = str(event_id)

    logging.debug("DEBUG event_id in create_event assigns: " + str(event_id))

    with MQTTHandler('flask_app') as mqtt:
        mqtt.publish('config/flask', json.dumps({'event_id': event_id}, indent=4))
    return render_template('event_tracker.html', current_date=current_date, host_ip=DBTarget.resolve_target(os.getenv('SERVER_TARGET',
                             DBTarget.LOCAL)), event_id=os.getenv("event_id"), config_image = os.getenv("event_details"))


@app.route('/set_event_time/', methods=['POST'])
@login_required
def set_event_time():
    if request.json['status'] == 0:
        try:
            request.json['packet_end'] = DBHandler.simple_select('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1')[0][0]
        except IndexError as e:
            request.json['packet_end'] = 1
        with MQTTHandler('flask_app') as mqtt:
            mqtt.publish('config/flask', 'end_event')
    DBHandler.set_event_status(**request.json, target=os.getenv('SERVER_TARGET', DBTarget.LOCAL), user='electric', returning='day_id')
    return render_template('event_tracker.html', host_ip=DBTarget.resolve_target(os.getenv('SERVER_TARGET', DBTarget.LOCAL)), event_id=request.json['event_id'])


@app.route('/reset_config_image', methods=['POST', 'GET'])
@login_required
def reset_config_image():
    os.environ['event_details'] = ""

    # Update correct current page to be new event
    with MQTTHandler('flask_app') as mqtt:
        mqtt.publish('config/page_sync', "index_page")

    logging.debug("Config image reset. Event has ended. Redirect to follow.")
    return redirect(url_for('index'))
    #return json.dumps({'success': True}), 200, {'ContentType': 'application/json'}

@app.route('/tune_data', methods=['GET', 'POST'])
@login_required
def tune_data():
    data = request.data
    json_object = json.loads(data)
    print(json_object)
    return render_template('texas_tune.html')


@app.route('/verify_page/<string:cur_page>', methods=['GET', 'POST'])
@login_required
def verify_page(cur_page):
    logging.debug("cur_page is: " + cur_page)
    logging.debug("current page_details is: " + os.getenv("page_details"))

    storedPage = os.getenv("page_details")

    #Check against the current stored page
    if cur_page == storedPage:
        #If already on correct page, do not change
        logging.debug("Client on Correct Page")
        return '', 204
    else:
        logging.debug("Client NOT on Correct Page. Redirect to follow.")
        #If page is wrong, redirect to the right page
        if storedPage == "new_event_page":
            logging.debug("NOTIF (Debug) Server Day-ID stores: " + os.getenv("day_id") + " and is about to hand off redirect.");
            return redirect(url_for('new_event', day_id=os.getenv("day_id"), method='new')) #temporary routing
        elif storedPage == "running_event_page":
            return redirect(url_for('create_event'))
        elif storedPage == "index_page":
            return redirect(url_for('index'))

    #No case triggered, error
    return '', 404


@app.route('/turn_data', methods=['GET', 'POST'])
@login_required
def turn_data():
    data = request.data
    json_object = json.loads(data)
    print(json_object)
    return render_template('event_tracker.html', host_ip=DBTarget.resolve_target(os.getenv('SERVER_TARGET', DBTarget.LOCAL)))


@app.route('/accel_data', methods=['GET', 'POST'])
@login_required
def accel_data():
    data = request.data
    json_object = json.loads(data)
    logging.debug(json_object)
    return render_template('event_tracker.html', host_ip=DBTarget.resolve_target(os.getenv('SERVER_TARGET', DBTarget.LOCAL)))


@app.route('/texas_tune/', methods=['GET', 'POST'])
@login_required
def vcu_parameters():
    if request.method == 'POST':
        print(request)
    elif request.method == 'GET':
        return render_template('texas_tune.html')


@app.route('/gates/', methods=['POST'])
@login_required
def create_gates():
    pass

@app.route('/new_lap/', methods=['POST'])
@login_required
def add_new_lap():
    if 'laps' not in config:
        config['laps'] = []
    data = request.form
    if 'time' in data:
        config['laps'].append(data['time'])
        notify_listeners()
    return json.dumps({'success':True}), 200, {'ContentType':'application/json'}

@app.route('/login', methods=['GET', 'POST'])
def login():
    """
    Shows login form and handles user submissions
    """
    if request.method == "POST":
        username = request.form.get("username", "")
        password = request.form.get("password", "")

        # Query the DB for the user
        conn = sqlite3.connect(USER_DB_PATH)
        c = conn.cursor()
        c.execute('SELECT id, username, password_hash FROM users WHERE username = ?', (username,))
        row = c.fetchone()
        conn.close()

        if row:
            user_id, db_username, db_password_hash = row

            # Check the password
            if check_password_hash(db_password_hash, password):
                user_obj = User(user_id, db_username, db_password_hash)
                login_user(user_obj)
                return redirect(url_for("splash"))
        return render_template("login.html", error="Invalid Credentials")

    return render_template("login.html")

@app.route('/splash')
@app.route('/')
@login_required
def splash():
    current_date = datetime.today().strftime("%B %d, %Y")  # Example: "March 9, 2025"
    return render_template('splash.html', current_date=current_date)

@app.route('/dashboards')
@login_required
def dashboards():
    return redirect('https://lhrelectric.org/grafana')

@app.route('/logout')
@login_required
def logout():
    """
    Logs user out and redirects to login page
    """
    logout_user()
    return redirect(url_for('login'))

def notify_listeners():
    print(config)
    with MQTTHandler('flask_app') as mqtt:
        mqtt.publish('config/event_sync', json.dumps(config, indent=4))

if __name__ == '__main__':
    logging.debug("MAIN START. Today is: " + os.getenv("date_id"))

    with MQTTHandler('test', target=MQTTTarget.LOCAL, on_message=config_subscribe) as mqtt:
        mqtt.client.subscribe('config/+') #TODO remove '+' if not necessary
        mqtt.client.loop_start()

        if os.getenv('IN_DOCKER'):
            app.run(host='0.0.0.0', ssl_context=('./ssl/fullchain.pem', './ssl/privkey.pem'))
        else:
            app.run(host='0.0.0.0', debug=False)
