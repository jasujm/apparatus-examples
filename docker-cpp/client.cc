#include <zmq.hpp>

#include <cassert>
#include <iostream>

const char message[] {"Hello, world!"};

int main()
{
    zmq::context_t ctx;
    zmq::socket_t socket {ctx, zmq::socket_type::req};
    socket.connect("tcp://server:9999");
    const auto send_result = socket.send(
        zmq::buffer(message, sizeof(message) - 1), zmq::send_flags::none);
    assert(send_result && *send_result == sizeof(message) - 1);
    zmq::message_t msg;
    const auto recv_result = socket.recv(msg, zmq::recv_flags::none);
    assert(recv_result && *recv_result == sizeof(message) - 1);
    std::cerr << "Received: " << msg << "\n";
    return 0;
}
