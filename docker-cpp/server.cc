#include <zmq.hpp>

#include <cassert>

int main()
{
    zmq::context_t ctx;
    zmq::socket_t socket {ctx, zmq::socket_type::rep};
    socket.bind("tcp://*:9999");
    zmq::message_t msg;
    const auto recv_result = socket.recv(msg, zmq::recv_flags::none);
    assert(recv_result);
    const auto send_result = socket.send(msg, zmq::send_flags::none);
    assert(send_result && *recv_result == *send_result);
    return 0;
}
