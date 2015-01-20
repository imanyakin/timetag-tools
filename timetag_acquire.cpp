// vim: set fileencoding=utf-8 noet :

/* timetag-tools - Tools for UMass FPGA timetagger
 *
 * Copyright © 2010 Ben Gamari
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/ .
 *
 * Author: Ben Gamari <bgamari@physics.umass.edu>
 */


#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <libusb.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <queue>

#include <zmq.hpp>

#include "timetagger.h"
#include "record_format.h"

#define VENDOR_ID 0x04b4
#define PRODUCT_ID 0x1004

#define MAX_CTRL_MSG_LEN 256

class timetag_acquire {
        struct buffer {
                std::shared_ptr<const uint8_t> buf;
                size_t length;
                off_t offset;
                buffer(std::shared_ptr<const uint8_t> buf, size_t length, off_t offset=0)
                        : buf(buf), length(length), offset(offset) {}
        };

        timetagger t;
        zmq::context_t zmq_ctx;
        zmq::socket_t ctrl_sock;  // used from command loop
        zmq::socket_t data_sock;  // used only from data_callback
        zmq::socket_t event_sock; // used from command loop

        std::string handle_command(std::string line);

public:
        void listen();

        timetag_acquire(libusb_context* ctx, libusb_device_handle* dev)
                : t(ctx, dev, [=](const uint8_t* buffer, size_t length) {
                           this->data_sock.send(buffer, length);
                   }),
                  zmq_ctx(),
                  ctrl_sock(this->zmq_ctx, ZMQ_REP),
                  data_sock(this->zmq_ctx, ZMQ_PUB),
                  event_sock(this->zmq_ctx, ZMQ_PUB)
        {
                this->ctrl_sock.bind("ipc:///tmp/timetag-ctrl");
                this->data_sock.bind("ipc:///tmp/timetag-data");
                this->event_sock.bind("ipc:///tmp/timetag-event");
                std::atomic_thread_fence(std::memory_order_seq_cst);

                struct group *grp = getgrnam("timetag");
                mode_t mode = grp != NULL ? 0660 : 0666;
                chmod("/tmp/timetag-ctrl", mode);
                chmod("/tmp/timetag-data", mode);
                chmod("/tmp/timetag-event", mode);

                t.reset_counter();
                t.start_readout();
        }

        ~timetag_acquire()
        {
                t.stop_readout();
        }
};

void timetag_acquire::listen()
{
        while (true) {
                char buf[MAX_CTRL_MSG_LEN];
                int len = this->ctrl_sock.recv(buf, MAX_CTRL_MSG_LEN);

                if (len > MAX_CTRL_MSG_LEN) {
                        const char error[] = "error: message too long";
                        this->ctrl_sock.send(error, sizeof(error));
                        continue;
                }

                std::string cmd(buf, len);
                std::string response = handle_command(cmd);
                this->ctrl_sock.send(response.c_str(), response.length());
        }
}

/*
 * Return whether to stop
 */
std::string timetag_acquire::handle_command(std::string line)
{
        using boost::lexical_cast;
        std::vector<std::string> tokens;
        std::stringstream response;
        boost::split(tokens, line, boost::is_any_of("\t "));
        if (tokens.size() == 0) {
                return "error: no command";
        }

        struct command {
                std::string name;
                unsigned int n_args;
                std::function<void ()> f;
                std::string description;
                std::string args;
        };
        std::vector<command> commands = {
                {"start_capture", 0,
                        [&]() {
                                t.start_capture();
                                response << "ok";
                                const char event[] = "capture start";
                                event_sock.send(event, sizeof(event));
                        },
                        "Start the timetagging engine"
                },
                {"stop_capture", 0,
                        [&]() {
                                t.stop_capture();
                                response << "ok";
                                const char event[] = "capture stop";
                                event_sock.send(event, sizeof(event));
                        },
                        "Stop the timetagging engine"
                },
                {"capture?", 0,
                        [&]() { response << t.get_capture_en(); },
                        "Return whether the timetagging engine is running"
                },
                {"set_send_window", 1,
                        [&]() {
                                int records = lexical_cast<int>(tokens[1]);
                                t.set_send_window(records);
                                response << "ok";
                        },
                        "Set the USB send window size",
                        "SIZE"
                },
                {"strobe_operate", 2,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                bool enabled = lexical_cast<bool>(tokens[2]);
                                t.set_strobe_operate(channel, enabled);
                                response << "ok";
                        },
                        "Enable/disable a strobe channel",
                        "CHAN ENABLED"
                },
                {"strobe_operate?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_strobe_operate(channel);
                        },
                        "Display operational state of strobe channel",
                        "CHAN"
                },
                {"delta_operate", 2,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                bool enabled = lexical_cast<bool>(tokens[2]);
                                t.set_delta_operate(channel, enabled);
                                response << "ok";
                        },
                        "Enable/disable a strobe channel",
                        "CHAN ENABLED"
                },
                {"delta_operate?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_delta_operate(channel);
                        },
                        "Display operational state of delta channel",
                        "CHAN"
                },
                {"version?", 0,
                        [&]() { response << t.get_version(); },
                        "Display hardware version"
                },
                {"clockrate?", 0,
                        [&]() { response << t.get_clockrate(); },
                        "Display hardware acquisition clockrate"
                },
                {"reset_counter", 0,
                        [&]() { t.reset_counter(); response << "ok"; },
                        "Reset timetag counter"
                },
                {"record_count?", 0,
                        [&]() { response << t.get_record_count(); },
                        "Display current record count"
                },
                {"lost_record_count?", 0,
                        [&]() { response << t.get_lost_record_count(); },
                        "Display current lost record count"
                },
                {"seq_clockrate?", 0,
                        [&]() { response << t.get_seq_clockrate(); },
                        "Display sequencer clockrate"
                },
                {"seq_operate", 1,
                        [&]() {
                                bool enabled = lexical_cast<bool>(tokens[1]);
                                t.set_global_sequencer_operate(enabled);
                                response << "ok";
                                if (enabled) {
                                        const char event[] = "seq start";
                                        event_sock.send(event, sizeof(event));
                                } else {
                                        const char event[] = "seq stop";
                                        event_sock.send(event, sizeof(event));
                                }
                        },
                        "Enable/disable sequencer",
                        "CHAN"
                },
                {"seq_operate?", 0,
                        [&]() {response << t.get_global_sequencer_operate();},
                        "Get operational state of sequencer",
                },
                {"reset_seq", 0,
                        [&]() { t.reset_sequencer(); response << "ok"; },
                        "Return sequencer outputs to initial states"
                },
                {"seqchan_operate", 2,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                bool enabled = lexical_cast<bool>(tokens[2]);
                                t.set_seqchan_operate(channel, enabled);
                                response << "ok";
                        },
                        "Enable/disable sequencer channel",
                        "CHAN ENABLED"
                },
                {"seqchan_operate?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_seqchan_operate(channel);
                        },
                        "Get operational state of sequencer channel",
                        "CHAN"
                },
                {"seqchan_config", 5,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                bool initial_state = lexical_cast<bool>(tokens[2]);
                                int initial_count = lexical_cast<int>(tokens[3]);
                                int low_count = lexical_cast<int>(tokens[4]);
                                int high_count = lexical_cast<int>(tokens[5]);
                                t.set_seqchan_initial_state(channel, initial_state);
                                t.set_seqchan_initial_count(channel, initial_count);
                                t.set_seqchan_low_count(channel, low_count);
                                t.set_seqchan_high_count(channel, high_count);
                                response << "ok";
                        },
                        "Configure sequencer channel",
                        "CHAN INITIAL_STATE INITIAL_COUNT LOW_COUNT HIGH_COUNT"
                },
                {"seqchan_initial_state?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_seqchan_initial_state(channel);
                        },
                        "Get initial state of sequencer channel",
                        "CHAN"
                },
                {"seqchan_initial_count?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_seqchan_initial_count(channel);
                        },
                        "Get initial count of sequencer channel",
                        "CHAN"
                },
                {"seqchan_low_count?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_seqchan_low_count(channel);
                        },
                        "Get low count of sequencer channel",
                        "CHAN"
                },
                {"seqchan_high_count?", 1,
                        [&]() {
                                int channel = lexical_cast<int>(tokens[1]);
                                response << t.get_seqchan_high_count(channel);
                        },
                        "Get high count of sequencer channel",
                        "CHAN"
                },
        };

        std::string cmd = tokens[0];
        if (cmd == "help") {
                for (auto c=commands.begin(); c != commands.end(); c++) {
                        response << c->name << "\t"
                                 << c->args << "\t"
                                 << c->description << std::endl;
                }
                return response.str();
        }

        for (auto c=commands.begin(); c != commands.end(); c++) {
                if (c->name == cmd) {
                        if (tokens.size() != c->n_args+1) {
                                response << "error: invalid command (expects "
                                         << c->n_args << " arguments)";
                                return response.str();
                        }
                        c->f();
                        return response.str();
                }
        }

        return "error: unknown command";
}

static void daemonize()
{
        pid_t pid, sid;

        if (getppid() == 1) return;
        pid = fork();
        if (pid < 0)
                exit(EXIT_FAILURE);
        else if (pid > 0)
                exit(EXIT_SUCCESS);

        umask(0);

        sid = setsid();
        if (sid < 0) {
                fprintf(log_file, "Failed to detach from parent\n");
                exit(EXIT_FAILURE);
        }
}

static void print_usage()
{
        printf("usage: timetag_acquire -s [SOCKET] -d -h\n");
        printf(" ");
        printf("arguments:\n");
        printf("  -s [SOCKET]    Listen on the given UNIX domain control socket\n");
        printf("  -d             Daemonize\n");
        printf("  -h             Display help message\n");
}

int main(int argc, char** argv)
{
        libusb_context* ctx;
        libusb_device_handle* dev;

        bool daemon = false;
        int c;

        while ((c = getopt(argc, argv, "l:dh")) != -1) {
                switch (c) {
                case 'l':
                        log_file = fopen(optarg, "w");
                        break;
                case 'd':
                        daemon = true;
                        break;
                case 'h':
                        print_usage();
                        exit(0);
                default:
                        printf("Unknown option %c\n", optopt);
                        print_usage();
                }
        }

        // Daemonize
        if (daemon) daemonize();

        // Handle SIGPIPE somewhat reasonably
        signal(SIGPIPE, SIG_IGN);

        // Disable output buffering
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(log_file, NULL, _IONBF, 0);

        // Renice ourselves
        if (setpriority(PRIO_PROCESS, 0, -10))
                fprintf(log_file, "Warning: Priority elevation failed.\n");

        libusb_init(&ctx);
        dev = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
        if (!dev) {
                fprintf(log_file, "Failed to open device.\n");
                exit(1);
        }

        struct group *grp = getgrnam("timetag");
        if (grp != NULL) {
                int res = setegid(grp->gr_gid);
                if (res)
                        fprintf(log_file, "Failed to set effective group: %d\n", res);
        } else {
                fprintf(log_file, "Couldn't find timetag group. Running in root group.\n");
        }

        struct passwd *pw = getpwnam("timetag");
        if (pw != NULL) {
                int res = seteuid(pw->pw_uid);
                if (res)
                        fprintf(log_file, "Failed to set effective group: %d\n", res);
        } else {
                fprintf(log_file, "Couldn't find timetag user. Running as root.\n");
        }

        timetag_acquire ta(ctx, dev);
        ta.listen();

        libusb_close(dev);
        libusb_exit(ctx);
        return 0;
}
