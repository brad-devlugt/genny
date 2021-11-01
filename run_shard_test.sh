#!/bin/sh

function teardown() {
    kill -9 `pgrep mongos`
    kill -9 `pgrep mongod`
}

workload="$1"


teardown

rm -rf dbs/

echo "Starting backing mongod..."
mkdir -p dbs/logs
mkdir -p dbs/data
mongod --shardsvr --replSet "shard-replset" --dbpath ./dbs/data --logpath ./dbs/logs/mongod.log --port 27001 --fork


mongosh --eval 'rs.initiate()' "mongodb://localhost:27001"

echo "Starting config server..."
mkdir -p dbs/config_data
mkdir -p dbs/config_logs
mongod --configsvr --replSet "config-replset" --dbpath ./dbs/config_data --logpath ./dbs/config_logs/mongod.log --port 27002 --fork

mongosh --eval 'rs.initiate()' "mongodb://localhost:27002"


echo "Starting mongos"
mkdir -p dbs/mongos_data
mkdir -p dbs/mongos_logs
mongos --configdb "config-replset/localhost:27002" --port 27003 --logpath ./dbs/mongos_logs/mongos.log --fork


mongosh --eval 'sh.addShard( "shard-replset/localhost:27001")' "mongodb://localhost:27003"

./run-genny workload -- run -w "$workload" -u "mongodb://localhost:27003"

./run-genny translate ./build/WorkloadOutput/CedarMetrics/ -o ./build/WorkloadOutput/CedarMetrics/out.ftdc

t2 ./build/WorkloadOutput/CedarMetrics/out.ftdc ./dbs/data/diagnostic.data/

teardown
