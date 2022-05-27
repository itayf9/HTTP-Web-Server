#include "SocketsArray.h"

int SocketsArray::getSocketCounter() const // returns the 'socketsCounter'
{
	return socketsCount;
}
/*----------------------------------------------------------------*/
SocketState* const SocketsArray::getSockets() // returns the 'sockets' array 
{
	return sockets;
}
/*----------------------------------------------------------------*/
bool SocketsArray::addSocket(SOCKET id, int what) // adds a new socket
{
	// runs through the sockets array, tries to find an empty space for a new socket
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY) // finds an empty space
		{
			sockets[i].id = id;
			sockets[i].recv = what;
			sockets[i].send = IDLE;
			sockets[i].len = 0;
			time(&sockets[i].timerSinceLastByteRecv);
			socketsCount++;
			return (true);
		}
	}
	return (false);
}
/*----------------------------------------------------------------*/
void SocketsArray::removeSocket(int index) // removes a socket at 'index'
{
	sockets[index].id = 0;
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}
/*----------------------------------------------------------------*/
void SocketsArray::acceptConnection(int index) // accepts a new connection
{
	SOCKET id = sockets[index].id;
	struct sockaddr_in from;		// Address of sending partner
	int fromLen = sizeof(from);

	SOCKET msgSocket = accept(id, (struct sockaddr*)&from, &fromLen);
	if (INVALID_SOCKET == msgSocket)
	{
		cout << "HTTP Server: Error at accept(): " << WSAGetLastError() << endl;
		return;
	}
	cout << "HTTP Server: Client " << inet_ntoa(from.sin_addr) << ":" << ntohs(from.sin_port) << " is connected." << msgSocket << endl;

	// Set the socket to be in non-blocking mode.
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	// tries to add the socket
	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}
/*----------------------------------------------------------------*/
void SocketsArray::receiveMessage(int index) // recieves a new messege from socket at 'index'
{
	SOCKET msgSocket = sockets[index].id;
	time(&sockets[index].timerSinceLastByteRecv);

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	if (SOCKET_ERROR == bytesRecv)
	{
		cout << "HTTP Server: Error at recv(): " << WSAGetLastError() << endl;
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}

	if (bytesRecv == 0)
	{
		closesocket(msgSocket);
		removeSocket(index);
		return;
	}
	else
	{
		sockets[index].buffer[len + bytesRecv] = '\0'; //add the null-terminating to make it a string
		cout << "HTTP Server: Recieved: " << bytesRecv << " bytes of \n\n\"" << &sockets[index].buffer[len] << "\" message.\n\n";

		sockets[index].len += bytesRecv;

		if (sockets[index].len > 0)
		{
			stringstream sstream;
			sstream << sockets[index].buffer; // inserts the received buffer into a stringstream

			string nextLine;
			getline(sstream, nextLine, '\n'); // gets the first line of the HTTP request

			// divides the first line into method, path and protocol
			string option, path, protocol;
			option = strtok(nextLine.data(), " ");
			path = strtok(nullptr, " ");
			protocol = strtok(nullptr, "\n");
			int sizeOfMessage = option.size() + path.size() + protocol.size() + 3;

			// deletes the '/' from the path, except in case of a TRACE request
			if (path[0] == '/' && option != "TRACE")
			{
				path = path.substr(1);
			}
			sockets[index].messageData[(string)"path"] = path;

			// indicates that the socket is ready to send data
			sockets[index].send = SEND;

			// indicates the type of the method
			// adds all the header data to the map
			if (option == "GET")
			{
				sockets[index].sendSubType = SEND_GET;
				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "HEAD")
			{
				sockets[index].sendSubType = SEND_HEAD;
				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "POST")
			{
				sockets[index].sendSubType = SEND_POST;
				extractDataToMap(sstream, sizeOfMessage, index);
				sizeOfMessage += atoi(sockets[index].messageData[(string)"Content-Length:"].c_str());
			}
			else if (option == "PUT")
			{
				sockets[index].sendSubType = SEND_PUT;
				extractDataToMap(sstream, sizeOfMessage, index);
				sizeOfMessage += atoi(sockets[index].messageData[(string)"Content-Length:"].c_str());
			}
			else if (option == "DELETE")
			{
				sockets[index].sendSubType = SEND_DELETE;
				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "TRACE")
			{
				sockets[index].sendSubType = SEND_TRACE;
				extractTraceDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "OPTIONS")
			{
				sockets[index].sendSubType = SEND_OPTIONS;
				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else
			{
				sockets[index].sendSubType = SEND_NOT_IMPLEMENTED;
				extractDataToMap(sstream, sizeOfMessage, index); // clearing the request buffer
			}

			memcpy(sockets[index].buffer, &sockets[index].buffer[sizeOfMessage], sockets[index].len - sizeOfMessage);
			sockets[index].len -= sizeOfMessage;
		}
	}
}
/*----------------------------------------------------------------*/
void SocketsArray::sendMessage(int index) // send a response messege to socket at 'index' 
{
	int bytesSent = 0;
	int statusCode;
	char sendBuff[1024];
	string strBuff;
	string message;
	bool hasQueryString = false;

	SOCKET msgSocket = sockets[index].id;
	string path = sockets[index].messageData[(string)"path"];

	// tries to open the file with the given path
	ifstream requestedFile(path);

	// prepares a HTTP response, considering the type of the method
	if (sockets[index].sendSubType == SEND_GET)
	{
		if (sockets[index].messageData.find((string)"Body-Data") != sockets[index].messageData.end())
		{
			// if the GET messege has body (bad request)
			statusCode = 400;
		}
		else
		{
			// selects the right status code, considering the file path and query strings
			statusCode = decodePathToResponseStatus(path, requestedFile);
		}

		// assembles the header of the response
		assembleResponseHeader(strBuff, statusCode, index, requestedFile);

		// adds the matching body data, considering the status code
		if (statusCode == 200)
		{
			string fileData;
			requestedFile.seekg(0, ios_base::beg);
			while (!requestedFile.eof())
			{
				getline(requestedFile, fileData);
				strBuff += fileData + "\r\n";
			}
		}
		else if (statusCode == 404)
		{
			strBuff += Not_Found + "\r\n";
		}
	}
	else if (sockets[index].sendSubType == SEND_HEAD)
	{
		if (sockets[index].messageData.find((string)"Body-Data") != sockets[index].messageData.end())
		{
			// if the HEAD messege has body (bad request)
			statusCode = 400;
		}
		else
		{
			// selects the right status code, considering the file path and query strings
			statusCode = decodePathToResponseStatus(path, requestedFile);
		}

		// assembles the header of the response
		assembleResponseHeader(strBuff, statusCode, index, requestedFile);
	}
	else if (sockets[index].sendSubType == SEND_POST)
	{
		cout << endl << sockets[index].messageData[(string)"Body-Data"] << endl; // print to console
		int sizeOfBodyData = Processed_Successfully.length(); // calcualte content length
		assembleResponseHeader(strBuff, 200, index, requestedFile, sizeOfBodyData);
		strBuff += Processed_Successfully;
	}
	else if (sockets[index].sendSubType == SEND_PUT)
	{
		string message;
		ofstream PutRequestedFile(path);
		if (!PutRequestedFile.is_open()) // the file is read only
		{
			message = Not_Allowed + '\n';
			assembleResponseHeader(strBuff, 405, index, requestedFile, message.length(), getAllowedMethods(path));
		}
		else
		{
			PutRequestedFile << sockets[index].messageData[(string)"Body-Data"];

			message = Created_Successfully + '\n';
			assembleResponseHeader(strBuff, 201, index, requestedFile, message.size());
		}
		strBuff += message;
		PutRequestedFile.close();

	}
	else if (sockets[index].sendSubType == SEND_DELETE)
	{
		int sizeOfBodyData;
		string messege;

		if (sockets[index].messageData.find((string)"Body-Data") != sockets[index].messageData.end())
		{
			// if the DELETE messege has body (bad request)
			statusCode = 400;
			requestedFile.close();
		}
		else
		{
			// selects the right status code, considering the file path and query strings
			statusCode = decodePathToResponseStatus(path, requestedFile);
		}

		if (statusCode == 200)
		{
			// tries to delete the requested file
			requestedFile.close();
			bool isDeleted = std::filesystem::remove(path);

			if (!isDeleted)
			{
				// could not delete
				statusCode = 500;
			}
		}

		// selects the body data, based on the status code
		switch (statusCode)
		{
		case 200:
			messege = Successfully_Deleted;
			break;
		case 400:
			messege = Bad_Request;
			break;
		case 404:
			messege = Not_Found;
			break;
		case 500:
			messege = Cant_Delete;
			break;
		default:
			break;
		}
		sizeOfBodyData = messege.length();
		assembleResponseHeader(strBuff, statusCode, index, requestedFile, sizeOfBodyData);
		strBuff += messege;
	}
	else if (sockets[index].sendSubType == SEND_TRACE)
	{
		string TRACEbody;
		TRACEbody += "TRACE " + sockets[index].messageData[(string)"path"] + " HTTP/1.1\r\n";
		TRACEbody += sockets[index].messageData[(string)"TRACE"];

		assembleResponseHeader(strBuff, 200, index, requestedFile, TRACEbody.size());
		strBuff += TRACEbody;
	}
	else if (sockets[index].sendSubType == SEND_OPTIONS)
	{
		string availableMethods = getAllowedMethods(path);
		assembleResponseHeader(strBuff, 200, index, requestedFile, 0, availableMethods);
	}
	else if (sockets[index].sendSubType == SEND_NOT_IMPLEMENTED)
	{
		assembleResponseHeader(strBuff, 501, index, requestedFile);
	}

	requestedFile.close();

	strcpy(sendBuff, strBuff.c_str());

	bytesSent = send(msgSocket, sendBuff, (int)strlen(sendBuff), 0);
	if (SOCKET_ERROR == bytesSent)
	{
		cout << "HTTP Server: Error at send(): " << WSAGetLastError() << endl;
		return;
	}

	cout << "HTTP Server: Sent: " << bytesSent << "\\" << strlen(sendBuff) << " bytes of \n\n\"" << sendBuff << "\" message.\n\n";

	if (sockets[index].len == 0)
	{
		sockets[index].send = IDLE;
	}

	// closes the connection after sending the response, in the connection header is set to 'close'
	if (sockets[index].messageData[(string)"Connection"] == "close\r")
	{
		cout << "HTTP Server: Client " << sockets[index].id << " is disconnected\n";
		closesocket(sockets[index].id);
		removeSocket(index);
	}

	sockets[index].messageData.clear();
}
/*----------------------------------------------------------------*/
void SocketsArray::extractDataToMap(stringstream& sstream, int& sizeOfMessage, const int& index) // extracts the data from the messege to a map
{
	string nextLine;

	getline(sstream, nextLine, '\n');
	sizeOfMessage += nextLine.size() + 1;
	while (nextLine != "\r") // reading the request data
	{
		string key = strtok(nextLine.data(), " ");
		string data = strtok(nullptr, "\n");
		key = key.substr(0, key.size() - 1); // remove the ':' from the key 
		sockets[index].messageData[key] = data; // add the data value to the dictionary

		getline(sstream, nextLine, '\n');
		sizeOfMessage += nextLine.size() + 1; // calculating size of request messege
	}

	if (sockets[index].messageData.find((string)"Content-Length") != sockets[index].messageData.end()) // check if request contains body data
	{
		string bodyData;
		char currCh;

		for (int i = 0; i < atoi(sockets[index].messageData[(string)"Content-Length"].c_str()); i++) // extract the body data to dictionary 
		{
			sstream.get(currCh);
			if (sstream.eof())
			{
				sockets[index].send = IDLE;
				break;
			}
			bodyData += currCh;
		}
		sockets[index].messageData[(string)"Body-Data"] = bodyData;
		sizeOfMessage += bodyData.size(); // update size of messege
	}
}
/*----------------------------------------------------------------*/
void SocketsArray::assembleResponseHeader(string& strBuff, const int& code, const int& index, ifstream& requestedFile, int sizeOfBodyData, string headerExtraInfo) // assembles a response header
{
	time_t timer; time(&timer);

	strBuff += "HTTP/1.1 " + to_string(code) + " " + response_codes[code] + "\n" + "Date: " + ctime(&timer) + "Server: " + SERVER_NAME + "\n"; // server constant response 

	if (sockets[index].sendSubType == SEND_GET || sockets[index].sendSubType == SEND_HEAD)
	{
		int fileSize;
		switch (code)
		{
		case 200: // 200 OK
			requestedFile.seekg(0, ios_base::end);
			fileSize = requestedFile.tellg();
			strBuff += "Content-Length: " + to_string(fileSize) + "\n"
				+ "Content-Type: text/html; charset=utf-8\n";
			break;
		case 404:
			strBuff += "Content-Length: " + to_string(Not_Found.length()) + "\n"
				+ "Content-Type: text/html; charset=utf-8\n";
			break;
		}
	}
	else if (sockets[index].sendSubType == SEND_POST)
	{
		if (code == 200)
		{
			strBuff += "Content-Length: " + to_string(sizeOfBodyData) + "\nContent-Type: text/html; charset=utf-8\n";
		}
	}
	else if (sockets[index].sendSubType == SEND_PUT)
	{
		if (code == 405)
		{
			strBuff += "Allow: " + headerExtraInfo + "\n";
		}
		strBuff += "Content-Length: " + to_string(sizeOfBodyData) + "\nContent-Type: text/html;charset=utf-8\n";
	}
	else if (sockets[index].sendSubType == SEND_DELETE)
	{
		strBuff += "Content-Length: " + to_string(sizeOfBodyData) + "\nContent-Type: text/html;charset=utf-8\n";
	}
	else if (sockets[index].sendSubType == SEND_TRACE)
	{
		strBuff += "Content-Length: " + to_string(sizeOfBodyData) + "\n"
			+ "Content-Type: HTTP/1.1\n";
	}
	else if (sockets[index].sendSubType == SEND_OPTIONS)
	{
		strBuff += "Allow: " + headerExtraInfo + "\n";
	}
	else if (sockets[index].sendSubType == SEND_NOT_IMPLEMENTED)
	{
		strBuff += "Allow: GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS\n";
	}
	strBuff += "Connection: " + sockets[index].messageData[(string)"Connection"] + "\n";
	strBuff += "\n";
}
/*----------------------------------------------------------------*/
int SocketsArray::decodePathToResponseStatus(string& path, ifstream& requestedFile) // selects a matching response status
{
	bool hasQueryString = false;
	int indexQuestionMark = path.find_first_of('?'); // find question mark character
	string langParmeter, queryParameter, defaultPath;

	int indexExtention = path.find_first_of('.'); // find dot character
	string fileNameExtention; 

	if (indexQuestionMark != string::npos) //  found query string
	{
		hasQueryString = true;
		string quaryStr = path.substr(indexQuestionMark + 1); // extract the quaryStr to variable
		path = path.substr(0, indexQuestionMark); // update path 
		defaultPath = path;
		fileNameExtention = path.substr(indexExtention + 1); // extract file name extension to variable

		int indexEqualSign = quaryStr.find_first_of('='); // find EqualSign character
		if (indexEqualSign == string::npos) // invalid query string
		{
			return 400;
		}

		queryParameter = quaryStr.substr(0, indexEqualSign);
		langParmeter = quaryStr.substr(indexEqualSign + 1);

		if (validQueryParameter(queryParameter) && validLangParameter(langParmeter)) // all good 
		{
			if (langParmeter != "en") // fixing the path value to the requested language 
			{
				path = path.substr(0, path.length() - (fileNameExtention.length() +1)) + "-" + langParmeter + '.' + fileNameExtention;
			}
		}
		else // unknown language or unknown quary parameter
		{
			return 400;
		}

		requestedFile.close();
		requestedFile.open(path);
	}
	indexExtention = path.find_first_of('.');
	fileNameExtention = path.substr(indexExtention + 1);
	if (fileNameExtention != "txt" && fileNameExtention != "htm" && fileNameExtention != "html") // check valid file extension
	{
		return 500;
	}
	
	if (hasQueryString && !requestedFile.is_open()) // has query string and did not find the requested language
	{
		if (path == defaultPath) // lang=en
		{
			// file not found
			return 404;
		}
		else // lang=fr or lang=he
		{
			requestedFile.close();
			requestedFile.open(defaultPath);
			if (!requestedFile)
			{
				// file not found
				return 404;
			}
			return 200;
		}
	}
	else if (hasQueryString && requestedFile.is_open()) // has query string and did find the requsted language
	{
		return 200;
	}
	else if (!requestedFile.is_open()) // has no query string and did find the requsted file
	{
		// file not found
		return 404;
	}
	else  // has no query string and did find the requsted file
	{
		return 200;
	}
}
/*----------------------------------------------------------------*/
string SocketsArray::getAllowedMethods(const string& path) // returns all the allowed methods that can be operated on the file in 'path'
{
	string availableMethods;
	if (path == "*")
	{
		// answer about all the server
		availableMethods += "GET, HEAD, DELETE, TRACE, POST, PUT, OPTIONS";
	}
	else
	{
		ifstream requestedFileIn(path);
		if (requestedFileIn.is_open()) // file is readable
		{
			availableMethods += "GET, HEAD, DELETE "; 
			requestedFileIn.close();

			ofstream requestedFileOut(path); // file is wirteable
			if (requestedFileOut.is_open())
			{
				availableMethods += "POST, PUT ";
			}
			requestedFileOut.close();
		}
		else // file not exists
		{
			availableMethods += "PUT ";
		}



		availableMethods += "TRACE, OPTIONS";

	}

	return availableMethods;
}
/*----------------------------------------------------------------*/
bool SocketsArray::validQueryParameter(string& queryStr) // checks the validation of the query parameters
{
	return queryStr == "lang" ? true: false; 
}
/*----------------------------------------------------------------*/
bool SocketsArray::validLangParameter(string& langParameter)  // checks the validation of the lang parameter
{
	return (langParameter == "en" || langParameter == "he" || langParameter == "fr") ? true : false;
}
/*----------------------------------------------------------------*/
void SocketsArray::extractTraceDataToMap(stringstream& sstream, int& sizeOfMessage, const int& index) // extracts the data from the messege to a map, when the methos is TRACE
{
	string nextLine;
	string traceBuff;
	
	getline(sstream, nextLine, '\n');
	traceBuff += nextLine + '\n';
	sizeOfMessage += nextLine.size() + 1;
	while (nextLine != "\r")
	{
		getline(sstream, nextLine, '\n');
		sizeOfMessage += nextLine.size() + 1;
		traceBuff += nextLine + '\n';
	}
	sockets[index].messageData["TRACE"] = traceBuff; // copy all the trace messege to messageData
}
/*----------------------------------------------------------------*/
double SocketsArray::calcTimePassed(int index) // calculates the diffrence between the last time measure for socket at 'index', and the current time
{
	time_t newMeasure;
	time(&newMeasure);
	double timePassed = difftime(newMeasure, sockets[index].timerSinceLastByteRecv); // get time difference

	return timePassed;
}
