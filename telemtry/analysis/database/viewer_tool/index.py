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
import threading
import uuid
from functools import partial
from werkzeug.middleware.dispatcher import DispatcherMiddleware
from werkzeug.middleware.proxy_fix import ProxyFix


REALTIME = True

sys.path.append(str(Path(__file__).parents[3]))
from flask import Flask, render_template, url_for, request, redirect, jsonify, json, Response, stream_with_context

from analysis.sql_utils.db_handler import DBHandler, DBTarget
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget

config = {}
active_users = {}
os.environ["event_details"] = ""
os.environ["event_id"] = "-1"
os.environ["page_details"] = "index_page"
latest_page_details = os.getenv("page_details")
try:
    os.environ["date_id"] = str(DBHandler.simple_select('SELECT date FROM drive_day ORDER BY day_id DESC LIMIT 1')[0][0])
except IndexError:
    # os.environ["date_id"] = datetime.today().strftime("%Y-%m-%d")
    os.environ["date_id"] = datetime.today().strftime("2024-02-02")

def config_subscribe(client, userdata, msg):
    
    if msg.topic == 'config/event_sync':
        #Convert msg to json object
        msg = json.loads(msg.payload.decode())

        #Store and print return
        os.environ["event_details"] = json.dumps(msg)
        logging.debug("Index Event Details: " + os.getenv("event_details"))
        notify_listeners()

    elif msg.topic == 'config/event_update_sync':
        msg = json.loads(msg.payload.decode())
        print("RECEIVED EVENT UPDATE SYNC: ", msg)
        try:
            json_obj = json.loads(os.getenv("event_details"))
            json_obj.update(msg)
            os.environ['event_details'] = json.dumps(json_obj)
            print("NEW CONFIG FILE: ", msg)
        except Exception as e:
            print(e)
            os.environ['event_details'] = json.dumps(msg)
        notify_listeners()

    elif msg.topic == 'config/page_sync':
        # Convert msg to json object
        msg = msg.payload.decode()
        print("PAGE SYNC", msg)
        # TODO safety, ensure all fields present?
        logging.debug("PAGE PAYLOAD: " + str(msg))
        os.environ["page_details"] = msg
        global latest_page_details
        latest_page_details = msg
        print("PAGE SYNC ", msg)


def mqtt_client_loop(mqtt):
    # Start the MQTT client loop (this will run forever in the background)
    mqtt.client.loop_forever()


def start_background_tasks():
    def mqtt_loop():
        client_id = f'flask_app_prod_{uuid.uuid4()}'
        with MQTTHandler(client_id, target=MQTTTarget.get(), on_message=config_subscribe) as mqtt:
            mqtt.client.loop_forever()
    threading.Thread(target=mqtt_loop, daemon=True).start()

def make_app():
    app = Flask(__name__, static_url_path='/static')

    app.secret_key = "some-super-secret-key"  # TODO change for PROD
    app.config['PREFERRED_URL_SCHEME'] = 'https'
    app.config['APPLICATION_ROOT'] = '/webtool'
    app.config['SESSION_COOKIE_PATH'] = '/webtool'

    # Create login manager, redirect to login page if login fails
    login_manager = LoginManager()
    login_manager.init_app(app)
    login_manager.login_view = 'login'
    USER_DB_PATH = "./users.db"

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

    app.wsgi_app = ProxyFix(app.wsgi_app, x_proto=1, x_host=1)

    app.wsgi_app = DispatcherMiddleware(app.wsgi_app, {
        '/webtool': app.wsgi_app
    })

    config = {}
    latest_event_details = os.environ["event_details"]
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
        if int(event_id := os.getenv("event_id")) > 0 or event_id == "-9999" or os.getenv("event_details"): #TODO more robust check than -1 pls
            logging.debug("Redirecting to Running Event.")
            return redirect(url_for('create_event'))

        try:
            day_id = DBHandler.simple_select('SELECT day_id FROM drive_day ORDER BY day_id DESC LIMIT 1')[0][0]
            logging.debug("DEBUG select day_id returns: " + str(day_id))
            os.environ["day_id"] = str(day_id)
        except (ValueError, IndexError) as e:
            os.environ["day_id"] = "0"

        #If no event running but drive day has been created, set current page to event details config page
        if os.getenv("date_id") == str(date.today()):
            logging.debug("DEBUG: Date ID equal.")
            # day_id = DBHandler.simple_select(table='drive_day', target=DBTarget.get(), user='electric', returning='day_id')

            return redirect(url_for('new_event', day_id=os.getenv("day_id"))) #temporary routing
        else:
            logging.debug("DEBUG: Date ID NOT equal.")

        #No drive day or event running, set current page to index in page_sync and return render template for creating the drive day
        os.environ["page_details"] = "index_page"
        #with MQTTHandler(f'flask_app_{uuid.uuid4()}') as mqtt:
        #    mqtt.publish('config/page_sync', "index_page")
        return render_template('index.html', day_id=os.getenv("day_id"), host_ip=DBTarget.resolve_target(DBTarget.get(client=True)))

    @app.route('/new_drive_day/', methods=['GET'])
    @login_required
    def new_drive_day():
        day_id = DBHandler.insert(table='drive_day', target=DBTarget.get(), user='electric', data=request.args, returning='day_id')
        os.environ["date_id"] = str(date.today())
        os.environ["day_id"] = str(day_id)
        logging.debug("NEW_DRIVE_DAY Reset date_id to: " + os.getenv("date_id"))
        return redirect(url_for('new_event', day_id=day_id))


    @app.route('/new_event/', methods=['GET'])
    @login_required
    def new_event():
        return render_template('input_screen.html', host_ip=DBTarget.resolve_target(DBTarget.get(client=True)), day_id=request.form.get('day_id', request.args['day_id']))


    @app.route('/create_event/', methods=['GET', 'POST'])
    @login_required
    def create_event():

        #! TODO: PROTECT
        print("CREATE EVENT")
        current_date = datetime.today().strftime("%B %d, %Y")
        if request.method == 'POST':
            inputs = request.form.to_dict()
        else:
            return render_template('event_tracker.html', current_date=current_date,
                    host_ip=DBTarget.resolve_target(DBTarget.get(client=True)),
                    event_id = os.getenv("event_id"), config_image = os.getenv("event_details"))
        inputs['status'] = 2
        try:
            last_packet = DBHandler.simple_select('SELECT packet_end FROM event WHERE status = 0 ORDER BY event_id DESC LIMIT 1')[0][0]
        except IndexError as e:
            last_packet = 0
        inputs['packet_start'] = last_packet + 1

        try:
            day_id = DBHandler.simple_select('SELECT day_id FROM drive_day ORDER BY day_id DESC LIMIT 1')[0][0]
            logging.debug("DEBUG select day_id returns: " + str(day_id))
            os.environ["day_id"] = str(day_id)
        except (ValueError, IndexError) as e:
            os.environ["day_id"] = "0"

        print("INPUTS: ", inputs)
        day_id, event_id = DBHandler.insert(table='event', target=DBTarget.get(),
                                            user='electric', data=inputs, returning=['day_id', 'event_id'])
        os.environ["event_id"] = str(event_id)

        logging.debug("DEBUG event_id in create_event assigns: " + str(event_id))

        with MQTTHandler(f'flask_app_{uuid.uuid4()}') as mqtt:
            mqtt.publish('config/flask', json.dumps({'event_id': event_id}, indent=4))
        return render_template('event_tracker.html', current_date=current_date, host_ip=DBTarget.resolve_target(DBTarget.get(client=True)), event_id=os.getenv("event_id"), config_image = os.getenv("event_details"))


    @app.route('/set_event_time/', methods=['POST'])
    @login_required
    def set_event_time():
        if request.json['status'] == 0:
            try:
                request.json['packet_end'] = DBHandler.simple_select('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1')[0][0]
            except IndexError as e:
                request.json['packet_end'] = 1
            with MQTTHandler(f'flask_app_{uuid.uuid4()}') as mqtt:
                mqtt.publish('config/flask', 'end_event')
        DBHandler.set_event_status(**request.json, target=DBTarget.get(), user='electric', returning='day_id')
        return render_template('event_tracker.html', host_ip=DBTarget.resolve_target(DBTarget.get(client=True)), event_id=request.json['event_id'])

    @app.route('/handshake/', methods=['GET'])
    def handshake():
        try:
            last_pack = DBHandler.simple_select('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1')[0][0]
        except IndexError as e:
            last_pack = 0
        return json.dumps({'time': time.time() * 1000, 'last_packet': last_pack}), 200, {'ContentType': 'application/json'}

    @app.route('/reset_config_image', methods=['POST', 'GET'])
    def reset_config_image():
        global latest_page_details
        global latest_event_details
        os.environ["event_details"] = ""
        os.environ["event_id"] = "-1"
        os.environ["page_details"] = "index_page"
        latest_page_details = os.getenv("page_details")
        latest_event_details = os.getenv("event_details")

        # Update correct current page to be new event
        #with MQTTHandler(f'flask_app_{uuid.uuid4()}') as mqtt:
        #    mqtt.publish('config/page_sync', "index_page")

        print("1 - Config Reset, Passing Index")
        return redirect(url_for('index'))
        #return json.dumps({'success': True}), 200, {'ContentType': 'application/json'}

    @app.route('/tune_data', methods=['GET', 'POST'])
    @login_required
    def tune_data():
        data = request.data
        json_object = json.loads(data)
        print(json_object)
        return render_template('texas_tune.html')

    @app.route('/turn_data', methods=['GET', 'POST'])
    @login_required
    def turn_data():
        data = request.data
        json_object = json.loads(data)
        print(json_object)
        return render_template('event_tracker.html', host_ip=DBTarget.resolve_target(DBTarget.get(client=True)))


    @app.route('/accel_data', methods=['GET', 'POST'])
    @login_required
    def accel_data():
        data = request.data
        json_object = json.loads(data)
        logging.debug(json_object)
        return render_template('event_tracker.html', host_ip=DBTarget.resolve_target(DBTarget.get(client=True)))


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
    #! MIGHT NEED TO IMPLEMENT LATER @login_required
    def add_new_lap():
        json_obj = json.loads(os.environ["event_details"])
        print("JSON OBJ", json_obj)
        data = request.form.to_dict()
        if 'time' in data:
            print("TIME IN DATA")
            if 'laps' not in json_obj:
                print("LAPS IN JSON")
                json_obj['laps'] = []

            time_to_append = int(((time.time() * 1000) if REALTIME else data['time']) - (json_obj['timerEventTime']))
            if len(json_obj['laps']) > 0: time_to_append -= json_obj['laps'][-1]

            json_obj['laps'].append(time_to_append)
            print("LAPS: ", json_obj['laps'])
            os.environ['event_details'] = json.dumps(json_obj)
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
    def dashboards():
        with open('../../../net_configs.json') as f:
             targets_dict = json.load(f)
        if targets_dict['CLIENT_TARGET'] == 'LOCAL':
            return redirect("http://localhost:3000")
        return redirect(f"https://lhrelectric.org/grafana")

    @app.route('/logout')
    @login_required
    def logout():
        """
        Logs user out and redirects to login page
        """
        logout_user()
        return redirect(url_for('login'))


    @app.route('/active_users', methods=['GET', 'POST'])
    def update_active_users():
        if request.method == 'POST':
            try:
                # Retrieve the JSON payload sent by the client.
                payload = request.get_json()  # Assumes the client sends Content-Type: application/json
                user_id = payload.get('user_id')

                # Update active heartbeat for the user
                active_users[user_id] = time.time()
                logging.debug(f"Heartbeat received from user {user_id}")

                return jsonify({"success": True}), 200
            except Exception as e:
                logging.error("Error processing heartbeat: " + str(e))
                return jsonify({"error": str(e)}), 400
        # For GET requests, return the count of active users.
        count = count_active_users(timeout=15)
        return jsonify({"active_users": count})

    # Add a new SSE endpoint for event_sync messages
    @app.route('/event-sync-stream')
    def sse_event_sync():
        print("Call to SSE Event Sync")
        return Response(stream_with_context(event_sync_stream()), mimetype="text/event-stream")

    # New SSE endpoint for page sync messages
    @app.route('/page-sync-stream')
    def sse_page_sync():
        print("Call to SSE Page Sync")
        return Response(stream_with_context(page_sync_stream()), mimetype="text/event-stream")

    @app.route('/update-event-sync', methods=['POST'])
    def update_event_sync():
        print("Call to Update Event Sync")
        """
        Endpoint to receive JSON updates from the client.
        """
        global latest_event_details
        # Parse the JSON data from the client
        json_data = request.get_json()

        if json_data.get("endFlag"):
            #Handle event termination logic
            logging.debug("End Flag Detected. Closing event on DB.")

            # Packet Generation for Testing
            # for i in tqdm(range(1, 100)):
            #    DBHandler.insert('packet', target=DBTarget.LOCAL, user='electric', data={'packet_id': i, 'time': int(time.time())})

            #Close event in database
            try:
                last_pack = DBHandler.simple_select('SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1')[0][0]
            except IndexError as e:
                last_pack = 0

            DBHandler.set_event_status(int(os.getenv("event_id")), 0, packet_end=last_pack, user='electric')
            #Reset event variables
            os.environ["event_id"] = "-1"
            os.environ["event_details"] = ""

            #Update target page
            os.environ["page_details"] = "index_page"  # update shared env variable
            global latest_page_details
            latest_page_details = "index_page"

        # Update the global event details. Store it as a JSON-formatted string.
        os.environ['event_details'] = json.dumps(json_data)
        latest_event_details = os.getenv("event_details")

        app.logger.debug("Event sync updated: %s", latest_event_details)
        print("Event Details stores: " + latest_event_details)

        # Respond with a success message.
        return jsonify({"success": True})

    # New endpoint: update the page target based on client POST request
    @app.route('/update-page-target', methods=['POST'])
    def update_page_target():
        print("Call to Update Page Target")
        json_data = request.get_json()
        new_target = json_data.get("target_page")
        if new_target:
            os.environ["page_details"] = new_target  #update shared env variable
            global latest_page_details
            latest_page_details = new_target  #update SSE shared variable
            app.logger.debug("Updated page target: %s", new_target)
            return jsonify({"success": True})
        else:
            return jsonify({"error": "No target_page provided"}), 400

    start_background_tasks()
    return app

def notify_listeners():
    global latest_event_details
    latest_event_details = os.getenv("event_details")

latest_event_details = os.getenv("event_details")

#Generator function for SSE that streams updates of event details.
def event_sync_stream():
    yield "data: ping\n\n"
    global latest_event_details
    last_sent = None
    while True:
        print("LATEST EVENT DETAILS: ", latest_event_details)
        logging.warning(latest_event_details)
        if latest_event_details != last_sent:
            #Yeilds event details as SSE message (with double newline at end)
            yield f"data: {latest_event_details}\n\n"
            last_sent = latest_event_details
        time.sleep(1)  # Adjust the interval as needed

#Generator for SSE page sync updates
def page_sync_stream():
    yield "data: ping\n\n"
    global latest_page_details
    last_sent = None
    while True:
        if latest_page_details != last_sent:
            #CHANGED: Yield new target page as SSE data
            yield f"data: {latest_page_details}\n\n"
            last_sent = latest_page_details
        time.sleep(1)  # Adjust update interval as needed

def count_active_users(timeout=15):
    """Return the number of users with a heartbeat in the last `timeout` seconds."""
    current_time = time.time()
    # Count only those users whose heartbeat is within the timeout window.
    return len(list(filter(lambda last_seen: current_time - last_seen <= timeout, active_users.values())))

app = make_app()

if __name__ == '__main__':
    with MQTTHandler('test', target=MQTTTarget.get(), on_message=config_subscribe) as mqtt:
        mqtt.client.subscribe('config/#')
        mqtt.client.loop_start()

        if os.getenv('IN_DOCKER'):
            app.run(host='127.0.0.1', ssl_context=('/etc/letsencrypts/live/lhrelectric.org/fullchain.pem', '/etc/letsencrypts/live/lhrelectric.org/privkey.pem'))
        else:
            app.run(host='127.0.0.1', debug=False)
