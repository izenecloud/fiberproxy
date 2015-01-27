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
change this to your influxdb log server address:
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
- HTTP, MSGPACK, RAW and Forward support.
- cross services transaction based on TCC support.
- monitor and log aggregator 
