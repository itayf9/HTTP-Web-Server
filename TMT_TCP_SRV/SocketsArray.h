#pragma once
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS
#include <winsock2.h>
#include <iostream>
#include <time.h>
#include <sstream>
#include <map>
#include <fstream>
#include <filesystem>

using namespace std;
using std::char_traits;

const int MAX_SOCKETS = 60;
const int EMPTY = 0;
const int LISTEN = 1;
const int RECEIVE = 2;
const int IDLE = 3;
const int SEND = 4;
const int SEND_GET = 1;
const int SEND_HEAD = 2;
const int SEND_POST = 3;
const int SEND_PUT = 4;
const int SEND_DELETE = 5;
const int SEND_TRACE = 6;
const int SEND_OPTIONS = 7;
const int SEND_NOT_IMPLEMENTED = 8;
const int MAX_BUF_LEN = 1024;
const string SERVER_NAME = (string)"ITAY&DANIEL";
const string FR = (string)"-fr";
const string HE = (string)"-he";
const string Not_Found = string("<html><head></head><body><center><h1>404 Not Found</h1></center></body></html>");
const string Not_Allowed = string("<html><head></head><body><center><h1>405 This Method is Not Allowed.</h1></center></body></html>");
const string Bad_Request = string("<html><head></head><body><center><h1>400 Bad Request</h1></center></body></html>");
const string Created_Successfully = string("<html><head></head><body><center><h1>Created Successfully</h1></center></body></html>");
const string Cant_Delete = string("<html><head></head><body><center><h1>Error Couldn't Delete File!</h1></center></body></html>");
const string Successfully_Deleted = string("<html><head></head><body><center><h1>file deleted successfully!!</h1></center></body></html>");
const string Processed_Successfully = string("<html><head></head><body><center><h1>Processed_Successfully</h1></center></body></html>");

struct SocketState
{
	SOCKET id;			// Socket handle
	int	recv;			// Receiving?
	int	send;			// Sending?
	int sendSubType;	// Sending sub-type
	char buffer[MAX_BUF_LEN];
	map<string, string> messageData;
	time_t timerSinceLastByteRecv = 0;
	int len;
	sockaddr_in fromAddress;
};

class SocketsArray
{
private:
	SocketState sockets[MAX_SOCKETS] = { 0 };
	int socketsCount = 0;

	map<int, string> response_codes = {
	{100, (string)"Continue"},
	{101, (string)"Switching Protocols"},
	{200, (string)"OK"},
	{201, (string)"Created"},
	{202, (string)"Accepted"},
	{203, (string)"Non-Authoritative Information"},
	{204, (string)"No Content"},
	{205, (string)"Reset Content"},
	{206, (string)"Partial Content"},
	{300, (string)"Multiple Choices"},
	{301, (string)"Permanent Redirect"},
	{302, (string)"Found"},
	{303, (string)"See Other"},
	{304, (string)"Not Modified"},
	{305, (string)"Use Proxy"},
	{307, (string)"Temporary Redirect"},
	{400, (string)"Bad Request"},
	{401, (string)"Unauthorized"},
	{402, (string)"Payment Required"},
	{403, (string)"Forbidden"},
	{404, (string)"Not Found"},
	{405, (string)"Method Not Allowed"},
	{406, (string)"Not Acceptable"},
	{407, (string)"Proxy Authentication Required"},
	{408, (string)"Request Time-out"},
	{409, (string)"Conflict"},
	{410, (string)"Gone"},
	{411, (string)"Length Required"},
	{412, (string)"Precondition Failed"},
	{413, (string)"Request Entity Too Large"},
	{414, (string)"Request-URI Too Large"},
	{415, (string)"Unsupported Media Type"},
	{416, (string)"Requested range not satisfiable"},
	{417, (string)"Expectation Failed"},
	{500, (string)"Internal Server Error"},
	{501, (string)"Not Implemented"},
	{502, (string)"Bad Gateway"},
	{503, (string)"Service Unavailable"},
	{504, (string)"Gateway Time-out"},
	{505, (string)"HTTP Version not supported"}
	};


public:
	int getSocketCounter() const;
	SocketState* const getSockets();

	bool addSocket(SOCKET id, int what);
	void removeSocket(int index);
	void acceptConnection(int index);
	void receiveMessage(int index);
	void sendMessage(int index);
	void extractDataToMap(stringstream& sstream, int& sizeOfMessage, const int& index);
	void assembleResponseHeader(string& strBuff, const int& code,const int& index, ifstream& requestedFile, int sizeOfBodyData = 0, string headerExtraInfo= "");
	int decodePathToResponseStatus(string& path, ifstream& requestedFile);
	string getAllowedMethods(const string& path);
	bool validQueryParameter(string& queryStr);
	bool validLangParameter(string& langParameter);
	void extractTraceDataToMap(stringstream& sstream, int& sizeOfMessage, const int& index);
	double calcTimePassed(int index);
};