# reCamera SSCMA Node Protocol Documentation
## Overview
This protocol describes the topic format and interaction methods for devices to communicate via the MQTT protocol within the SSCMA system. Nodes in the SSCMA system exchange data through specific topics. Clients can complete various operations by sending requests and receiving responses.

## Topic Format
The format of the communication topic is:
`sscma/<version>/<client_id>/node/<direction>/<node_id>`
- `<version>`: Protocol version. Currently, it is `v0`.
- `<client_id>`: A unique identifier for the device or client.
- `<direction>`: Data flow direction, with the following two values:
  - `in`: Service request interface. The client sends instructions to the server.
  - `out`: Service response interface. The server returns results or status to the client.
- `<node_id>`: The ID of the node for the specified operation.

## Example Topics
- `sscma/v0/recamera/node/in/12345`: A request sent by the client to the node.
- `sscma/v0/recamera/node/out/12345`: The response or status information of the node.

## Frame Format
### Request Frame
The request frame is used by the client to initiate operation requests to the server and is usually sent to the specified node via the `in` topic.
```json
{
    "type": 3,
    "name": "action",
    "data": {
        "xxx": "xxx",
        ...
    }
}
```
- `type`: Message type. `3` indicates a request frame.
- `name`: The name of the requested action, such as `action`, used to identify the request type.
- `data`: Request data, containing specific operation parameters.

### Response Frame
The response frame is the server's response to the request frame and is usually sent to the client via the `out` topic.
```json
{
    "type": 1,
    "name": "action",
    "code": 0,
    "data": {
        "xxx": "xxx",
        ...
    }
}
```
- `type`: Message type. `1` indicates a response frame.
- `name`: The name of the requested action, such as `action`, used to identify the request type.
- `code`: Operation code. Usually, `0` indicates success, and other values indicate error codes or exceptions.
- `data`: Request data, containing specific operation parameters.

### Event Frame
The event frame is used by the server to send event notifications to the client. The client can receive event information by subscribing to the `out` topic.
```json
{
    "type": 2,
    "name": "action",
    "code": 0,
    "data": {
        "xxx": "xxx",
        ...
    }
}
```
- `type`: Message type. `2` indicates an event frame.
- `name`: The name of the requested action, such as `action`, used to identify the request type.
- `code`: Operation code. Usually, `0` indicates success, and other values indicate error codes or exceptions.
- `data`: Request data, containing specific operation parameters.

### Log Frame
The log frame is used by the server to transmit system logs or status information to the client, typically for debugging and monitoring purposes.
```json
{
    "type": 3,
    "name": "LOG",
    "code": 0,
    "data": "All is well"
}
```
- `type`: Message type. `3` indicates a log frame.
- `name`: Fixed as `LOG`, identifying this as a log message.
- `code`: Log status code. Usually, `0` indicates no exceptions.
- `data`: Log content, provided as a string.

### Node Management
The creation, destruction, and control of all services and node operations follow the same general structure.
#### Create Node
Send a request via the `in` topic, with `node_id` as the unique identifier.
```json
{
    "type": 3,
    "name": "create",
    "data": {
        "type": "<node_type>", // Node type
        "config": { ... }, // Configuration information
        "dependencies": [...], // Dependent nodes
        "dependents": [...], // Nodes that depend on this one
    }
}
```
- `type`: Fixed as `3`, indicating a request.
- `name`: Fixed as `create`.
- `data.type`: Service type, passed according to different services, such as `camera`, `model`, `stream`, `save`.
- `data.config`: Specific configuration of the service, which varies depending on the service type.

#### Destroy Node
Send a request via the `in` topic, with `node_id` as the unique identifier.
```json
{
    "type": 3,
    "name": "destroy",
    "data": ""
}
```
- `type`: Fixed as `3`, indicating a request.
- `name`: Fixed as `destroy`.
- `data`: Usually, no input is required to destroy a node.

#### Control Node
Send a request via the `in` topic, with `node_id` as the unique identifier.
```json
{
    "type": 3,
    "name": "<action_type>", // Specific action, such as config
    "data": {....}, // Parameters required for the action
}
```
- `type`: Fixed as `3`, indicating a creation request.
- `name`: Passed according to the control actions provided by the specific service, such as `config`.
- `data`: Specific configuration of the action, which varies depending on the service type.

## Image Service
### Create Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| option | int | Enumerated value |
| audio | bool:true | Whether to enable audio recording |
| preview | bool:false | Whether to enable preview |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "create",
"data": {
"type": "camera",
"config": {
           "option": 0
       }
}
}
```
Response: `sscma/v0/recamera/node/out/12345`
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

### Destroy Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
    "type": 3,
    "name": "destroy",
    "data": "",
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

#### Enable (enabled)
##### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| data | bool |  |

##### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

##### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "enabled",
"data": true
}
```
Response:
```json
{
"type": 1,
"name": "enabled",
"code": 0,
"data": true
}
```

## Model Service
### Create Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| uri | string | Model path: Empty for the default path |
| tscore | int | Confidence threshold |
| tiou | int | IOU threshold |
| topk | int | Quantity threshold |
| labels | string[] | Target labels |
| debug | bool | Whether to output images |
| audio | bool:true | Whether to record audio |
| trace | bool:false | Whether to track the target |
| counting | bool:false | Whether to count the targets |
| splitter | int[4] | Target counting split line |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "create",
"data": {
"type": "camera",
"config": {
           "uri": "test.model",
           "debug": "true"
}
}
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": {
        "model_id": "0",
        "classes": []
   }
}
```

### Destroy Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
    "type": 3,
    "name": "destroy",
    "data": "",
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

### Control Node
#### Control Commands
| Parameter | Description |
|---|---|
| enabled | Enable |
| config | Configure |

#### Configure (config)
##### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| tscore | int | Confidence threshold |
| tiou | int | IOU threshold |
| topk | int | Quantity threshold |
| labels | string[] | Target labels |
| debug | bool | Whether to output images |
| trace | bool | Whether to track the target |
| counting | bool | Whether to count the targets |
| splitter | int[4] | Target counting split line |

##### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

##### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "config",
"data": {
        "debug": "true"
}
}
```
Response:
```json
{
"type": 1,
"name": "config",
"code": 0,
"data": ""
}
```

#### Enable (enabled)
##### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| data | bool |  |

##### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

##### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "enabled",
"data": true
}
```
Response:
```json
{
"type": 1,
"name": "enabled",
"code": 0,
"data": true
}
```

## Streaming Service
### Create Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| protocol | int | Protocol type |
| port | int | Streaming port |
| session | string | Streaming session |
| user | string | Login user |
| password | string | Login key |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
    "type": 3,
    "name": "destroy",
    "data": "",
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

### Destroy Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
    "type": 3,
    "name": "destroy",
    "data": "",
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

#### Control Commands
| Parameter | Description |
|---|---|
| enabled | Enable |

#### Enable (enabled)
##### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| data | bool |  |

##### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

##### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "enabled",
"data": true
}
```
Response:
```json
{
"type": 1,
"name": "enabled",
"code": 0,
"data": true
}
```

## Saving Service
### Create Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| storage | int | Storage address<br/>0: Local device<br/>1: External storage |
| duration | int | Duration. 0 for continuous, in seconds |
| slice | int | Slicing time, in seconds |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
    "type": 3,
    "name": "create",
    "data": {
        "type": "save",
        "config":{
            "storage": "local",
            "duration": -1,
            "slice": 120,
        }
    },
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

### Destroy Node
#### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

#### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
    "type": 3,
    "name": "destroy",
    "data": "",
}
```
Response:
```json
{
"type": 1,
"name": "create",
"code": 0,
"data": ""
}
```

### Control Node
#### Control Commands
| Parameter | Description |
|---|---|
| enabled | Enable |

#### Enable (enabled)
##### Request Parameters
| Parameter | Type | Description |
|---|---|---|
| data | bool |  |

##### Response Parameters
| Parameter | Type | Description |
|---|---|---|
| None |  |  |

##### Usage Example
Request: `sscma/v0/recamera/node/in/12345`
```json
{
"type": 3,
"name": "enabled",
"data": true
}
```
Response:
```json
{
"type": 1,
"name": "enabled",
"code": 0,
"data": true
}
```