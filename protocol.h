#pragma once

constexpr int max_name_len = 32;

enum object_desc
{
	human,
	non_human,
	vehicle,
	static_object
};

enum object_form
{
	cube,
	sphere,
	pyramid,
	cone
};

struct coordinate
{
	int x;
	int y;
};

// Message head
enum msg_type
{
	join, // Client joining game at server
	leave, // Client leaving game
	change, // Information to clients
	event, // Information from clients to server
	text_message // Send text messages to one or all
};

// Included first in all messages
struct msg_head
{
	unsigned int length; // Total length for whole message
	unsigned int seq_no; // Sequence number
	unsigned int id; // Client ID or 0;
	msg_type type; // Type of message
};

// Message type Join (Client -> Server)
struct join_msg
{
	msg_head head;
	object_desc desc;
	object_form form;
	char name[max_name_len];
};

// Message type Leave (Client -> Server)
struct leave_msg
{
	msg_head head;
};

// Message type Change (Server -> Client)
enum change_type
{
	new_player,
	player_leave,
	new_player_position
};

// Included first in all Change messages
struct change_msg
{
	msg_head head;
	change_type type;
};

// Variations of ChangeMsg
struct new_player_msg
{
	change_msg msg; //Change message header with new client id
	object_desc desc;
	object_form form;
	char name[max_name_len];
};

struct player_leave_msg
{
	change_msg msg; //Change message header with new client id
};

struct new_player_position_msg
{
	change_msg msg; //Change message header
	coordinate pos; //New object position
	coordinate dir; //New object direction
};

// Messages of type Event (Client -> Server)
enum event_type
{
	move
};

// Included first in all Event messages
struct event_msg
{
	msg_head head;
	event_type type;
};

// Variations of EventMsg
struct move_event
{
	event_msg event;
	coordinate pos; //New object position
	coordinate dir; //New object direction
};

// Messages of type TextMessage
struct text_message_msg
{
	msg_head head;
	char text[1];
};
