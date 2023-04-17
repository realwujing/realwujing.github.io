#include <QDebug>
#include <QString>
#include <google/protobuf/util/json_util.h>
#include <iostream>
#include "nodes.pb.h"
using google::protobuf::util::JsonStringToMessage;

bool proto_to_json(const google::protobuf::Message& message, std::string& json) {
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;
    options.preserve_proto_field_names = true;
    return MessageToJsonString(message, &json, options).ok();
}

bool json_to_proto(const std::string& json, google::protobuf::Message& message) {
    return JsonStringToMessage(json, &message).ok();
}

int main() {
    kim::node_info node;
    std::string json_string;

    node.set_name("111111");
    node.set_node_type("34rw343");
    node.set_conf_path("reuwyruiwe");
    node.set_work_path("ewiruwe");
    node.set_worker_cnt(3);

    QString str = QString(QString::fromLocal8Bit(node.name().c_str()));
    qInfo() << "str:" << str;

    node.mutable_addr_info()->set_bind("xxxxxxxxxx");
    node.mutable_addr_info()->set_port(342);
    node.mutable_addr_info()->set_gate_bind("fsduyruwerw");
    node.mutable_addr_info()->set_gate_port(4853);

    /* protobuf 转 json。 */
    if (!proto_to_json(node, json_string)) {
        std::cout << "protobuf convert json failed!" << std::endl;
        return 1;
    }
    std::cout << "protobuf convert json done!" << std::endl
              << json_string << std::endl;

    node.Clear();
    std::cout << "-----" << std::endl;

    /* json 转 protobuf。 */
    if (!json_to_proto(json_string, node)) {
        std::cout << "json to protobuf failed!" << std::endl;
        return 1;
    }
    std::cout << "json to protobuf done!" << std::endl
              << "name: " << node.name() << std::endl
              << "bind: " << node.mutable_addr_info()->bind()
              << std::endl;
    return 0;
}