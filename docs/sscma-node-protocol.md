# SSCMA Node Protocol 

## Overviews

This protocol describes the topic format and interaction methods for devices communicating via the MQTT protocol in the SSCMA system. Nodes in the SSCMA system exchange data through specific topics, and clients can perform operations by sending requests and receiving responses.

## Topic Format

The communication topic format is as follows:

- **\<version\>**: Protocol version, currently **v0**.
- **\<client_id\>**: Unique identifier for the device or client.
- **\<direction\>**: Data flow direction, with two possible values:
  - **in**: Service request interface. Clients send commands to the server.
  - **out**: Service response interface. The server returns results or statuses to clients.
- **\<node_id\>**: ID of the node being operated on.

## Example Topics

- **`sscma/v0/recamera/node/in/12345`**: Request sent from a client to a node.
- **`sscma/v0/recamera/node/out/12345`**: Response or status information from the node.

## Frame Formats

### Request Frame

Used by clients to send operation requests to the server via the `in` topic.

- **`type`**: Message type. **3** indicates a request frame.
- **`name`**: Action name (e.g., `action`), identifying the request type.
- **`data`**: Request data containing operation-specific parameters.

### Response Frame

Sent by the server in response to a request via the `out` topic.

- **`type`**: Message type. **1** indicates a response frame.
- **`name`**: Action name (e.g., `action`), matching the request type.
- **`code`**: Operation code. **0** indicates success; other values indicate errors.
- **`data`**: Response data containing operation-specific parameters.

### Event Frame

Used by the server to notify clients of events via the `out` topic.

- **`type`**: Message type. **1** indicates a response frame.
- **`name`**: Action name (e.g., `action`), identifying the event type.
- **`code`**: Operation code. **0** indicates success; other values indicate errors.
- **`data`**: Event data containing relevant parameters.

### Log Frame

Used by the server to send system logs or status information via the `out` topic.

- **`type`**: Message type. **3** indicates a log frame.
- **`name`**: Fixed as **`LOG`** to identify log messages.
- **`code`**: Log status code. **0** indicates no exceptions.
- **`data`**: Log content as a string.

---

## Node Management

### Create Node

Send a request via the `in` topic with the `node_id` as the unique identifier.

- **`type`**: Fixed as **3** (request).
- **`name`**: Fixed as **`create`**.
- **`data.type`**: Service type (e.g., `camera`, `model`, `stream`, `save`).
- **`data.config`**: Service-specific configuration.

### Destroy Node

Send a request via the `in` topic with the `node_id` as the unique identifier.

- **`type`**: Fixed as **3** (request).
- **`name`**: Fixed as **`destroy`**.
- **`data`**: Typically empty for destroy operations.

### Control Node

Send a request via the `in` topic with the `node_id` as the unique identifier.

- **`type`**: Fixed as **3** (request).
- **`name`**: Control action name (e.g., `config`).
- **`data`**: Action-specific configuration.

---

## Image Service

### Create Node

#### Request Parameters

| Parameter | Type      | Description                     |
|-----------|-----------|---------------------------------|
| `option`  | `int`     | Enum value                      |
| `audio`   | `bool`    | Enable audio recording (default: `true`) |
| `preview` | `bool`    | Enable preview (default: `false`) |

#### Response Parameters

*No parameters.*

#### Example

**Request Topic:** `sscma/v0/recamera/node/in/12345`  
**Response Topic:** `sscma/v0/recamera/node/out/12345`

### Destroy Node

#### Request/Response Parameters

*No parameters.*

#### Example

**Request Topic:** `sscma/v0/recamera/node/in/12345`

### Enable Node

#### Request Parameters

| Parameter | Type      | Description |
|-----------|-----------|-------------|
| `data`    | `*bool*`  | Enable/disable state |

#### Response Parameters

*No parameters.*

#### Example

**Request Topic:** `sscma/v0/recamera/node/in/12345`

---

## Model Service

### Create Node

#### Request Parameters

| Parameter   | Type          | Description                          |
|-------------|---------------|--------------------------------------|
| `uri`       | `*string*`    | Model path (empty for default)       |
| `tscore`    | `*int*`       | Confidence threshold                 |
| `tiou`      | `*int*`       | IOU threshold                        |
| `topk`      | `*int*`       | Top-K threshold                      |
| `labels`    | `*string[]*`  | Target labels                        |
| `debug`     | `*bool*`      | Enable image output                  |
| `audio`     | `*bool*`      | Enable audio recording (default: `true`) |
| `trace`     | `*bool*`      | Enable target tracking (default: `false`) |
| `counting`  | `*bool*`      | Enable object counting (default: `false`) |
| `splitter`  | `*int[4]*`    | Object counting split line           |

#### Response Parameters

*No parameters.*

#### Example

**Request Topic:** `sscma/v0/recamera/node/in/12345`

### Control Node (Config)

#### Request Parameters

| Parameter   | Type          | Description                  |
|-------------|---------------|------------------------------|
| `tscore`    | `*int*`       | Confidence threshold         |
| `tiou`      | `*int*`       | IOU threshold                |
| `topk`      | `*int*`       | Top-K threshold              |
| `labels`    | `*string[]*`  | Target labels                |
| `debug`     | `*bool*`      | Enable image output          |
| `trace`     | `*bool*`      | Enable target tracking       |
| `counting`  | `*bool*`      | Enable object counting       |
| `splitter`  | `*int[4]*`    | Object counting split line   |

#### Response Parameters

*No parameters.*

---

## Streaming Service

### Create Node

#### Request Parameters

| Parameter   | Type      | Description                |
|-------------|-----------|----------------------------|
| `protocol`  | `int`     | Protocol type              |
| `port`      | `*int*`   | Streaming port             |
| `session`   | `*string*`| Streaming session ID       |
| `user`      | `*string*`| Login username             |
| `password`  | `*string*`| Login password             |

#### Response Parameters

*No parameters.*

#### Example

**Request Topic:** `sscma/v0/recamera/node/in/12345`

---

## Storage Service

### Create Node

#### Request Parameters

| Parameter   | Type      | Description                          |
|-------------|-----------|--------------------------------------|
| `storage`   | `int`     | Storage location (0: local, 1: external) |
| `duration`  | `int`     | Duration in seconds (0 for unlimited) |
| `slice`     | `int`     | Slice time in seconds                |
| `start`     | `bool`    | Start immediately                    |

#### Response Parameters

*No parameters.*

#### Example

**Request Topic:** `sscma/v0/recamera/node/in/12345`