#ifndef __PICLOCK_CONTROL_TCP_H_INCLUDED
#define __PICLOCK_CONTROL_TCP_H_INCLUDED

void create_tcp_threads();

void update_tcp_gpis(uint16_t values);

void handle_faked_tcp_message(const std::string &msg);

#endif
