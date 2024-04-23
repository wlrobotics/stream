#pragma once

#include "grpcpp/grpcpp.h"
#include "proto/etcd/kv.pb.h"
#include "proto/etcd/kv.grpc.pb.h"
#include "proto/etcd/rpc.pb.h"
#include "proto/etcd/rpc.grpc.pb.h"

class EtcdRPCClient {
public:
    EtcdRPCClient() = default;
    ~EtcdRPCClient() = default;
    void init();
    std::int64_t lease_grant(int ttl);
    bool get(const std::string& key, std::string& value);
    bool put(std::string const & key, std::string const & value, std::int64_t lease_id);
    bool lease_keep_alive(std::int64_t lease_id);
    bool put_if_not_exists(std::string const & key, std::string const & value, std::int64_t lease_id);
    bool leases(std::vector<std::int64_t>& lease_ids);
    int check_key_exists(std::string const & key);
private:
    std::unique_ptr<etcdserverpb::KV::Stub> kv_stub_;
    std::unique_ptr<etcdserverpb::Lease::Stub> lease_stub_;
};