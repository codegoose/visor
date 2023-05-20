#pragma once  // Ensures this header file is included only once during compilation

#include <map>  // Include the header file for using std::map
#include <atomic>  // Include the header file for using std::atomic
#include <string>  // Include the header file for using std::string

namespace sc::iracing {  // Start of the sc::iracing namespace

    enum class status {  // Declaration of an enumeration named "status" with different connection states
        stopped,  // The connection is stopped
        searching,  // The connection is searching
        connected,  // The connection is connected
        live  // The connection is live
    };

    void startup();  // Declaration of the function "startup()" for starting the iRacing connection
    void shutdown();  // Declaration of the function "shutdown()" for shutting down the iRacing connection

    const status &get_status();  // Declaration of the function "get_status()" for retrieving the current connection status

    std::map<std::string, int> variables();  // Declaration of the function "variables()" for retrieving various connection variables

    const std::atomic<bool> &prev();  // Declaration of the function "prev()" for retrieving the previous value
    const std::atomic<float> &lap_percent();  // Declaration of the function "lap_percent()" for retrieving the lap percentage
    const std::atomic<float> &rpm();  // Declaration of the function "rpm()" for retrieving the RPM value
    const std::atomic<float> &rpm_prev();  // Declaration of the function "rpm_prev()" for retrieving the previous RPM value
    const std::atomic<float> &speed();  // Declaration of the function "speed()" for retrieving the speed value
    const std::atomic<float> &speed_prev();  // Declaration of the function "speed_prev()" for retrieving the previous speed value
    const std::atomic<int> &gear();  // Declaration of the function "gear()" for retrieving the gear value
    const std::atomic<int> &gear_prev();  // Declaration of the function "gear_prev()" for retrieving the previous gear value

}  // End of the sc::iracing namespace

