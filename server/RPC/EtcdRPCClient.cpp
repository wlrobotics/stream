#include "EtcdRPCClient.h"

#include <thread>
#include <chrono>

#include "Util/logger.h"
#include "Config.h"

using namespace toolkit;

void EtcdRPCClient::init() {
    auto channel = grpc::CreateChannel(ConfigInfo.service.ectd_endpoint, grpc::InsecureChannelCredentials());
    kv_stub_ = etcdserverpb::KV::NewStub(channel);
    lease_stub_ = etcdserverpb::Lease::NewStub(channel);
}

std::int64_t EtcdRPCClient::lease_grant(int ttl) {
    grpc::ClientContext context;

    etcdserverpb::LeaseGrantRequest req;
    req.set_ttl(ttl);

    etcdserverpb::LeaseGrantResponse resp;
    grpc::Status status = lease_stub_->LeaseGrant(&context, req, &resp);
    if (!status.ok()) {
        ErrorL << "LeaseGrant " 
               << ",grpc_code=" << status.error_code() 
               << ": " << status.error_message();
        return -1;
    }
    
    return resp.id();
}

bool EtcdRPCClient::get(const std::string& key, std::string& value) {
    grpc::ClientContext context;

    etcdserverpb::RangeRequest req;
    req.set_key(key);

    etcdserverpb::RangeResponse resp;
    grpc::Status status = kv_stub_->Range(&context, req, &resp);
    if (!status.ok()) {
        ErrorL << "Get "
               << ",grpc_code=" << status.error_code()
               << ": " << status.error_message() << std::endl;
        return false;
    }

    if (resp.count() == 0) {
        return false;
    }

    value = resp.kvs(0).value();

    return true;
}

bool EtcdRPCClient::put(std::string const & key, std::string const & value, int64_t lease_id) {
    grpc::ClientContext context;

    etcdserverpb::PutRequest req;
    req.set_key(key);
    req.set_value(value);
    req.set_lease(lease_id);

    etcdserverpb::PutResponse resp;
    grpc::Status status = kv_stub_->Put(&context, req, &resp);
    if (!status.ok()) {
        ErrorL << "Put " 
               << ",grpc_code=" << status.error_code() 
               << ": " << status.error_message() << std::endl;
        return false;
    }

    return true;
}

bool EtcdRPCClient::put_if_not_exists(std::string const & key, std::string const & value, std::int64_t lease_id) {
    etcdserverpb::TxnRequest req;
    etcdserverpb::Compare* comp = req.add_compare();
    comp->set_result(etcdserverpb::Compare::CompareResult::Compare_CompareResult_EQUAL);
    comp->set_target(etcdserverpb::Compare::CompareTarget::Compare_CompareTarget_CREATE);
    comp->set_key(key);
    comp->set_version(0);

    etcdserverpb::PutRequest put_req;
    put_req.set_key(key);
    put_req.set_value(value);
    put_req.set_prev_kv(true);
    put_req.set_lease(lease_id);
    etcdserverpb::RequestOp* req_success = req.add_success();
    req_success->set_allocated_request_put(&put_req);
    
    etcdserverpb::RangeRequest get_req;
    get_req.set_key(key);
    etcdserverpb::RequestOp* req_failure = req.add_failure();
    req_failure->set_allocated_request_range(&get_req);

    grpc::ClientContext ctx;
    etcdserverpb::TxnResponse resp;
    grpc::Status status = kv_stub_->Txn(&ctx, req, &resp);
    if (!status.ok()) {
        ErrorL << "Txn,code=" << status.error_code() 
               << ": " << status.error_message();
        return false;
    }

    if(!resp.succeeded()) {
        if (resp.responses(0).response_range().kvs(0).value() != value) {
            return false;
        }
    }

    return true;
}

bool EtcdRPCClient::lease_keep_alive(std::int64_t lease_id) {
    grpc::ClientContext context;

    auto grpc_stream = lease_stub_->LeaseKeepAlive(&context);

    bool ret = false;
    etcdserverpb::LeaseKeepAliveRequest req;
    req.set_id(lease_id);
    ret = grpc_stream->Write(req);
    if(!ret) {
        return false;
    }

    etcdserverpb::LeaseKeepAliveResponse resp;
    ret = grpc_stream->Read(&resp);
    if(!ret) {
        ErrorL << std::hex << lease_id << ",grpc stream read failed!";
        return false;
    }

    if(resp.ttl() <= 0) {
        ErrorL << std::hex << lease_id << ",ttl <= 0, "  << resp.ttl();
        return false;
    }

    return true;
}


bool EtcdRPCClient::leases(std::vector<std::int64_t>& lease_ids) {
    grpc::ClientContext context;

    etcdserverpb::LeaseLeasesRequest req;
    etcdserverpb::LeaseLeasesResponse resp;
    grpc::Status status = lease_stub_->LeaseLeases(&context, req, &resp);
    if (!status.ok()) {
        ErrorL << "leases,code=" << status.error_code() << ": " << status.error_message();
        return false;
    }

    for(int index = 0; index < resp.leases_size(); index++){
        lease_ids.emplace_back(resp.leases(index).id());
    }

    return true;
}

int EtcdRPCClient::check_key_exists(std::string const & key) {
    grpc::ClientContext context;

    etcdserverpb::RangeRequest req;
    req.set_key(key);
    req.set_range_end("\xC0\x80");

    etcdserverpb::RangeResponse resp;
    grpc::Status status = kv_stub_->Range(&context, req, &resp);
    if (!status.ok()) {
        ErrorL << "Range "
               << ",grpc_code=" << status.error_code()
               << ": " << status.error_message() << std::endl;
        return -1;
    }

    return resp.kvs_size();
}