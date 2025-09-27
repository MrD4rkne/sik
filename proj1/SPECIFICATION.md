# Network Clock Synchronization

Implement a program for synchronizing clocks in a peer-to-peer model. The clock synchronization network consists of equal nodes. Each node communicates with other nodes to synchronize its clock, taking into account the travel time of packets in the network.

The operation of the clock synchronization network includes the following activities:

1. joining a node to the network through contact with another node,
2. selecting a leader to which nodes synchronize their clocks,
3. synchronizing clocks taking into account transmission packet delays.

Each node maintains its natural clock, which is the number of milliseconds since its startup.

The synchronization level of a node is:

- value 255 when the node is not synchronized with any node,
- value 0 when the node is the synchronization source for other nodes (is the leader),
- value 1 when the node is synchronized with the leader,
- value 2 when the node is synchronized with a node that is synchronized with the leader, etc., but at most 254.

Each node stores its synchronization level. The initial value of the node's synchronization level is 255. The node remembers which node it is synchronized with.

Nodes communicate using UDP and IPv4 addressing.

## Node Program Parameters

The program implementing node functionality accepts the following parameters in the command line:

- `-b bind_address` – IP address on which the node listens, optional, by default the node listens on all addresses of the host on which it is running;
- `-p port` – port on which the node listens, number from 0 to 65535, zero means choosing any port, optional, default zero;
- `-a peer_address` – IP address or hostname of another node with which this node should communicate, optional;
- `-r peer_port` – port of another node with which this node should communicate, number from 1 to 65535, optional.

Parameters can be given in any order. Parameters `-a` and `-r` must both be given. The behavior of the program when any parameter is given multiple times should be reasonable.

## Messages Exchanged Between Nodes

The following fields may appear in messages exchanged between nodes:

- `message` – 1 byte, message type;
- `count` – 2 bytes, number of known nodes;
- `peer_address_length` – 1 byte, number of bytes in the `peer_address` field;
- `peer_address` – `peer_address_length` bytes, node IP address in format as in IP header;
- `peer_port` – 2 bytes, port number on which the node listens;
- `timestamp` – 8 bytes, time, clock value;
- `synchronized` – 1 byte, node synchronization level.

Values in binary multi-byte fields are written in network order.

Nodes send messages from the port on which they listen. Nodes obtain information about the sender's IP address and port number from the network layer. The IP address and port number uniquely identify a node.

## Joining the Network

A node can join the network in two ways:

1. if it was started without parameters `-a` and `-r`, it listens to messages and waits for a new participant;
2. if it was started with parameters `-a` and `-r`, it sends a HELLO message to the specified node.

Messages exchanged in this communication stage:

- `HELLO` – message = 1;
- `HELLO_REPLY` – message = 2, count – number of records containing information about nodes known to the responding node, followed by these records, each containing `peer_address_length`, `peer_address`, `peer_port`;
- `CONNECT` – message = 3;
- `ACK_CONNECT` – message = 4.

The HELLO message informs another node about the desire to establish communication with it.

The HELLO_REPLY message is a response to the HELLO message. It informs the new node in the network about other active nodes in the network. The transmitted list does not include the sender or receiver of this message. Based on this list, the new node sends a CONNECT message to each learned node to establish communication with that node.

The CONNECT message informs about the desire to establish communication with another node. The node that received the CONNECT message adds to its node list the node from which it received this message.

The ACK_CONNECT message is a response to the CONNECT message. It confirms the establishment of communication. The node adds to its node list the nodes that confirmed the establishment of communication.

Nodes that exchanged HELLO and HELLO_REPLY messages have established communication and no longer exchange CONNECT and ACK_CONNECT messages.

## Time Synchronization

The clock synchronization process for nodes involves exchanging three messages:

- `SYNC_START` – message = 11, synchronized, timestamp;
- `DELAY_REQUEST` – message = 12;
- `DELAY_RESPONSE` – message = 13, synchronized, timestamp.

The SYNC_START message is sent cyclically by all nodes whose synchronization level is less than 254, to all nodes known to them. This message contains the synchronization level and the current clock value T1 of the node that is the sender of this message.

A node attempts synchronization and responds to SYNC_START only when:

- the sender is known to it;
- the sender has a synchronization level less than 254;
- the sender's synchronization level is less than its synchronization level for the sender being the node with which this node is synchronized;
- the sender's synchronization level is at least two less than its synchronization level for the sender being a node with which this node is not synchronized.

The node continues the synchronization process only with the first node that meets the conditions described above.

If for 20–30 seconds a node does not receive a SYNC_START message from the node with which it is synchronized, or receives such a message from it with a synchronized field value greater than or equal to its own synchronization level value, it changes its synchronization level to 255.

If a node received a SYNC_START message and continues the synchronization process, it records its clock value T2 at the moment of receiving this message, sends back a DELAY_REQUEST message, and records its clock value T3 at the moment of sending it.

The node that started the synchronization process, after receiving the DELAY_REQUEST message, sends back a DELAY_RESPONSE message containing again its synchronization level and clock value T4 at the moment of receiving the DELAY_REQUEST message.

The synchronized node calculates:

```c
offset = (T2 - T1 + T3 - T4) / 2
```

From this moment, the synchronized node is synchronized at a level one greater than the synchronized value received in the SYNC_START and DELAY_RESPONSE messages (the values of this field must be identical in both messages).

Nodes should send SYNC_START messages to all nodes known to them every 5 to 10 seconds. If a node does not receive a DELAY_REQUEST or DELAY_RESPONSE response within 5 to 10 seconds, it stops the synchronization procedure and ignores such messages received after this time.

If the node sending T1 and T4 values has a clock synchronized with another node, it sends the values of this synchronized time.

## Leader Selection

For the node synchronization process to begin, at least one node must become a leader. The following message is used for this:

- `LEADER` – message = 21, synchronized.

If the synchronized field value in the LEADER message is 0, the node becomes a leader (sets its synchronization level to 0) and two seconds after receiving this message, starts sending synchronizing messages to other nodes it knows.

If the synchronized field value in the LEADER message is 255 and the node is a leader, this node stops being a leader (sets its synchronization level to 255) and stops sending synchronizing messages to other nodes, while finishing already started synchronizing message exchanges.

Other values in the synchronized field of this message are incorrect. A message with value 255 in the synchronized field received by a non-leader node is also incorrect.

A node should react to every LEADER message from the beginning of operation without verifying its sender.

## Providing Current Time Information

Each node should provide current time information using messages:

- `GET_TIME` – message = 31;
- `TIME` – message = 32, synchronized, timestamp.

The GET_TIME message is a request to provide the current time.

The TIME message is a response to the GET_TIME message and contains the synchronization level and natural clock value of the responding node if the node is not synchronized with another node, and the value of this clock corrected by the offset value if the node is synchronized with another node.

A node should respond to every GET_TIME message from the beginning of operation without verifying its sender.

## Error Handling

The program should carefully check parameter correctness, and detailed error information should be printed to standard error output, terminating with exit code 1.

Errors related to calling system or library functions should be handled by printing appropriate information to standard error output. If an error prevents further program operation, the program should be terminated with exit code 1. Errors that allow further program operation, e.g., incorrect messages, ignored messages, should not interrupt its operation. A message should be considered incorrect also when the sender is an unknown node (see exceptions described above) or is unexpected in a given communication state, or when it contains incorrect or unexpected value of some field.

Error information printed to standard error output should start with the word ERROR. Information about incorrect messages should start with ERROR MSG, followed by a space and at most 10 initial bytes of this message written in hexadecimal, for example:

```text
ERROR MSG 7a12c534
```

## Additional Explanations

- The size of the count field in the HELLO_REPLY message limits the number of nodes to 65535. It should be assumed that this is the upper limit and messages that would cause this limit to be exceeded should be ignored. If a HELLO_REPLY message is too large to be sent, it should be discarded and treated like other ignored messages.

- A HELLO_REPLY message should be considered entirely erroneous if it does not contain count records, if any of the `peer_address_length` or `peer_port` fields has an incorrect value, if the sender or receiver of this message is in the node list.

## Solution

The solution should be implemented in C or C++, using the socket interface. No other libraries implementing network communication may be used. We expect a single-threaded solution, yet communication with one node should not block communication with other nodes. The program must compile and run in the computer laboratory.

Programs should be written according to best practices. The lack of obvious expectations for programs (e.g., that they won't format the disk, or that values returned by system functions are checked) in the task description does not mean that a program that doesn't meet them will be considered good. Program code will also be subject to evaluation. The protocol will be tested rigorously.

As a solution, submit an archive containing files necessary to build the solution. Binary files and other unnecessary files must not be included. Use zip, rar, 7z, or tar and gzip programs to create the archive. The archive should have the appropriate extension .zip, .rar, .7z, or .tgz respectively. After unpacking, all files should be in the directory where the archive file is. The archive cannot contain subdirectories. The archive should contain a makefile or Makefile file. Executing the make command should create an executable file peer-time-sync. Executing the make clean command should remove all files created during compilation.
