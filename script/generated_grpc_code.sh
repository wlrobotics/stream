#!/bin/bash
set -e

grpc_dir='grpc'

if [ -n "$1" ]; then
    grpc_dir='temp/grpc'
else
    grpc_dir='grpc'
fi

# $1=proto_file
generated_grpc_code()
{
./3rdparty/$grpc_dir/bin/protoc -I=./server/RPC/proto --grpc_out=./server/RPC/proto --cpp_out=./server/RPC/proto --plugin=protoc-gen-grpc=./3rdparty/$grpc_dir/bin/grpc_cpp_plugin $1
}

generated_grpc_code event.proto
generated_grpc_code stream.proto
generated_grpc_code ptz.proto
generated_grpc_code flow.proto
generated_grpc_code gb28181.proto

# $1=proto_file
generated_etcd_grpc_code()
{
./3rdparty/$grpc_dir/bin/protoc -I=./server/RPC/proto/etcd --grpc_out=./server/RPC/proto/etcd --cpp_out=./server/RPC/proto/etcd --plugin=protoc-gen-grpc=./3rdparty/$grpc_dir/bin/grpc_cpp_plugin $1
}

generated_etcd_grpc_code gogoproto/gogo.proto
generated_etcd_grpc_code google/api/http.proto
generated_etcd_grpc_code google/api/annotations.proto
generated_etcd_grpc_code auth.proto
generated_etcd_grpc_code kv.proto
generated_etcd_grpc_code rpc.proto
