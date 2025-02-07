import paho.mqtt.client as mqtt
import json
import uuid

class SSCMANodeClient:
    def __init__(self, id="recamera", version="v0"):
        self.id = id
        self.version = version
        self._nodes = []
        self._connected = False
        self._pending_requests = []
        self.mqtt_client = mqtt.Client()
        self.mqtt_client.on_connect = self.on_connect
        self.mqtt_client.on_message = self.on_message

    def on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self._connected = True
            print(f"Connected to MQTT broker! Client ID: {self.id}")  
            self.mqtt_client.subscribe(f"sscma/{self.version}/{self.id}/node/out/#")
        else:
            print(f"Failed to connect, return code {rc}")
    

    def on_message(self, client, userdata, msg):
        try:
            payload = json.loads(msg.payload.decode())
            id = msg.topic.split("/")[-1]
            for node in self._nodes:
                if node.id == id:
                    node.receive(payload)
                    break
        except json.JSONDecodeError:
            print(f"Invalid JSON payload received on topic {msg.topic}: {msg.payload.decode()}")
        
    def request(self, node, action, data):
        
        if node != None:
            topic = f"sscma/{self.version}/{self.id}/node/in/{node.id}"
        else:
            topic = f"sscma/{self.version}/{self.id}/node/in/"
            
        if not self._connected:
            self._pending_requests.append((node, action, data))
            return
        
        pyload = {"type": 3, "name": action, "data": data}
        print("{}:{}".format(topic, pyload))
        self.mqtt_client.publish(topic, json.dumps(pyload))  
        
    def start(self, broker, port):        
        self.mqtt_client.connect(broker, port)
        self.mqtt_client.loop_start()
        while not self._connected:
            pass
        
        self.request(None, "clear", {})
        
        for node in self._nodes:
            node.create()
        
        for request in self._pending_requests:
            self.request(*request)
        
    
    def stop(self):
        for node in self._nodes:
            node.destroy()
            
        self.request(None, "clear", {})
        
        self.mqtt_client.loop_stop()
        self.mqtt_client.disconnect()
        self._connected = False

class Node:    
    def __init__(self, client, id=None):
        self.client = client
        self.id = id if id else str(uuid.uuid4())
        self.dependencies = []
        self.dependents = []
        self.onReceive = None
        self.client._nodes.append(self)

    def sink(self, target_node):
        self.dependents.append(target_node)
        target_node.dependencies.append(self)
        
    def send(self, data):
        for dependent in self.dependents:
            dependent.receive(data)
            
    def request(self, action, data):
        self.client.request(self, action, data)
        
    def receive(self, data):
        if self.onReceive:
            self.onReceive(data)
    
    def destroy(self):
        self.request("destroy", {})
    
    def enable(self):
        self.request("enabled", True)

    def disable(self):
        self.request("enabled", False)
        

class CameraNode(Node):
    
    def __init__(self, client, option='2', audio=True, preview=False):
        super().__init__(client)
        self.option = option
        self.audio = audio
        self.preview = preview
        
    def create(self):
        data = {
            "type": "camera",
            "config": {
                "option": self.option,
                "audio": self.audio,
                "preview": self.preview
            },
            "dependencies": [n.id for n in self.dependencies],  
            "dependents": [n.id for n in self.dependents]       
        }
        self.request("create", data)

class ModelNode(Node):
    
    def __init__(self, client, uri="", tscore=0.45, tiou=0.35, topk=0, labels=None, debug=False, audio=True, trace=False,
                 counting=False, splitter=None):
        super().__init__(client)
        self.uri = uri
        self.tscore = tscore
        self.tiou = tiou
        self.topk = topk
        self.labels = labels or []
        self.debug = debug
        self.audio = audio
        self.trace = trace
        self.counting = counting
        self.splitter = splitter or []
        
    def create(self):
        data = {
            "type": "model",
            "config": {
                "uri": self.uri,
                "tscore": self.tscore,
                "tiou": self.tiou,
                "topk": self.topk,
                "labels": self.labels,
                "debug": self.debug,
                "audio": self.audio,
                "trace": self.trace,
                "counting": self.counting,
                "splitter": self.splitter
            },
            "dependencies": [n.id for n in self.dependencies],  
            "dependents": [n.id for n in self.dependents]       
        }
        self.request("create", data)



class StreamingNode(Node):
    
    def __init__(self, client, protocol="rtsp", port=8054, session="live", user="admin", password="admin"):
        super().__init__(client)
        self.protocol = protocol
        self.port = port
        self.session = session
        self.user = user
        self.password = password
        
        
    def create(self):
        data = {
            "type": "stream",
            "config": {
                "protocol": self.protocol,
                "port": self.port,
                "session": self.session,
                "user": self.user,
                "password": self.password
            },
            "dependencies": [n.id for n in self.dependencies],  
            "dependents": [n.id for n in self.dependents]       
        }
        self.request("create", data)

class SaveNode(Node):
    
    def __init__(self, client, storage="local", duration=-1, slice_time=300):
        super().__init__(client)
        self.storage = storage
        self.duration = duration
        self.slice_time = slice_time
        
    def create(self):
        data = {
            "type": "save",
            "config": {
                "storage": self.storage,
                "duration": self.duration,
                "slice":self.slice_time,
                "enabled": True
            },
            "dependencies": [n.id for n in self.dependencies],  
            "dependents": [n.id for n in self.dependents]       
        }
        self.request("create", data)
        

def model_message_handler(data):
    print(f"Received data: {data}")
    
def camera_message_handler(data):
    print(f"Received data: {data}")
    
def stream_message_handler(data):
    print(f"Received data: {data}")

def save_message_handler(data):
    print(f"Received data: {data}")

if __name__ == "__main__":
    client = SSCMANodeClient("recamera", "v0")
    
    camera = CameraNode(client)
    model = ModelNode(client, tiou=0.25, tscore=0.45) 
    stream = StreamingNode(client, protocol="rtsp", port=8554, session="live", user="admin", password="admin")
    saving = SaveNode(client, storage="local", slice_time=300)
    
    camera.sink(model) # connect camera to model
    camera.sink(stream) # connect camera to stream
    camera.sink(saving) # connect camera to saving
    
    camera.onReceive = camera_message_handler
    model.onReceive =  model_message_handler
    stream.onReceive = stream_message_handler
    saving.onReceive = save_message_handler
    
    client.start("192.168.42.1", 1883)
    
    
    print("Enter any key to exit...")
    
    try:
        input()
    finally:
        client.stop()
    
    
    