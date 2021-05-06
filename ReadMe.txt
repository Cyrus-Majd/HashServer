

------=| SOCKET KEY-VALUE PAIR STORAGE SYSTEM |=------

AUTHORS: Cyrus Majd


Program Description: 	
	A server software which allows key value pairs to be stored a machine's memory. By default, key value pairs
	are strings, but this can be easily modified. Has applications in distributed computing where lots of info has to
	be stored in memory.
	
	This is the server code. The idea is that you connect to this server from a client, and send either a GET, SET, or DEL request.
	The appropriate request, along with any parameters, is then processed in a separate thread, one for each client.

	Values are stored in a mutex-locked synchronous queue data structure. The contents of the queue is an array of structs containing
	a key value pair. This is the data structure with which the client interacts.

How to use the program:
	
	- COMPILATION: Compile "echos.c" with the following command: "gcc -g -fsanitize=address echos.c -lpthread -lm -o echos"
		       Alternatively, you could run "make" with the included makefile.
	- EXECUTION: To use the storage system, simply call executable "./echos" and pass in one arguement.
	
How to connect to the program:

	- A telnet or netcat connection to the IP address of the host and at the specified server port is sufficient to open a
	connection. No special access is currently specified, but this can be modified.
	- Once a connection is made, you can send commands.
	- The program handles three commands: "SET", "GET", & "DEL".

		"SET" [length] [key] [value]
			Sets a key-value pair in the synchronous queue structure. If a pair with the same key already exists, the old pair
			is deleted and the new pair takes its place.
		"GET" [length] [key]
			Returns the value at the key in the synchronous queue structure. If the key-value pair does not exist,
			KNF is returned.
		"DEL" [length] [key]
			Deletes the key-value pair specified. If it does not exists, KNF is returned.
	- Every command must be followed by a newline or newline character '\n'. Every parameter must also be separated with this.
	The server will automatically send back a response to your requests in your terminal.
	- To end a connection, press ctrl + C or send in some incorrect input.
	
Error Responses:

	Several things will cause the connection to the server to close. If you misformat your query, "ERR" will be returned, along with
	"BAD", indicating a bad input format. "ERR", "BAD" will also return if you make a misspelling of a command. If your message
	length is also incorrect, "ERR" "LEN" will be returned, indicating that there is an error with the given length. All of these
	responses close the connection to the client and end the process thread the connection was using. A connection will also close
	if the client enters ctrl + C AT ANY TIME.
	
Arguements:

	The program will take one arguement; the port you wish the server to run on. The program will crash if you give it
	any more or less. The specified port must be an integer.
		       
Program structure:

	Once the server software is operational and given arguements are validated, a queue-array synchronous data structure is
	initialized. This will be the main data structure that all clients will interact with. The queue data structure is mutex-locked
	and has an array of key-value pairs, as well as pointers to the head and tail of the array. The program then enters an 
	infinite loop, which constantly looks out for inbound connections at the specified server port. Once a request is recieved,
	a connection begins in a new process thread as the server continues to look for other connection requests. Inside each
	connection thread, commands and parameters are read in one chunk at a time from the socket, and are processed accordingly.
	To "SET" a key-value pair, a method is called which immediately goes to the end of the synchronous queue structure's array
	of key-value pair structs and appends the new pair to the end. The efficiency of this is O(1), thanks to our pointers. To "GET"
	a value from a key, the program first checks if the pair exists. If it does, it does an O(n) scan to find the value and return
	it, but if the pair does not exists, error code "KNF" for "key not found" is returned. The "DEL" command also first checks if
	the pair exists, and returns "KNF" if it doesnt. If it does exist, the value is returned to the user, but the pair is then
	deleted. This also takes O(n) time. The program thread terminates when the user exits their connection, and the main thread
	can terminate when hitting ctrl + C for the server program. 
	
Test cases:

	- Program handles incorrect lengths for messages
	- Program handles strange characters as key-value pairs
	- Program handles incorrect commands
	- Program handles multiple connections running sequentially
	- Program handles requests being sent to the queue at the same time from different clients.


		       
