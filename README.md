# Messaging-Platform
This project is a practical assignment for the Operating Systems course, focusing on the development of a topic-based messaging platform using the C language on a UNIX/Linux system.  The main objective is to demonstrate a solid understanding of UNIX system mechanisms such as processes, pipes, signals, and file descriptors.

## ⚙️ Execution

To build the platform files and clean previously generated ones, you can use the following commands:

1. **Clean previously generated files**  
   ```bash
   make clean

2. **Generate the files again**  
   ```bash
   make

## 🚀 Features

### 🖥️ **Server (managed by the manager)**

1. List users currently using the platform
 ```bash
users
 ```
Displays the list of users currently active on the platform.

3. Remove a user
```bash
remove <username>
```
Allows the manager to remove a user from the platform.

5. List topics on the platform
```
topics
```
Displays the names of existing topics and the number of persistent messages in each.

7. List messages of a specific topic
```
show <topic>
```
Displays all persistent messages from the specified topic.

9. Lock a topic
```bash
lock <topic>
```
Blocks new messages from being sent to the specified topic.

11. Unlock a topic
```bash
unlock <topic>
```
Allows messages to be sent again to a previously locked topic.

12. Shut down the platform
```bash
close
```
Shuts down the platform.  
<br>

### 👤 **Client**

1. Get a list of all topics
```bash
topics
```

Displays the names of existing topics and the number of persistent messages in each.

3. Send a message to a specific topic
```bash
msg <topic> <duration> <message>
```
Allows a client to send a message to a given topic. Subscription to the topic is not required to send messages.

5. Subscribe to a topic
```bash
subscribe <topic>
```
Allows a client to subscribe to a specific topic and receive its messages.

7. Unsubscribe from a specific topic
```bash
unsubscribe <topic>
```
Allows a client to unsubscribe from a topic.

9. Exit the platform, terminating the feed process
```bash
exit
```
Allows a client to exit the platform.

##
Developed by Jimena Arnaiz and Iván Estépar for the Operating Systems course (ISEC).
