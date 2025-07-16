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

2. Remove a user
```bash
remove <username>
```
Allows the manager to remove a user from the platform.

3. List topics on the platform
```
topics
```
Displays the names of existing topics and the number of persistent messages in each.

4. List messages of a specific topic
```
show <topic>
```
Displays all persistent messages from the specified topic.

5. Lock a topic
```bash
lock <topic>
```
Blocks new messages from being sent to the specified topic.

6. Unlock a topic
```bash
unlock <topic>
```
Allows messages to be sent again to a previously locked topic.

7. Shut down the platform
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

2. Send a message to a specific topic
```bash
msg <topic> <duration> <message>
```
Allows a client to send a message to a given topic. Subscription to the topic is not required to send messages.

3. Subscribe to a topic
```bash
subscribe <topic>
```
Allows a client to subscribe to a specific topic and receive its messages.

4. Unsubscribe from a specific topic
```bash
unsubscribe <topic>
```
Allows a client to unsubscribe from a topic.

5. Exit the platform, terminating the feed process
```bash
exit
```
Allows a client to exit the platform.

##
Developed by Jimena Arnaiz and Iván Estépar for the Operating Systems course (ISEC).
