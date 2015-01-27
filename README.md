# fiberproxy
The fiber-based proxy for the micro services.

## Dependency
- gcc 4.7 or above
- boost 1.56 or above
- izenelib in izenecloud
- consul service 
- influxdb log service

## Build
Build with c++11 support.
<pre>
mkdir build
cd build
cmake ../source
make -j4
</pre>

## Configure
Modify the config.xml under the bin directory. Currently the zookeeper and HDFS configure is not used.

Change your influxdb log server address:

<pre>
    LogServerConnection host="172.16.5.168" port="7000" log_service="influxlog" log_tag="dev"
</pre>
Change the consul service address:
<pre>
    ServiceDiscovery servers="10.10.99.131:8500,10.10.103.131:8500"
</pre>

## Feature
- fiber based
- multi services in a single call
- auto discovery services
- load-balance and auto-retry on failure.
- HTTP, MSGPACK and Forward support.
- cross services transaction based on TCC support.
- monitor and log aggregator 

## Usage

### Deploy consul service
Download the consul from official and start them on all the machines that the micro service may be deployed to form the service registry cluster.

### Register your micro services to consul
Given a register file reg.json below:
<pre>
{
  "ID":"your-service-id",
  "Name":"your-service-name",
  "Tags":["http", "dev"],
  "Port":18282,
  "Check":
  {
    "Script":"curl -s -d \"{}\" http://localhost:18282/commands/check_alive",
    "Interval":"30s"
  }
}
</pre>
Note the Tags include the protocol the service supported and the cluster name the service deployed. The cluster name is used to allow the different fiber-proxy-server for different cluster (Only service in the same cluster will be proxied).

The check script for consul can be any that supported by the consul. It is recommended for the micro service to implement the same check HTTP API to simplify the configure.

### Call the service using the proxy
#### HTTP API
For single service, just replace the original path with the following.
<pre>
/commands/call_single_service_async/service_name/service_api
</pre>
The response will be the same. If the service is not available , error will return as below:
<pre>
{
  "errors":["Service Not Found."],
  "header":
  {
    "success":false
  }
}
</pre>
For multi services, you should call the API with filled POST data as below:
<pre>
/commands/call_services_async
</pre>
<pre>
{
  "call_api_list":
  [
  {
      "service_name":"sf1r",
      "service_cluster":"dev",
      "service_type":2,
      "service_req_data":"{\"collection\":\"hotelo\", \"header\":{\"controller\":\"documents\",\"action\":\"get_doc_count\"}}"
  },
  {
      "service_name":"local_test",
      "service_cluster":"dev",
      "service_type":2,
      "service_req_data":"{}"
  },
  {
      "service_name":"sf1r",
      "service_cluster":"dev",
      "service_api":"/sf1r/documents/get_doc_count",
      "service_type":0,
      "enable_cache":true,
      "service_req_data":"{\"collection\":\"hotelo\"}"
  }
  ]
}
</pre>
`service_type`： indicated the micro service protocal, 0 HTTP, 1 MSGPACK-RPC, 2 RAW, 3 custom.

The response :
<pre>
{
  "header":{"success":true},
  "service_rsp_list":
  [
    {
      "is_cached":false,
      "service_error":"Service Not Found.",
      "service_name":"sf1r",
      "service_rsp":""
    },
    {
      "is_cached":false,
      "service_name":"local_test",
      "service_rsp":"local_test"
    },
    {
      "is_cached":false,
      "service_error":"Send Service Request Failed. ",
      "service_name":"sf1r",
      "service_rsp":""
    }
  ]
}
</pre>

#### Msgpack-RPC call
For single rpc method, just replace the origin method with:
</pre>
call_single_service_async/your_service_name/your_rpc_method
</pre>

#### Port forward
For some private protocol, you can use the port forward to proxy the micro service to balance the request to the all the back-end servers.

The port forward is managed by the consul service, add a port forward as below:
<pre>
curl -X PUT -d "service-name,dev,custom" http://consul-server-ip:8500/v1/kv/fibp-forward-port/10006
</pre>

Note it need be registered under /v1/kv/fibp-forward-port/ to allow the auto discovery for the forward port. All the request to the specific port on the fiber proxy server will be forward to the registered service.

Delete the forward：
<pre>
curl -X DELETE http://consul-server-ip:8500/v1/kv/bop-forward-port/10006
</pre>

### Transaction API for multi services
To support the transaction api cross services, each micro service api in the transaction should implement the api with TCC support(this requires the api support the cancel and confirm action).
For the transaction API, the response should include the extend field:
<pre>
{
  "transaction_id":"xxxx",
  ...
}
</pre>
The cancel and confirm API should be `your_transaction_api/cancel` and `your_transaction_api/confirm` and POST data will be :
<pre>
{
  "transaction_id":"xxx"
}
</pre>
For client, add the transaction flag in the Post data:
<pre>
{
  "do_transaction":true,
  "call_api_list":
  ...
}
</pre>

### Log 
All the micro services latency and status will be send to the influxdb specificed in the configure.
The format will be :
<pre>
[
  {
    "name": "fibp_api_log",
    "columns": ["time", "logid", "start_time", "end_time", "latency"],
    "points": [
      [1400425947368, 1, 1400425947360, 1400425947365, 5]
    ]
  },
  {
    "name": "fibp_services_log",
    "columns": ["time", "logid", "start_time", "end_time", "latency", "service_name", "host_port", "is_fail", "failed_msg"],
    "points": [
    ]
  }
]
</pre>
You can view them using the grafana or some other UI tools.
