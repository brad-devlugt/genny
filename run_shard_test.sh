#!/bin/sh

function teardown() {
    kill -9 `pgrep mongos`
    kill -9 `pgrep mongod`
}

workload="$1"

teardown

rm -rf dbs/

echo "Starting backing mongod..."

for i in {1..3}
do
    datapath="dbs/data_${i}"
    logpath="dbs/logs_${i}"
    port="2700${i}"
    mkdir -p $datapath
    mkdir -p $logpath
    mongod --shardsvr --replSet "shard-replset-${i}" --dbpath $datapath --logpath $logpath/mongod.log --port $port --fork
    mongosh --eval 'rs.initiate()' "mongodb://localhost:${port}"
done


echo "Starting config server..."
mkdir -p dbs/config_data
mkdir -p dbs/config_logs
mongod --configsvr --replSet "config-replset" --dbpath ./dbs/config_data --logpath ./dbs/config_logs/mongod.log --port 27000 --fork

mongosh --eval 'rs.initiate()' "mongodb://localhost:27000"


echo "Starting mongos"
mkdir -p dbs/mongos_data
mkdir -p dbs/mongos_logs
mongos --configdb "config-replset/localhost:27000" --port 27015 --logpath ./dbs/mongos_logs/mongos.log --fork


for i in {1..3}
do
    shard_str="shard-replset-${i}/localhost:2700${i}"
    mongosh --eval "sh.addShard(\"${shard_str}\")" "mongodb://localhost:27015"
done

./run-genny workload -- run -w "$workload" -u "mongodb://localhost:27015"

./run-genny translate ./build/WorkloadOutput/CedarMetrics/ -o ./build/WorkloadOutput/CedarMetrics/out.ftdc

t2 ./build/WorkloadOutput/CedarMetrics/out.ftdc

teardown
