#include "SocketsArray.h"

int SocketsArray::getSocketCounter() const
{
	return socketsCount;
}

SocketState* const SocketsArray::getSockets()
{
	return sockets;
}

bool SocketsArray::addSocket(SOCKET id, int what)
{
	for (int i = 0; i < MAX_SOCKETS; i++)
	{
		if (sockets[i].recv == EMPTY)
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

void SocketsArray::removeSocket(int index)
{
	sockets[index].id = 0;
	sockets[index].recv = EMPTY;
	sockets[index].send = EMPTY;
	socketsCount--;
}

void SocketsArray::acceptConnection(int index)
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
	//
	unsigned long flag = 1;
	if (ioctlsocket(msgSocket, FIONBIO, &flag) != 0)
	{
		cout << "HTTP Server: Error at ioctlsocket(): " << WSAGetLastError() << endl;
	}

	if (addSocket(msgSocket, RECEIVE) == false)
	{
		cout << "\t\tToo many connections, dropped!\n";
		closesocket(id);
	}
	return;
}

void SocketsArray::receiveMessage(int index)
{
	SOCKET msgSocket = sockets[index].id;
	time(&sockets[index].timerSinceLastByteRecv);

	int len = sockets[index].len;
	int bytesRecv = recv(msgSocket, &sockets[index].buffer[len], sizeof(sockets[index].buffer) - len, 0);

	// fix or add:

		// check what happens when recvBuff has no /r/n/r/n (half header is sent)

	// bonus:

		// save 'from' in 'acceptConnection' in order to print the address and port of client disconnecting later

		// convert text answers to html answers - posible, but Not recomended - creates complications..

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
			sstream << sockets[index].buffer;

			string nextLine;
			getline(sstream , nextLine, '\n');

			string option, path, protocol;
			option = strtok(nextLine.data(), " ");
			path = strtok(nullptr, " ");
			protocol = strtok(nullptr, "\n");
			int sizeOfMessage = option.size() + path.size() + protocol.size() + 3;

			if (path[0] == '/' && option != "TRACE")
			{
				path = path.substr(1); // deletes the '/' 
			}
			sockets[index].messageData[(string)"path"] = path;
			
			if (option == "GET")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_GET;

				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "HEAD")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_HEAD;

				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "POST")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_POST;

				extractDataToMap(sstream, sizeOfMessage, index);
				sizeOfMessage += atoi(sockets[index].messageData[(string)"Content-Length:"].c_str());
			}
			else if (option == "PUT")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_PUT;
				
				extractDataToMap(sstream, sizeOfMessage, index);
				sizeOfMessage += atoi(sockets[index].messageData[(string)"Content-Length:"].c_str());
			}
			else if (option == "DELETE")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_DELETE;

				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "TRACE")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_TRACE;

				extractTraceDataToMap(sstream, sizeOfMessage, index);
				//extractDataToMap(sstream, sizeOfMessage, index);
			}
			else if (option == "OPTIONS")
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_OPTIONS;

				extractDataToMap(sstream, sizeOfMessage, index);
			}
			else
			{
				sockets[index].send = SEND;
				sockets[index].sendSubType = SEND_NOT_IMPLEMENTED;
				
				extractDataToMap(sstream, sizeOfMessage, index); // clearing the request buffer
			}

			memcpy(sockets[index].buffer, &sockets[index].buffer[sizeOfMessage], sockets[index].len - sizeOfMessage);
			sockets[index].len -= sizeOfMessage;

			/*
			else if (strncmp(sockets[index].buffer, "Exit", 4) == 0)
			{
				closesocket(msgSocket);
				removeSocket(index);
				return;
			}*/
		}
	}
}

void SocketsArray::sendMessage(int index)
{
	int bytesSent = 0;
	int statusCode;
	char sendBuff[1024];
	string strBuff;
	string message;
	bool hasQueryString = false;

	SOCKET msgSocket = sockets[index].id;
	string path = sockets[index].messageData[(string)"path"];

		ifstream requestedFile(path);

		if (sockets[index].sendSubType == SEND_GET)
		{
			if (sockets[index].messageData.find((string)"Body-Data") != sockets[index].messageData.end())
			{
				statusCode = 400;
			}
			else
			{
				statusCode = decodePathToResponseStatus(path, requestedFile);
			}
			assembleResponseHeader(strBuff, statusCode, index, requestedFile);
			if (statusCode == 200)
			{
				string fileData;
				requestedFile.seekg(0, ios_base::beg);
				while (!requestedFile.eof())
				{
					getline(requestedFile, fileData);
					strBuff += fileData;
					strBuff += "\r\n";
				}
			}
		}
		else if (sockets[index].sendSubType == SEND_HEAD)
		{
			if (sockets[index].messageData.find((string)"Body-Data") != sockets[index].messageData.end())
			{
				statusCode = 400;
			}
			else
			{
				statusCode = decodePathToResponseStatus(path, requestedFile);
			}
			assembleResponseHeader(strBuff, statusCode, index, requestedFile);
		}
		else if (sockets[index].sendSubType == SEND_POST)
		{
			string response("Request Processed Successfully\n");
			cout << endl << sockets[index].messageData[(string)"Body-Data"] << endl; // print to console
			int sizeOfBodyData = response.length(); // calcualte content length
			assembleResponseHeader(strBuff, 200, index, requestedFile, sizeOfBodyData);
			strBuff += response;
		}
		else if (sockets[index].sendSubType == SEND_PUT)
		{
			string message;
			ofstream PutRequestedFile(path);
			if (!PutRequestedFile.is_open()) // the file is read only
			{
				message = "This Method is not Allowed.";
				assembleResponseHeader(strBuff, 405, index, requestedFile, message.size(), getAllowedMethods(path) );
			}
			else
			{
				PutRequestedFile << sockets[index].messageData[(string)"Body-Data"];

				message = "Created Successfully\n";
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
				statusCode = 400;
			}
			else
			{
				statusCode = decodePathToResponseStatus(path, requestedFile);
			}
			if (statusCode == 200)
			{
				requestedFile.close();
				bool isDeleted = std::filesystem::remove(path);

				if (!isDeleted)
				{
					statusCode = 500;
				}
			}
			switch (statusCode)
			{
			case 200:
				messege = "file deleted successfully!!";
				break;
			case 400:
				messege = "Bad Request!";
				break;
			case 404:
				messege = "Requested File Isn't Found!";
				break;
			case 500:
				messege = "Error Couldn't Delete File!";
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

	if (sockets[index].messageData[(string)"Connection"] == "close\r")
	{
		cout << "HTTP Server: Client " << sockets[index].id << " is disconnected\n";
		closesocket(sockets[index].id);
		removeSocket(index);	
	}
}

void SocketsArray::extractDataToMap(stringstream& sstream, int& sizeOfMessage, const int& index)
{
	string nextLine;

	getline(sstream, nextLine, '\n');
	sizeOfMessage += nextLine.size() + 1;
	while (nextLine != "\r")
	{
		string key = strtok(nextLine.data(), " ");
		string data = strtok(nullptr, "\n");
		key = key.substr(0, key.size() - 1);
		sockets[index].messageData[key] = data;

		getline(sstream, nextLine, '\n');
		sizeOfMessage += nextLine.size() + 1;
	}

	if (sockets[index].messageData.find((string)"Content-Length") != sockets[index].messageData.end())
	{
		string bodyData;
		char currCh;

		for (int i = 0; i < atoi(sockets[index].messageData[(string)"Content-Length"].c_str()); i++)
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
		sizeOfMessage += bodyData.size();
	}
}

void SocketsArray::assembleResponseHeader(string& strBuff, const int& code, const int& index, ifstream& requestedFile, int sizeOfBodyData, string headerExtraInfo)
{
	time_t timer; time(&timer);

	strBuff += "HTTP/1.1 " + to_string(code) + " " + response_codes[code] + "\n" + "Date: " + ctime(&timer) + "Server: " + SERVER_NAME + "\n";

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

int SocketsArray::decodePathToResponseStatus(string& path, ifstream& requestedFile)
{
	bool hasQueryString = false;
	int indexQuestionMark = path.find_first_of('?');
	string langParmeter, queryParameter, defaultPath;

	int indexExtention = path.find_first_of('.');
	string fileNameExtention; 

	if (indexQuestionMark != string::npos) // found query string
	{
		hasQueryString = true;
		string quaryStr = path.substr(indexQuestionMark + 1);
		path = path.substr(0, indexQuestionMark);
		defaultPath = path;
		fileNameExtention = path.substr(indexExtention + 1);

		int indexEqualSign = quaryStr.find_first_of('=');
		if (indexEqualSign == string::npos) // invalid query string
		{
			return 400;
		}

		queryParameter = quaryStr.substr(0, indexEqualSign);
		langParmeter = quaryStr.substr(indexEqualSign + 1);

		if (validQueryParameter(queryParameter) && validLangParameter(langParmeter))
		{
			if (langParmeter != "en")
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
	if (fileNameExtention != "txt" && fileNameExtention != "htm" && fileNameExtention != "html")
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

string SocketsArray::getAllowedMethods(const string& path)
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
		if (requestedFileIn.is_open())
		{
			availableMethods += "GET, HEAD, DELETE ";
			requestedFileIn.close();

			ofstream requestedFileOut(path);
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

bool SocketsArray::validQueryParameter(string& queryStr)
{
	return queryStr == "lang" ? true: false;
}

bool SocketsArray::validLangParameter(string& langParameter)
{
	return (langParameter == "en" || langParameter == "he" || langParameter == "fr") ? true : false;
}

void SocketsArray::extractTraceDataToMap(stringstream& sstream, int& sizeOfMessage, const int& index)
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
	sockets[index].messageData["TRACE"] = traceBuff;
}

double SocketsArray::calcTimePassed(int index)
{
	time_t newMeasure;
	time(&newMeasure);
	double timePassed = difftime(newMeasure, sockets[index].timerSinceLastByteRecv);

	//cout << "time passed : " + to_string(timePassed) + '\n';

	return timePassed;
}