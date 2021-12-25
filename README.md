# 107ACN-Chat-Room
2018 Advanced Computer Networks - Homework 3  
Use TCP Socket to implement a chat room on the mininet.

## Server:
Usage: *./server &lt;port number>*

    1. Use multi-thread to handle requests from clients.
    2. List all the members and chat room online, client can choose which room to join.
    3. Handle clients request:
        * Send messages to the member who in the same group.
        * Decide person or group to receive the messages.
        
## Client:
Usage: *./client &lt;Server IP> &lt;Port number>*

Connect to server.  
Handle input:  

    <Message>  
    Send the messages to the group

    /W <Name or room> <Message>  
    Decide person or group to receive the messages

    Bye  
    Disconnection
