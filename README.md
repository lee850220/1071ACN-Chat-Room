# 107ACN-Chat-Room
Use TCP Socket to implement a chat room on the mininet. 

**Server**  
**Usage:** ```./server <port number>```  

**Client**  
**Usage:** ```./client <Server IP> <Port number>```

- - -

Handle input:  
```<Message>```  
Send the messages to the group.

```/W <Name or room> <Message>```  
Decide person or group to receive the messages.

```Bye```  
Disconnection

- - -

Environment:  
You can practice construct 4 host on the mininet and they can "ping" each other.

Other example to view [2018 Advanced Computer Networks Homework 3.pdf](https://github.com/lee850220/1071-NSYSU_Advanced_Computer_Network/blob/master/HW3/2018%20Advanced%20Computer%20Networks%20Homework%203.pdf) in detail. 

- - -

## ChangeLog
Ver. 1.0  
Multiple Client with fixed number.  
Allow username or roomname include whitespace.  
Change specified mode command.

Ver. 1.1  
Change SMI Structure, add Chatroom structure.

Ver. 1.2  
Change SMI Structure, add User_Fd structure.

Ver. 1.3  
Add Server debugging mode.

Ver. 1.4  
Add colorful output for client and server.

Ver. 1.5  
Dynamic allocate resource (#chatroom, #user)

Ver. 1.6  
Fix input words will diminish when get some message from server on typing.  
Fix function for "backspace" button

Ver. 1.7  
Fix client over 5 will crash. (dynamic allocate problem)  
Add function for "up, down, left, right" button

- - -

## Sample Output
![](https://github.com/lee850220/107ACN-Chat-Room/blob/main/2018-10-10_053434.PNG)

- - -

_Known BUG_
```
FD_SETSIZE = 1024 (fixed in linux) (may be solve in future)   
Other control function will not work (no function or may cause some bugs), due to self implement output control in ver. 1.6)
