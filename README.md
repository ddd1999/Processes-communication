# Processes-communication

Implementation of a **communication protocol between processes**.  
Requirements:
- the communication is done by executing commands that are read from the keyboard in the parent process and executed in the child process
- commands are strings marked by *new line*
- responses are octet strings prefixed by the length of the response
- the result will be shown by the parent process  

The minimal protocol has the following commands:

- ***"login : username"*** - configuration file
- ***"myfind file"*** - finding a file and showing the information associated with that file (date of creation, date of modification, size of the file, access rights etc)
- **"mystat file"*** - attributes of the file
- ***"quit"***  

The communication between processes is made by using at least once all of the following: *internal pipes*, *external pipes* and *socketpair*.



